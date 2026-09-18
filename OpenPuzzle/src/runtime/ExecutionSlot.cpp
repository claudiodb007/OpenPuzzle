#include "openpuzzle/runtime/ExecutionSlot.hpp"

#include <limits>

namespace openpuzzle {

namespace {

constexpr const char* cudaPrefix = "cuda-";

} // namespace

bool ExecutionSlot::valid(
    const std::string& value) {
  return value == "primary" ||
         value == "gpu" ||
         value == "cpu" ||
         value == "cuda" ||
         value == "opencl" ||
         cudaDevice(value).has_value();
}

std::string ExecutionSlot::cuda(
    const std::uint64_t device) {
  return std::string(cudaPrefix) +
         std::to_string(device);
}

std::optional<std::uint64_t>
ExecutionSlot::cudaDevice(
    const std::string& value) {
  const std::string prefix(cudaPrefix);

  if (value.size() <= prefix.size() ||
      value.compare(0, prefix.size(), prefix) != 0) {
    return std::nullopt;
  }

  const auto digits =
      value.substr(prefix.size());

  if (digits.size() > 1 && digits.front() == '0') {
    return std::nullopt;
  }

  std::uint64_t device = 0;

  for (const char character : digits) {
    if (character < '0' || character > '9') {
      return std::nullopt;
    }

    const auto digit =
        static_cast<std::uint64_t>(
            character - '0');

    if (device >
        (std::numeric_limits<std::uint64_t>::max() - digit) /
            10) {
      return std::nullopt;
    }

    device = device * 10 + digit;
  }

  return device;
}

std::string ExecutionSlot::fileSuffix(
    const std::string& value) {
  if (value == "gpu" ||
      value == "cpu" ||
      value == "cuda" ||
      value == "opencl") {
    return "-" + value;
  }

  if (cudaDevice(value)) {
    return "-" + value;
  }

  return {};
}

} // namespace openpuzzle
