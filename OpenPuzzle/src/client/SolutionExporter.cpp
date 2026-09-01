#include "openpuzzle/client/SolutionExporter.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace openpuzzle::client {

namespace {

constexpr std::uintmax_t MaximumResultSize = 4096;

bool validAssignmentId(const std::string &value) {
  return !value.empty() &&
         value.size() <= 128 &&
         std::all_of(
             value.begin(),
             value.end(),
             [](unsigned char character) {
               return std::isalnum(character) ||
                      character == '-';
             });
}

bool validCompressedWif(const std::string &value) {
  static const std::string alphabet =
      "123456789ABCDEFGHJKLMNPQRSTUVWXYZ"
      "abcdefghijkmnopqrstuvwxyz";

  return value.size() == 52 &&
         (value.front() == 'K' ||
          value.front() == 'L') &&
         std::all_of(
             value.begin(),
             value.end(),
             [&](char character) {
               return alphabet.find(character) !=
                      std::string::npos;
             });
}

std::string lowercase(std::string value) {
  std::transform(
      value.begin(),
      value.end(),
      value.begin(),
      [](unsigned char character) {
        return static_cast<char>(
            std::tolower(character));
      });

  return value;
}

std::string trim(std::string value) {
  const auto notSpace =
      [](unsigned char character) {
        return std::isspace(character) == 0;
      };

  value.erase(
      value.begin(),
      std::find_if(
          value.begin(),
          value.end(),
          notSpace));

  value.erase(
      std::find_if(
          value.rbegin(),
          value.rend(),
          notSpace).base(),
      value.end());

  return value;
}

bool validHex(std::string value) {
  if (value.rfind("0x", 0) == 0 ||
      value.rfind("0X", 0) == 0) {
    value.erase(0, 2);
  }

  return
      !value.empty() &&
      value.size() <= 64 &&
      std::all_of(
          value.begin(),
          value.end(),
          [](unsigned char character) {
            return std::isxdigit(character) != 0;
          });
}

std::string normalizedHex(std::string value) {
  value = trim(std::move(value));

  if (value.rfind("0x", 0) == 0 ||
      value.rfind("0X", 0) == 0) {
    value.erase(0, 2);
  }

  value = lowercase(std::move(value));

  const auto first =
      value.find_first_not_of('0');

  return first == std::string::npos
             ? "0"
             : value.substr(first);
}

bool hexWithinRange(
    const std::string &value,
    const std::string &start,
    const std::string &end) {
  if (!validHex(value) ||
      !validHex(start) ||
      !validHex(end)) {
    return false;
  }

  const auto normalizedValue =
      normalizedHex(value);
  const auto normalizedStart =
      normalizedHex(start);
  const auto normalizedEnd =
      normalizedHex(end);

  const auto compare =
      [](const std::string &left,
         const std::string &right) {
        if (left.size() != right.size()) {
          return left.size() < right.size()
                     ? -1
                     : 1;
        }

        if (left == right) {
          return 0;
        }

        return left < right ? -1 : 1;
      };

  return
      compare(normalizedValue, normalizedStart) >= 0 &&
      compare(normalizedValue, normalizedEnd) <= 0;
}

bool ensurePrivateDirectory(
    const fs::path &path,
    std::string &error) {
  std::error_code filesystemError;
  const auto status =
      fs::symlink_status(
          path,
          filesystemError);

  if (filesystemError &&
      filesystemError !=
          std::errc::no_such_file_or_directory) {
    error = "Unable to inspect solution directory";
    return false;
  }

  if (fs::exists(status)) {
    if (fs::is_symlink(status) ||
        !fs::is_directory(status)) {
      error = "Unsafe solution directory path";
      return false;
    }
  } else if (!fs::create_directory(
                 path,
                 filesystemError) ||
             filesystemError) {
    error = "Unable to create solution directory";
    return false;
  }

  fs::permissions(
      path,
      fs::perms::owner_all,
      fs::perm_options::replace,
      filesystemError);

  if (filesystemError) {
    error = "Unable to protect solution directory";
    return false;
  }

  return true;
}

bool writeAll(
    int descriptor,
    const std::string &content) {
  std::size_t written = 0;

  while (written < content.size()) {
    const ssize_t result =
        ::write(
            descriptor,
            content.data() + written,
            content.size() - written);

    if (result < 0) {
      if (errno == EINTR) {
        continue;
      }

      return false;
    }

    if (result == 0) {
      return false;
    }

    written +=
        static_cast<std::size_t>(result);
  }

  return true;
}

bool publishProtectedTextFile(
    const fs::path &destination,
    const std::string &content,
    std::string &error) {
  std::error_code filesystemError;
  const auto status =
      fs::symlink_status(
          destination,
          filesystemError);

  if (!filesystemError &&
      fs::exists(status)) {
    if (fs::is_symlink(status) ||
        !fs::is_regular_file(status)) {
      error = "Unsafe solution notice path";
      return false;
    }

    fs::permissions(
        destination,
        fs::perms::owner_read |
            fs::perms::owner_write,
        fs::perm_options::replace,
        filesystemError);

    if (filesystemError) {
      error = "Unable to protect solution notice";
      return false;
    }

    return true;
  }

  if (filesystemError &&
      filesystemError !=
          std::errc::no_such_file_or_directory) {
    error = "Unable to inspect solution notice path";
    return false;
  }

  const fs::path temporary =
      destination.parent_path() /
      ("." +
       destination.filename().string() +
       ".tmp-" +
       std::to_string(
           static_cast<long long>(
               ::getpid())));

  fs::remove(
      temporary,
      filesystemError);
  filesystemError.clear();

  const int descriptor =
      ::open(
          temporary.c_str(),
          O_WRONLY |
              O_CREAT |
              O_EXCL |
              O_NOFOLLOW,
          S_IRUSR |
              S_IWUSR);

  if (descriptor < 0) {
    error = "Unable to create protected solution notice";
    return false;
  }

  const bool written =
      writeAll(
          descriptor,
          content);

  const bool synchronized =
      written &&
      ::fsync(descriptor) == 0;

  const bool closed =
      ::close(descriptor) == 0;

  if (!written ||
      !synchronized ||
      !closed) {
    fs::remove(
        temporary,
        filesystemError);
    error = "Unable to write protected solution notice";
    return false;
  }

  fs::rename(
      temporary,
      destination,
      filesystemError);

  if (filesystemError) {
    fs::remove(
        temporary,
        filesystemError);
    error = "Unable to publish solution notice";
    return false;
  }

  fs::permissions(
      destination,
      fs::perms::owner_read |
          fs::perms::owner_write,
      fs::perm_options::replace,
      filesystemError);

  if (filesystemError) {
    error = "Unable to protect solution notice";
    return false;
  }

  return true;
}

std::vector<std::string> readRecord(
    const fs::path &path,
    std::string &error) {
  std::error_code filesystemError;

  if (!fs::is_regular_file(
          path,
          filesystemError) ||
      filesystemError) {
    error = "Engine solution result is unavailable";
    return {};
  }

  const auto size =
      fs::file_size(
          path,
          filesystemError);

  if (filesystemError ||
      size == 0 ||
      size > MaximumResultSize) {
    error = "Engine solution result has an invalid size";
    return {};
  }

  std::ifstream input(
      path,
      std::ios::binary);

  if (!input.is_open()) {
    error = "Unable to open engine solution result";
    return {};
  }

  std::vector<std::string> lines;
  std::string line;

  while (std::getline(input, line)) {
    if (!line.empty() &&
        line.back() == '\r') {
      line.pop_back();
    }

    lines.push_back(line);

    if (lines.size() > 4) {
      break;
    }
  }

  if (lines.empty() ||
      lines.size() > 4) {
    error = "Unsupported engine solution format";
    return {};
  }

  return lines;
}

bool valueAfter(
    const std::string &line,
    const std::string &prefix,
    std::string &value) {
  if (line.rfind(prefix, 0) != 0) {
    return false;
  }

  value = line.substr(prefix.size());
  return !value.empty();
}

void publishSolutionNotice(
    const ClientExecutionState &state,
    const fs::path &root,
    SolutionExportResult &result) {
  const fs::path notice =
      root /
      ("KEY-FOUND-Puzzle-" +
       std::to_string(state.puzzle) +
       "-" +
       state.assignmentId +
       ".txt");

  std::ostringstream content;
  content
      << "OPENPUZZLE - PRIVATE KEY FOUND\n"
      << "================================\n"
      << "Puzzle: "
      << state.puzzle
      << "\n"
      << "Address: "
      << state.target
      << "\n\n"
      << "The private key is stored in the protected wallet file:\n"
      << result.walletPath
      << "\n\n"
      << "The private key is not included in this notice and was not uploaded.\n";

  std::string noticeError;

  if (!publishProtectedTextFile(
          notice,
          content.str(),
          noticeError)) {
    result.warning = noticeError;
    return;
  }

  result.noticePath =
      notice.string();
}

} // namespace

