#include "openpuzzle/runtime/LocalExecutionBackend.hpp"

#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/ExecutionStopper.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"

namespace openpuzzle {

ExecutionHandle LocalExecutionBackend::launch(
    const StartExecutionRequest& request) {
  BackgroundExecutionLauncher launcher;

  return launcher.start(request);
}

bool LocalExecutionBackend::stop(
    const ExecutionHandle& handle) {
  if (handle.workspace.empty()) {
    return false;
  }

  ExecutionStopper stopper;

  return stopper.stop(
      handle.workspace);
}

} // namespace openpuzzle
