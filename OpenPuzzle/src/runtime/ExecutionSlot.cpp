#include "openpuzzle/runtime/ExecutionSlot.hpp"

#include <limits>
#include <map>

namespace openpuzzle {

namespace {

constexpr const char* cudaPrefix = "cuda-";

std::optional<std::string> slotFromFilename(
    const std::string& filename,
    const std::string& prefix,
    const std::string& suffix) {
  if (filename.size() <=
          prefix.size() + suffix.size() ||
      filename.compare(
          0,
          prefix.size(),
          prefix) != 0 ||
      filename.compare(
          filename.size() - suffix.size(),
          suffix.size(),
          suffix) != 0) {
    return std::nullopt;
  }

  const auto slot =
      filename.substr(
          prefix.size(),
          filename.size() -
              prefix.size() -
              suffix.size());

  return ExecutionSlot::cudaDevice(slot)
      ? std::optional<std::string>(slot)
      : std::nullopt;
}

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

std::vector<std::string>
ExecutionSlot::discoverCudaSlots(
    const std::filesystem::path& directory) {
  std::map<std::uint64_t, std::string> slots;
  std::error_code error;

  std::filesystem::directory_iterator iterator(
      directory,
      error);

  if (error) {
    return {};
  }

  const std::filesystem::directory_iterator end;

  for (; iterator != end; iterator.increment(error)) {
    if (error) {
      return {};
    }

    const auto filename =
        iterator->path().filename().string();

    std::optional<std::string> slot =
        slotFromFilename(
            filename,
            "client-",
            ".state");

    if (!slot) {
      slot = slotFromFilename(
          filename,
          "runtime-",
          ".pid");
    }

    if (!slot) {
      slot = slotFromFilename(
          filename,
          "safestop-",
          ".requested");
    }

    if (!slot) {
      continue;
    }

    const auto device = cudaDevice(*slot);

    if (device) {
      slots.emplace(*device, *slot);
    }
  }

  std::vector<std::string> result;
  result.reserve(slots.size());

  for (const auto& [device, slot] : slots) {
    (void) device;
    result.push_back(slot);
  }

  return result;
}

} // namespace openpuzzle
