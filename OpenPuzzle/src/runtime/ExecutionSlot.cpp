#include "openpuzzle/runtime/ExecutionSlot.hpp"

#include <limits>
#include <map>

namespace openpuzzle {

namespace {

constexpr const char* cudaPrefix = "cuda-";
constexpr const char* openclPrefix = "opencl-";

std::optional<std::uint64_t> numberedDevice(
    const std::string& value,
    const std::string& prefix) {
  if (value.size() <= prefix.size() ||
      value.compare(0, prefix.size(), prefix) != 0) {
    return std::nullopt;
  }

  const auto digits = value.substr(prefix.size());
  if (digits.size() > 1 && digits.front() == '0') {
    return std::nullopt;
  }

  std::uint64_t device = 0;
  for (const char character : digits) {
    if (character < '0' || character > '9') {
      return std::nullopt;
    }
    const auto digit =
        static_cast<std::uint64_t>(character - '0');
    if (device >
        (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
      return std::nullopt;
    }
    device = device * 10 + digit;
  }
  return device;
}

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

  return ExecutionSlot::valid(slot) &&
             (ExecutionSlot::cudaDevice(slot) ||
              ExecutionSlot::openclDevice(slot))
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
         cudaDevice(value).has_value() ||
         openclDevice(value).has_value();
}

std::string ExecutionSlot::cuda(
    const std::uint64_t device) {
  return std::string(cudaPrefix) +
         std::to_string(device);
}

std::string ExecutionSlot::opencl(
    const std::uint64_t device) {
  return std::string(openclPrefix) +
         std::to_string(device);
}

std::optional<std::uint64_t>
ExecutionSlot::cudaDevice(
    const std::string& value) {
  return numberedDevice(value, cudaPrefix);
}

std::optional<std::uint64_t>
ExecutionSlot::openclDevice(
    const std::string& value) {
  return numberedDevice(value, openclPrefix);
}

std::string ExecutionSlot::fileSuffix(
    const std::string& value) {
  if (value == "gpu" ||
      value == "cpu" ||
      value == "cuda" ||
      value == "opencl") {
    return "-" + value;
  }

  if (cudaDevice(value) || openclDevice(value)) {
    return "-" + value;
  }

  return {};
}

std::vector<std::string>
ExecutionSlot::discoverCudaSlots(
    const std::filesystem::path& directory) {
  std::vector<std::string> result;
  for (const auto& slot : discoverGpuSlots(directory)) {
    if (cudaDevice(slot)) {
      result.push_back(slot);
    }
  }
  return result;
}

std::vector<std::string>
ExecutionSlot::discoverGpuSlots(
    const std::filesystem::path& directory) {
  std::map<std::pair<int, std::uint64_t>, std::string> slots;
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

    const auto cudaIndex = cudaDevice(*slot);
    const auto openclIndex = openclDevice(*slot);

    if (cudaIndex) {
      slots.emplace(
          std::make_pair(0, *cudaIndex),
          *slot);
    } else if (openclIndex) {
      slots.emplace(
          std::make_pair(1, *openclIndex),
          *slot);
    }
  }

  std::vector<std::string> result;
  result.reserve(slots.size());

  for (const auto& [identity, slot] : slots) {
    (void) identity;
    result.push_back(slot);
  }

  return result;
}

} // namespace openpuzzle
