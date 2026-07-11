#include "openpuzzle/engines/common/EngineRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace openpuzzle {

std::string EngineRegistry::normalizeId(
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

bool EngineRegistry::supportsBackend(
    const EngineDescriptor& engine,
    const std::string& backend) {
  const auto normalized =
      normalizeId(backend);

  if (normalized.empty()) {
    return true;
  }

  if (normalized == "cuda") {
    return engine.capabilities.cuda;
  }

  if (normalized == "opencl") {
    return engine.capabilities.opencl;
  }

  if (normalized == "cpu") {
    return engine.capabilities.cpu;
  }

  return normalizeId(engine.backend) ==
         normalized;
}

void EngineRegistry::registerEngine(
    const EngineDescriptor& engine) {
  const auto normalizedId =
      normalizeId(engine.id);

  for (auto& existing : engines_) {
    if (normalizeId(existing.id) ==
        normalizedId) {
      existing = engine;
      return;
    }
  }

  engines_.push_back(engine);
}

const std::vector<EngineDescriptor>&
EngineRegistry::engines() const {
  return engines_;
}

const EngineDescriptor* EngineRegistry::find(
    const std::string& id) const {
  const auto normalizedId =
      normalizeId(id);

  for (const auto& engine : engines_) {
    if (normalizeId(engine.id) ==
        normalizedId) {
      return &engine;
    }
  }

  return nullptr;
}

std::vector<EngineDescriptor>
EngineRegistry::installed() const {
  std::vector<EngineDescriptor> result;

  for (const auto& engine : engines_) {
    if (engine.runtime.installed) {
      result.push_back(engine);
    }
  }

  return result;
}

std::vector<EngineDescriptor>
EngineRegistry::available() const {
  std::vector<EngineDescriptor> result;

  for (const auto& engine : engines_) {
    if (engine.runtime.available) {
      result.push_back(engine);
    }
  }

  return result;
}

std::vector<EngineDescriptor>
EngineRegistry::supportingBackend(
    const std::string& backend,
    bool installedOnly) const {
  std::vector<EngineDescriptor> result;

  for (const auto& engine : engines_) {
    if (installedOnly &&
        !engine.runtime.installed) {
      continue;
    }

    if (!supportsBackend(
            engine,
            backend)) {
      continue;
    }

    result.push_back(engine);
  }

  return result;
}

std::size_t EngineRegistry::count() const {
  return engines_.size();
}

void EngineRegistry::clear() {
  engines_.clear();
}

} // namespace openpuzzle
