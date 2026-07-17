#pragma once

#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/runtime/ExecutionHandle.hpp"
#include "openpuzzle/workers/WorkerEngineCapability.hpp"

#include <optional>
#include <string>
#include <vector>

namespace openpuzzle {

class BackgroundExecutionLauncher;
class ExecutionStopper;
struct StartExecutionRequest;

enum class WorkerAgentState {
  Offline,
  Idle,
  Busy
};

struct WorkerAgentInfo {
  int workerId = 0;

  std::string machine;
  std::string gpuName;
  std::string backend;
  std::string engine;

  WorkerAgentState state = WorkerAgentState::Idle;

  double speedMkeys = 0.0;
  double temperature = 0.0;
  double power = 0.0;

  std::vector<WorkerEngineCapability> capabilities;
};

class WorkerAgent {
public:
  WorkerAgent();
  explicit WorkerAgent(WorkerAgentInfo info);

  const WorkerAgentInfo& info() const;

  WorkerAgentState state() const;

  bool offline() const;
  bool online() const;
  bool idle() const;
  bool busy() const;

  void markOffline();
  void markIdle();
  void markBusy();

  bool hasExecution() const;
  const std::optional<ExecutionHandle>& currentExecution() const;

  bool supports(const std::string& engine,
                const std::string& backend) const;

  const WorkerEngineCapability* bestCapability(
      const std::string& engine,
      const std::string& backend) const;

  ExecutionHandle execute(
      BackgroundExecutionLauncher& launcher,
      const StartExecutionRequest& request);

  bool attachExecution(
      const ExecutionHandle& handle);

  bool stop(ExecutionStopper& stopper);
  bool completeExecution();

  WorkerRecord toRecord() const;

  static std::string stateToString(WorkerAgentState state);
  static WorkerAgentState stateFromString(const std::string& state);

private:
  WorkerAgentInfo info_;
  std::optional<ExecutionHandle> currentExecution_;
};

} // namespace openpuzzle
