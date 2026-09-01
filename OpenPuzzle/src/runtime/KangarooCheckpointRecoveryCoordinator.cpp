#include "openpuzzle/runtime/KangarooCheckpointRecoveryCoordinator.hpp"

#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/ExecutionStopper.hpp"
#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"
#include "openpuzzle/runtime/KangarooWalkSeed.hpp"

#include <cerrno>
#include <csignal>
#include <exception>
#include <limits>
#include <string>
#include <utility>
#include <unistd.h>

namespace openpuzzle {
namespace {

KangarooCheckpointRecoveryResult failed(std::string error) {
  KangarooCheckpointRecoveryResult result;
  result.error = std::move(error);
  return result;
}

bool previousProcessStillMatches(
    const client::ClientExecutionState &state) {
  if (state.pid <= 0 || state.bootId.empty() ||
      state.processStartTime == 0) {
    return false;
  }

  const auto bootId = client::ClientStateStore::currentBootId();
  if (bootId.empty() || bootId != state.bootId) {
    return false;
  }

  if (kill(state.pid, 0) != 0 && errno != EPERM) {
    return false;
  }

  const auto startTime = LinuxProcessIdentity::startTime(state.pid);
  return startTime && *startTime == state.processStartTime;
}

} // namespace

KangarooCheckpointRecoveryResult
KangarooCheckpointRecoveryCoordinator::resume(
    const client::ClientExecutionState &previousState,
    const KangarooCheckpointRecoveryPlan &plan,
    const KangarooRecoveryLeaseResult &lease) {
  if (!plan.eligible || plan.request.command.empty()) {
    return failed("Kangaroo recovery plan is not eligible");
  }

  if (!lease.accepted()) {
    return failed("Kangaroo assignment lease was not accepted");
  }

  if (!KangarooWalkSeed::valid(plan.walkSeed) ||
      plan.request.walkSeed != plan.walkSeed) {
    return failed("Kangaroo recovery seed contract is invalid");
  }

  if ((!previousState.kangarooWalkSeed.empty() &&
       !KangarooWalkSeed::valid(previousState.kangarooWalkSeed)) ||
      previousState.kangarooGeneration ==
          std::numeric_limits<std::uint64_t>::max() ||
      plan.walkSeed == previousState.kangarooWalkSeed ||
      plan.generation != previousState.kangarooGeneration + 1) {
    return failed("Kangaroo recovery generation contract is invalid");
  }

  if (previousProcessStillMatches(previousState)) {
    return failed("Previous Kangaroo process is still running");
  }

  if (plan.request.workspace != previousState.workspace ||
      plan.request.rangeId != previousState.rangeId ||
      plan.request.puzzleId != previousState.puzzle) {
    return failed("Kangaroo recovery plan does not match local state");
  }

  BackgroundExecutionLauncher launcher;
  ExecutionHandle handle;

  try {
    handle = launcher.start(plan.request);
  } catch (const std::exception &exception) {
    return failed(std::string("Unable to restart Kangaroo: ") +
                  exception.what());
  }

  auto recovered = previousState;
  recovered.pid = handle.pid;
  recovered.bootId = client::ClientStateStore::currentBootId();
  recovered.command = plan.request.command;
  recovered.kangarooWalkSeed = plan.walkSeed;
  recovered.kangarooGeneration = plan.generation;

  const auto startTime = LinuxProcessIdentity::startTime(handle.pid);
  if (recovered.bootId.empty() || !startTime || *startTime == 0) {
    ExecutionStopper().stop(handle.workspace);
    return failed("Unable to establish restarted process identity");
  }

  recovered.processStartTime = *startTime;

  if (!client::ClientStateStore::save(recovered)) {
    ExecutionStopper().stop(handle.workspace);
    return failed("Unable to persist restarted Kangaroo state");
  }

  KangarooCheckpointRecoveryResult result;
  result.resumed = true;
  result.state = std::move(recovered);
  return result;
}

} // namespace openpuzzle
