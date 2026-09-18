#include "openpuzzle/runtime/CudaDeviceSelection.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <set>
#include <stdexcept>

namespace openpuzzle {

namespace {

std::string trim(
    const std::string& value) {
  const auto first =
      std::find_if_not(
          value.begin(),
          value.end(),
          [](const unsigned char character) {
            return std::isspace(character) != 0;
          });

  const auto last =
      std::find_if_not(
          value.rbegin(),
          value.rend(),
          [](const unsigned char character) {
            return std::isspace(character) != 0;
          }).base();

  if (first >= last) {
    return {};
  }

  return std::string(first, last);
}

int parseDevice(
    const std::string& token) {
  const auto value = trim(token);

  if (value.empty()) {
    throw std::runtime_error(
        "CUDA device list contains an empty item");
  }

  std::size_t consumed = 0;
  long long device = -1;

  try {
    device = std::stoll(value, &consumed, 10);
  } catch (...) {
    throw std::runtime_error(
        "CUDA devices must be non-negative whole numbers");
  }

  if (consumed != value.size() ||
      device < 0 ||
      device > std::numeric_limits<int>::max()) {
    throw std::runtime_error(
        "CUDA devices must be non-negative whole numbers");
  }

  return static_cast<int>(device);
}

std::vector<int> inventory(
    const std::vector<GpuInfo>& availableDevices) {
  std::vector<int> result;
  std::set<int> seen;

  for (const auto& gpu : availableDevices) {
    if (gpu.device < 0) {
      throw std::runtime_error(
          "CUDA inventory contains an invalid device index");
    }

    if (!seen.insert(gpu.device).second) {
      throw std::runtime_error(
          "CUDA inventory contains duplicate device " +
          std::to_string(gpu.device));
    }

    result.push_back(gpu.device);
  }

  std::sort(result.begin(), result.end());
  return result;
}

} // namespace

std::vector<int> CudaDeviceSelection::resolve(
    const std::string& selector,
    const std::vector<GpuInfo>& availableDevices) {
  const auto available =
      inventory(availableDevices);

  if (available.empty()) {
    throw std::runtime_error(
        "No CUDA devices were found");
  }

  const auto requested = trim(selector);

  if (requested.empty()) {
    throw std::runtime_error(
        "--devices requires 'all' or a comma-separated list");
  }

  if (requested == "all") {
    return available;
  }

  std::vector<int> result;
  std::set<int> seen;
  std::size_t beginning = 0;

  while (beginning <= requested.size()) {
    const auto separator =
        requested.find(',', beginning);

    const auto token =
        requested.substr(
            beginning,
            separator == std::string::npos
                ? std::string::npos
                : separator - beginning);

    const int device = parseDevice(token);

    if (!seen.insert(device).second) {
      throw std::runtime_error(
          "CUDA device " +
          std::to_string(device) +
          " was selected more than once");
    }

    if (!std::binary_search(
            available.begin(),
            available.end(),
            device)) {
      throw std::runtime_error(
          "CUDA device " +
          std::to_string(device) +
          " was not found");
    }

    result.push_back(device);

    if (separator == std::string::npos) {
      break;
    }

    beginning = separator + 1;
  }

  return result;
}

} // namespace openpuzzle
