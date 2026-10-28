#include "openpuzzle/engines/EngineManager.hpp"

#include "openpuzzle/engines/bitcrack/BitCrackEngine.hpp"
#include "openpuzzle/engines/common/EngineDiscovery.hpp"
#include "openpuzzle/engines/keyhunt/KeyHuntEngine.hpp"
#include "openpuzzle/engines/kangaroo/KangarooEngine.hpp"
#include "openpuzzle/tools/ToolManager.hpp"
#ifdef OPENPUZZLE_ENABLE_VANITY_PREVIEW
#include "VanityCommonEngine.hpp"
#endif
#include <stdexcept>

#include <algorithm>
#include <cctype>
#include <utility>

namespace openpuzzle {

std::string EngineManager::normalize(
    std::string value) {
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

EngineManager::EngineManager() {
  EngineDiscovery discovery;

  EngineDescriptor bitcrack;
  bitcrack.id = "bitcrack";
  bitcrack.name = "BitCrack";
  bitcrack.version = "unknown";
  bitcrack.backend = "CUDA/OpenCL";

  auto configuredBitCrack =
      ToolManager::bitcrackCudaPath();

  bitcrack.runtime =
      discovery.discover(
          configuredBitCrack.value_or(
              "cuBitCrack"));

  bitcrack.capabilities.cuda = true;
  bitcrack.capabilities.opencl = true;
  bitcrack.capabilities.cpu = false;
  bitcrack.capabilities.supportsCompressed = true;
  bitcrack.capabilities.supportsUncompressed = false;
  bitcrack.capabilities.supportsResume = true;
  bitcrack.capabilities.supportsCheckpoint = true;
  bitcrack.capabilities.supportsBenchmark = true;
  bitcrack.capabilities.supportsLinearSearch = true;
  bitcrack.capabilities.supportsKangarooSearch = false;
  bitcrack.capabilities.requiresPublicKey = false;

  registry_.registerEngine(bitcrack);

  factory_.registerFactory(
      "bitcrack",
      [](const std::string& executable) {
        return std::make_unique<BitCrackEngine>(
            executable);
      });

  EngineDescriptor keyhunt;
  keyhunt.id = "keyhunt";
  keyhunt.name = "KeyHunt";
  keyhunt.version = "unknown";
  keyhunt.backend = "CPU";

  keyhunt.runtime =
      discovery.discover("keyhunt");

  keyhunt.capabilities.cpu = true;
  keyhunt.capabilities.cuda = false;
  keyhunt.capabilities.opencl = false;
  keyhunt.capabilities.supportsCompressed = true;
  keyhunt.capabilities.supportsUncompressed = true;
  keyhunt.capabilities.supportsResume = false;
  keyhunt.capabilities.supportsCheckpoint = false;
  keyhunt.capabilities.supportsBenchmark = false;
  keyhunt.capabilities.supportsLinearSearch = true;
  keyhunt.capabilities.supportsKangarooSearch = false;
  keyhunt.capabilities.requiresPublicKey = false;

  registry_.registerEngine(keyhunt);

  factory_.registerFactory(
      "keyhunt",
      [](const std::string& executable) {
        return std::make_unique<KeyHuntEngine>(
            executable);
      });
  EngineDescriptor kangaroo;
  kangaroo.id = "kangaroo";
  kangaroo.name = "Pollard Kangaroo";
  kangaroo.version = "PSCKangaroo-compatible";
  kangaroo.backend = "CUDA";

  const auto configuredKangaroo =
      ToolManager::kangarooPath();

  kangaroo.runtime =
      discovery.discover(
          configuredKangaroo.value_or(
              "psckangaroo"));
  kangaroo.capabilities.cuda = true;
  kangaroo.capabilities.opencl = false;
  kangaroo.capabilities.cpu = false;
  kangaroo.capabilities.supportsCompressed = true;
  kangaroo.capabilities.supportsUncompressed = false;
  kangaroo.capabilities.supportsResume = false;
  kangaroo.capabilities.supportsCheckpoint = false;
  kangaroo.capabilities.supportsBenchmark = false;
  kangaroo.capabilities.supportsLinearSearch = false;
  kangaroo.capabilities.supportsKangarooSearch = true;
  kangaroo.capabilities.requiresPublicKey = true;

  registry_.registerEngine(kangaroo);

  factory_.registerFactory(
      "kangaroo",
      [](const std::string& executable) {
        return std::make_unique<KangarooEngine>(
            executable);
      });
}

SearchEnginePtr EngineManager::create(
    const std::string& engine,
    const std::string& executable) const {
  return factory_.create(
      normalize(engine),
      executable);
}

EngineManager::EngineManager(const VanityEngineConfiguration& vanity)
    : EngineManager() {
#ifdef OPENPUZZLE_ENABLE_VANITY_PREVIEW
  VanityCommonEngine engine(vanity);
  if (!engine.prepare()) throw std::invalid_argument("Verified Vanity executables required");
  EngineDescriptor descriptor;
  descriptor.id="vanity"; descriptor.name="Vanity"; descriptor.version="0.2.0";
  descriptor.backend="CUDA";
  descriptor.runtime.executable=vanity.engine;
  descriptor.runtime.installed=true; descriptor.runtime.available=true;
  descriptor.capabilities.cuda=true;
  descriptor.capabilities.supportsUncompressed=false;
  descriptor.capabilities.supportsBenchmark=false;
  descriptor.capabilities.supportsLinearSearch=true;
  descriptor.capabilities.supportsCheckpoint=true;
  descriptor.capabilities.supportsResume=true;
  descriptor.capabilities.defaultBlocks=1024;
  descriptor.capabilities.defaultThreads=256;
  descriptor.capabilities.defaultPoints=1024;
  descriptor.capabilities.distributedReady=true;
  registry_.registerEngine(descriptor);
  factory_.registerFactory("vanity",[vanity](const auto& executable) {
    if (executable!=vanity.engine) throw std::invalid_argument("Vanity executable binding differs");
    return std::make_unique<VanityCommonEngine>(vanity);
  });
#else
  (void)vanity;
  throw std::invalid_argument("Vanity support is not included in this build");
#endif
}

std::optional<std::string>
EngineManager::resolveExecutable(
    const std::string& engine,
    const std::string& backend) const {
  const auto engineId =
      normalize(engine);

  const auto backendId =
      normalize(backend);

  if (engineId == "bitcrack") {
    if (backendId == "opencl") {
      return ToolManager::bitcrackOpenCLPath();
    }

    if (backendId == "cuda") {
      return ToolManager::bitcrackCudaPath();
    }

    return std::nullopt;
  }

  if ((engineId == "kangaroo" || engineId == "vanity") &&
      backendId != "cuda") {
    return std::nullopt;
  }

  const auto* descriptor =
      registry_.find(engineId);

  if (!descriptor ||
      !descriptor->runtime.installed ||
      descriptor->runtime.executable.empty()) {
    return std::nullopt;
  }

  return descriptor->runtime.executable;
}

const EngineRegistry&
EngineManager::registry() const {
  return registry_;
}

} // namespace openpuzzle
