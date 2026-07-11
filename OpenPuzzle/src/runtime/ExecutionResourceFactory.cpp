#include "openpuzzle/runtime/ExecutionResourceFactory.hpp"

namespace openpuzzle {

ExecutionResource ExecutionResourceFactory::fromGpu(
    const GpuInfo& gpu,
    const std::string& engine) {
  ExecutionResource resource;

  resource.id =
      gpu.backend + ":" +
      std::to_string(gpu.device);

  if (!gpu.uuid.empty()) {
    resource.id = gpu.uuid + ":" + engine;
  }

  resource.name = gpu.name;
  resource.engine = engine;
  resource.backend = gpu.backend;
  resource.device = gpu.device;
  resource.memoryMb = gpu.memoryMb;
  resource.available = true;

  resource.capability.engine = engine;
  resource.capability.backend = gpu.backend;
  resource.capability.device = gpu.device;
  resource.capability.vramMb = gpu.memoryMb;
  resource.capability.available = true;

  return resource;
}

std::vector<ExecutionResource>
ExecutionResourceFactory::fromGpus(
    const std::vector<GpuInfo>& gpus,
    const std::string& engine) {
  std::vector<ExecutionResource> resources;
  resources.reserve(gpus.size());

  for (const auto& gpu : gpus) {
    resources.push_back(fromGpu(gpu, engine));
  }

  return resources;
}

} // namespace openpuzzle
