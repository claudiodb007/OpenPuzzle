#pragma once
#include <string>

namespace openpuzzle {
// A verified measured profile supplies these paths. Registration is explicit;
// ordinary production routing is enabled only after its own preflight.
struct VanityEngineConfiguration {
  std::string engine;
  std::string adapter;
  std::string engineSha256;
  std::string adapterSha256;
};
}
