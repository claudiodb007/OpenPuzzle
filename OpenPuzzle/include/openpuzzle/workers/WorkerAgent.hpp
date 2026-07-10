#pragma once

#include "openpuzzle/models/Models.hpp"

#include <string>

namespace openpuzzle {

class BackgroundExecutionLauncher;
struct StartExecutionRequest;
struct ExecutionHandle;

enum class WorkerState {
  Offline,
  Idle,
  Busy
};

struct WorkerAgentInfo {
  std::string machine;
  std::string gpuName;
  std::string backend;
  std::string engine;

  WorkerState state = WorkerState::Idle;

  double speedMkeys = 0.0;
  double temperature = 0.0;
  double power = 0.0;
};

class WorkerAgent {
public:
  WorkerAgent();

  explicit WorkerAgent(WorkerAgentInfo info);

  const WorkerAgentInfo& info() const;

  WorkerState state() const;

  bool offline() const;
  bool online() const;
  bool idle() const;
  bool busy() const;

  void markOffline();
  void markIdle();
  void markBusy();

  WorkerRecord toRecord() const;

  ExecutionHandle execute(
      BackgroundExecutionLauncher& launcher,
      const StartExecutionRequest& request);

  static std::string stateToString(WorkerState state);
  static WorkerState stateFromString(const std::string& state);

private:
  WorkerAgentInfo info_;
};

} // namespace openpuzzle
