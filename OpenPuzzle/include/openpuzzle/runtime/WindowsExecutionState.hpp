#pragma once
#include "openpuzzle/client/ClientExecutionState.hpp"
#include "openpuzzle/runtime/WindowsEngineRecord.hpp"
namespace openpuzzle {
inline std::optional<WindowsEngineRecord> windowsExecutionRecord(
    const client::ClientExecutionState& state) {
  if (state.processPlatform != WindowsEngineRecord::platform || !state.bootId.empty() ||
      !WindowsRuntimeRecord{state.ownerPid, state.ownerStartTime}.valid())
    return std::nullopt;
  WindowsEngineRecord record{{state.pid, state.processStartTime}, state.processGeneration};
  return record.valid() ? std::optional(record) : std::nullopt;
}
inline bool bindWindowsExecutionState(client::ClientExecutionState& state,
                                     const WindowsEngineRecord& record,
                                     const WindowsRuntimeRecord& owner) {
  if (!record.valid() || !owner.valid()) return false;
  state.pid = record.process.pid;
  state.processStartTime = record.process.creationTime;
  state.processPlatform = WindowsEngineRecord::platform;
  state.processGeneration = record.generation;
  state.bootId.clear();
  state.ownerPid = owner.pid;
  state.ownerStartTime = owner.creationTime;
  return true;
}
}
