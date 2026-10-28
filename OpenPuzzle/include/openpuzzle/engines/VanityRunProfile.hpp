#pragma once
#include "openpuzzle/engines/VanityEngineConfiguration.hpp"
#include "openpuzzle/client/ClientExecutionState.hpp"
#include "openpuzzle/client/RangeAssignment.hpp"
#include "openpuzzle/engines/common/PuzzleExecutionPlanner.hpp"
#include <cstdint>
#include <filesystem>
#include <string>

namespace openpuzzle {
struct VanityRunProfile {
  VanityEngineConfiguration executables;
  std::string gpu,fingerprint,uuid,driver;
  int device=0;
  unsigned grid=32;
  double planningMKeys=0,powerLimit=0;
  bool localOnly=false;
  static bool configured();
  static VanityRunProfile load(int device=0);
  static void requireServer(const std::string& server);
  std::uint64_t maximumKeys() const;
  std::uint64_t requestedKeys(int minutes) const;
  void validateHardware(int device,bool requireIdle) const;
  void validatePuzzle(const PuzzleExecutionMetadata&) const;
  void validateAssignment(const client::RangeAssignment&,const PuzzleExecutionMetadata&) const;
  void bindWorkspace(const std::filesystem::path&,const std::string& server) const;
  void requireWorkspaceBinding(const std::filesystem::path&,const std::string& server) const;
  // Pure native proof before the ordinary runtime probes a lease or relaunches.
  std::uint64_t prepareRecovery(const client::ClientExecutionState&,const std::filesystem::path& expected,
                               const std::string& server) const;
};
}
