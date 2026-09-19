#include "openpuzzle/runtime/OpenclDeviceSelection.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using openpuzzle::GpuInfo;
using openpuzzle::OpenclDeviceSelection;

namespace {

GpuInfo gpu(const int device) {
  GpuInfo value;
  value.device = device;
  value.name = "OpenCL GPU " + std::to_string(device);
  value.backend = "OpenCL";
  value.memoryMb = 8192;
  value.opencl = true;
  return value;
}

void fails(
    const std::string& selector,
    const std::vector<GpuInfo>& available,
    const std::string& expected) {
  try {
    (void) OpenclDeviceSelection::resolve(selector, available);
  } catch (const std::runtime_error& error) {
    assert(std::string(error.what()).find(expected) != std::string::npos);
    return;
  }
  assert(false);
}

} // namespace

int main() {
  const std::vector<GpuInfo> devices = {
      gpu(5), gpu(1), gpu(3),
  };

  assert(
      OpenclDeviceSelection::resolve("all", devices) ==
      std::vector<int>({1, 3, 5}));
  assert(
      OpenclDeviceSelection::resolve("5,1", devices) ==
      std::vector<int>({5, 1}));

  fails("", devices, "--devices requires");
  fails("1,1", devices, "selected more than once");
  fails("2", devices, "was not found");
  fails("-1", devices, "non-negative");
  fails("1,,3", devices, "empty item");
  fails("all", {}, "No OpenCL devices");

  std::cout << "OpenclDeviceSelectionTests passed\n";
  return 0;
}
