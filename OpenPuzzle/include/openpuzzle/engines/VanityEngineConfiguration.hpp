#pragma once
#include <string>

namespace openpuzzle {
// The packaged engine and adapter supply these paths and immutable hashes.
struct VanityEngineConfiguration {
  std::string engine;
  std::string adapter;
  std::string engineSha256;
  std::string adapterSha256;
};
}
