#include "openpuzzle/hardware/RusticlEnvironment.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace openpuzzle;

int main() {
  assert(RusticlEnvironment::valid("radeonsi"));
  assert(RusticlEnvironment::valid("radeonsi:0"));
  assert(RusticlEnvironment::valid("radeonsi,iris"));

  assert(!RusticlEnvironment::valid(""));
  assert(!RusticlEnvironment::valid("radeonsi iris"));
  assert(!RusticlEnvironment::valid("radeonsi;id"));
  assert(!RusticlEnvironment::valid("RUSTICL_ENABLE=radeonsi"));

  const char *original =
      std::getenv("RUSTICL_ENABLE");

  const bool hadOriginal =
      original != nullptr;

  const std::string saved =
      hadOriginal
          ? original
          : "";

  RusticlEnvironment::apply("radeonsi");

  const char *configured =
      std::getenv("RUSTICL_ENABLE");

  assert(configured != nullptr);
  assert(std::string(configured) == "radeonsi");

  if (hadOriginal) {
    assert(
        setenv(
            "RUSTICL_ENABLE",
            saved.c_str(),
            1) == 0);
  } else {
    assert(unsetenv("RUSTICL_ENABLE") == 0);
  }

  std::cout
      << "RusticlEnvironmentTests passed\n";

  return 0;
}