SolutionExportResult
SolutionExporter::exportSolution(
    const ClientExecutionState &state,
    const std::string &engineResultPath) {
  SolutionExportResult result;

  if (state.puzzle <= 0 ||
      state.target.empty() ||
      !validAssignmentId(
          state.assignmentId)) {
    result.error = "Invalid local assignment metadata";
    return result;
  }

  std::string parseError;
  const auto record =
      readRecord(
          engineResultPath,
          parseError);

  if (record.empty()) {
    result.error = parseError;
    return result;
  }

  std::string address;
  std::string privateKey;
  std::string compression;
  std::string privateKeyLabel;

  const auto engine =
      lowercase(state.engine);

  const bool kangaroo =
      engine == "kangaroo" ||
      engine == "psckangaroo";

  if (kangaroo &&
      record.size() == 1 &&
      valueAfter(
          record[0],
          "PRIVATE KEY:",
          privateKey)) {
    privateKey = trim(std::move(privateKey));

    if (!validHex(privateKey) ||
        !hexWithinRange(
            privateKey,
            state.start,
            state.end)) {
      result.error =
          "Kangaroo private key is invalid or outside the assignment";
      return result;
    }

    privateKey = normalizedHex(
        std::move(privateKey));
    privateKey.insert(
        privateKey.begin(),
        64 - privateKey.size(),
        '0');

    address = state.target;
    result.format = "hexadecimal";
    privateKeyLabel = "Private key (hex):";
  } else {
    if (record.size() != 4 ||
        record[0] !=
            "OPENPUZZLE_SOLUTION_V1" ||
        !valueAfter(
            record[1],
            "address=",
            address) ||
        !valueAfter(
            record[2],
            "private_key_wif=",
            privateKey) ||
        !valueAfter(
            record[3],
            "compression=",
            compression)) {
      result.error = "Unsupported engine solution format";
      return result;
    }

    if (address != state.target) {
      result.error = "Solution address does not match assignment";
      return result;
    }

    if (compression != "compressed" ||
        !validCompressedWif(
            privateKey)) {
      result.error = "Invalid compressed wallet-import key";
      return result;
    }

    result.format = "WIF (compressed)";
    privateKeyLabel = "Private key (WIF):";
  }

  const char *home =
      std::getenv("HOME");

  if (home == nullptr ||
      std::string(home).empty()) {
    result.error = "HOME is unavailable";
    return result;
  }

  const fs::path root =
      fs::path(home) /
      "OpenPuzzle-Solutions";

  const fs::path puzzleDirectory =
      root /
      ("Puzzle-" +
       std::to_string(state.puzzle));

  const fs::path assignmentDirectory =
      puzzleDirectory /
      state.assignmentId;

  std::string directoryError;

  if (!ensurePrivateDirectory(
          root,
          directoryError) ||
      !ensurePrivateDirectory(
          puzzleDirectory,
          directoryError) ||
      !ensurePrivateDirectory(
          assignmentDirectory,
          directoryError)) {
    result.error = directoryError;
    return result;
  }

  const fs::path destination =
      assignmentDirectory /
      "wallet-import.txt";

  std::error_code filesystemError;
  const auto destinationStatus =
      fs::symlink_status(
          destination,
          filesystemError);

  if (!filesystemError &&
      fs::exists(destinationStatus)) {
    if (fs::is_symlink(destinationStatus) ||
        !fs::is_regular_file(destinationStatus)) {
      result.error = "Unsafe wallet export path";
      return result;
    }

    fs::permissions(
        destination,
        fs::perms::owner_read |
            fs::perms::owner_write,
        fs::perm_options::replace,
        filesystemError);

    if (filesystemError) {
      result.error = "Unable to protect wallet export";
      return result;
    }

    result.success = true;
    result.walletPath =
        destination.string();
    publishSolutionNotice(
        state,
        root,
        result);
    return result;
  }

  if (filesystemError &&
      filesystemError !=
          std::errc::no_such_file_or_directory) {
    result.error = "Unable to inspect wallet export path";
    return result;
  }

  const fs::path temporary =
      assignmentDirectory /
      (".wallet-import.tmp-" +
       std::to_string(
           static_cast<long long>(
               ::getpid())));

  fs::remove(
      temporary,
      filesystemError);
  filesystemError.clear();

  const int descriptor =
      ::open(
          temporary.c_str(),
          O_WRONLY |
              O_CREAT |
              O_EXCL |
              O_NOFOLLOW,
          S_IRUSR |
              S_IWUSR);

  if (descriptor < 0) {
    result.error = "Unable to create protected wallet export";
    return result;
  }

  std::ostringstream content;
  content
      << "OpenPuzzle protected solution\n"
      << "=============================\n"
      << "Puzzle: "
      << state.puzzle
      << "\n"
      << "Address: "
      << address
      << "\n"
      << "Format: "
      << result.format
      << "\n"
      << privateKeyLabel
      << "\n"
      << privateKey
      << "\n";

  const bool written =
      writeAll(
          descriptor,
          content.str());

  const bool synchronized =
      written &&
      ::fsync(descriptor) == 0;

  const bool closed =
      ::close(descriptor) == 0;

  if (!written ||
      !synchronized ||
      !closed) {
    fs::remove(
        temporary,
        filesystemError);
    result.error = "Unable to write protected wallet export";
    return result;
  }

  fs::rename(
      temporary,
      destination,
      filesystemError);

  if (filesystemError) {
    fs::remove(
        temporary,
        filesystemError);
    result.error = "Unable to publish wallet export";
    return result;
  }

  fs::permissions(
      destination,
      fs::perms::owner_read |
          fs::perms::owner_write,
      fs::perm_options::replace,
      filesystemError);

  if (filesystemError) {
    result.error = "Unable to protect wallet export";
    return result;
  }

  result.success = true;
  result.walletPath =
      destination.string();
  publishSolutionNotice(
      state,
      root,
      result);
  return result;
}

} // namespace openpuzzle::client
