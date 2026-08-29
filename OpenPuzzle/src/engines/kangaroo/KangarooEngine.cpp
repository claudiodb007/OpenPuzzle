#include "openpuzzle/engines/kangaroo/KangarooEngine.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace openpuzzle {

namespace {

using HexValue = std::vector<unsigned int>;

std::string shellQuote(const std::string &value) {
  std::string quoted = "'";

  for (const char character : value) {
    if (character == '\'')
      quoted += "'\\''";
    else
      quoted += character;
  }

  quoted += "'";
  return quoted;
}

int hexDigit(const char character) {
  if (character >= '0' && character <= '9')
    return character - '0';

  if (character >= 'a' && character <= 'f')
    return character - 'a' + 10;

  if (character >= 'A' && character <= 'F')
    return character - 'A' + 10;

  return -1;
}

HexValue parseHex(const std::string &value, const char *label) {
  if (value.empty())
    throw std::invalid_argument(std::string(label) + " is empty");

  HexValue result;
  result.reserve(value.size());
  for (auto position = value.rbegin(); position != value.rend(); ++position) {
    const char character = *position;
    const int digit = hexDigit(character);
    if (digit < 0)
      throw std::invalid_argument(std::string(label) + " is not hexadecimal");
    result.push_back(static_cast<unsigned int>(digit));
  }

  while (result.size() > 1 && result.back() == 0)
    result.pop_back();

  return result;
}

int compareHex(const HexValue &left, const HexValue &right) {
  if (left.size() != right.size())
    return left.size() < right.size() ? -1 : 1;

  for (std::size_t index = left.size(); index-- > 0;) {
    if (left[index] != right[index])
      return left[index] < right[index] ? -1 : 1;
  }

  return 0;
}

HexValue inclusiveSize(const HexValue &start, const HexValue &end) {
  if (compareHex(end, start) < 0)
    throw std::invalid_argument("Kangaroo range end is below its start");

  HexValue result = end;
  int borrow = 0;
  for (std::size_t index = 0; index < result.size(); ++index) {
    const int startDigit = index < start.size()
        ? static_cast<int>(start[index])
        : 0;
    int digit = static_cast<int>(result[index]) - startDigit - borrow;
    if (digit < 0) {
      digit += 16;
      borrow = 1;
    } else {
      borrow = 0;
    }
    result[index] = static_cast<unsigned int>(digit);
  }

  unsigned int carry = 1;
  for (auto &digit : result) {
    const unsigned int sum = digit + carry;
    digit = sum & 0xF;
    carry = sum >> 4;
    if (carry == 0)
      break;
  }
  if (carry != 0)
    result.push_back(carry);

  while (result.size() > 1 && result.back() == 0)
    result.pop_back();

  return result;
}

bool validCompressedPublicKey(const std::string &value) {
  if (value.size() != 66 ||
      (value.rfind("02", 0) != 0 && value.rfind("03", 0) != 0))
    return false;

  return std::all_of(
      value.begin(), value.end(),
      [](unsigned char character) { return std::isxdigit(character) != 0; });
}

unsigned int exactRangeBits(
    const std::string &startText,
    const std::string &endText) {
  const auto start = parseHex(startText, "Kangaroo range start");
  const auto end = parseHex(endText, "Kangaroo range end");
  const auto size = inclusiveSize(start, end);

  std::size_t nonZeroIndex = size.size();
  unsigned int nonZeroDigit = 0;
  for (std::size_t index = 0; index < size.size(); ++index) {
    if (size[index] == 0)
      continue;
    if (nonZeroIndex != size.size())
      throw std::invalid_argument(
          "Kangaroo assignment size must be an exact power of two");
    nonZeroIndex = index;
    nonZeroDigit = size[index];
  }

  if (nonZeroIndex == size.size() ||
      (nonZeroDigit != 1 && nonZeroDigit != 2 &&
       nonZeroDigit != 4 && nonZeroDigit != 8))
    throw std::invalid_argument(
        "Kangaroo assignment size must be an exact power of two");

  unsigned int digitBits = 0;
  while ((1U << digitBits) != nonZeroDigit)
    ++digitBits;
  const unsigned int bits =
      static_cast<unsigned int>(nonZeroIndex * 4) + digitBits;

  if (bits < 32 || bits > 170)
    throw std::invalid_argument(
        "PSCKangaroo assignment width must be between 32 and 170 bits");

  return bits;
}

} // namespace

KangarooEngine::KangarooEngine(std::string executable)
    : executable_(std::move(executable)) {}

EngineInfo KangarooEngine::info() const {
  EngineInfo result;
  result.name = "Pollard Kangaroo";
  result.version = "PSCKangaroo-compatible";
  result.backend = "CUDA";
  result.executable = executable_;
  result.available = !executable_.empty();
  return result;
}

bool KangarooEngine::prepare() {
  return !executable_.empty();
}

std::string KangarooEngine::buildCommand(
    const EngineLaunchRequest &request) const {
  if (executable_.empty())
    throw std::invalid_argument("Kangaroo executable is empty");

  if (!validCompressedPublicKey(request.publicKey))
    throw std::invalid_argument(
        "Kangaroo requires a compressed public key");

  if (request.workspace.empty() ||
      request.outputFile.empty() ||
      request.logFile.empty())
    throw std::invalid_argument(
        "Kangaroo launch paths are incomplete");

  if (request.device < 0)
    throw std::invalid_argument(
        "Kangaroo GPU device must be non-negative");

  const auto rangeBits =
      exactRangeBits(request.startKey, request.endKey);

  const auto resultsFile =
      (std::filesystem::path(request.workspace) / "RESULTS.TXT").string();

  if (std::filesystem::path(request.outputFile) ==
      std::filesystem::path(resultsFile))
    throw std::invalid_argument(
        "Kangaroo output file must differ from RESULTS.TXT");

  std::ostringstream command;
  command
      << "cd " << shellQuote(request.workspace)
      << " && umask 077"
      << " && : > " << shellQuote(request.outputFile)
      << " && chmod 600 " << shellQuote(request.outputFile)
      << " && : > " << shellQuote(resultsFile)
      << " && chmod 600 " << shellQuote(resultsFile)
      << " && { " << shellQuote(executable_)
      << " -gpu " << request.device
      << " -dp 16"
      << " -range " << rangeBits
      << " -pubkey " << shellQuote(request.publicKey)
      << " -start " << shellQuote(request.startKey)
      << " -ramlimit 8"
      << " -concurrent 1"
      << " -wwbuffer 5"
      << " -checkpoint 0"
      << " >> " << shellQuote(request.logFile)
      << " 2>&1; status=$?; "
      << "if [ -s " << shellQuote(resultsFile) << " ]; then "
      << "cp -- " << shellQuote(resultsFile)
      << " " << shellQuote(request.outputFile)
      << " && chmod 600 " << shellQuote(request.outputFile)
      << "; fi; exit $status; }";

  return command.str();
}

bool KangarooEngine::launch() {
  running_ = true;
  return true;
}

bool KangarooEngine::stop() {
  running_ = false;
  return true;
}

bool KangarooEngine::running() const {
  return running_;
}

} // namespace openpuzzle
