#include "openpuzzle/core/RecoveryManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"
#include "openpuzzle/runtime/ExecutionPersistence.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

int main() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() /
      ("openpuzzle-persistence-json-" + std::to_string(getpid()));
  fs::remove_all(root);
  openpuzzle::WorkspaceManager workspace(root);
  workspace.createJobWorkspace(42);
  openpuzzle::ExecutionContext context;
  context.executionId = 7; context.puzzleId = 71;
  context.jobId = 42; context.rangeId = 1001;
  context.engine = "BitCrack \"quoted\"";
  context.command = "printf \"test\\n\"\n\t\r\b\f" + std::string(1, '\x01');
  context.workspace = workspace.jobWorkspace(42).string();
  context.echoOutput = false;
  openpuzzle::ExecutionPersistence persistence;
  persistence.writeExecutionFile(context); // Persist only; never execute the command.
  boost::property_tree::ptree json;
  bool valid = true;
  try { boost::property_tree::read_json(workspace.executionFile(42).string(), json); }
  catch (...) { valid = false; }
  std::cout << "Stored execution is valid JSON: " << valid << std::endl;
  assert(valid);
  assert(json.get<std::string>("engine") == context.engine);
  assert(json.get<std::string>("command") == context.command);
  openpuzzle::RecoveryManager recovery(workspace);
  const auto restored = recovery.buildExecutionContext(42);
  assert(restored.executionId == 7 && restored.jobId == 42 && restored.rangeId == 1001);
  assert(restored.command == context.command && restored.engine == context.engine);
  assert(restored.workspace == context.workspace && !restored.echoOutput);

  // A workspace containing quotes and backslashes also survives persistence.
  const fs::path odd = root / "space \"quote\" back\\slash";
  fs::create_directories(odd);
  context.workspace = odd.string();
  persistence.writeExecutionFile(context);
  boost::property_tree::read_json((odd / "execution.json").string(), json);
  assert(json.get<std::string>("workspace") == context.workspace);

  openpuzzle::ExecutionResult result;
  result.exitCode = 7; result.linesRead = 3; result.averageSpeed = 123.5;
  result.keysChecked = "12345";
  persistence.writeStateFile(context, "FAILED", result);
  boost::property_tree::read_json((odd / "state.json").string(), json);
  assert(json.get<std::string>("status") == "FAILED");
  assert(json.get<int>("exit_code") == 7);
  assert(json.get<std::string>("keys_checked") == "12345");
  // JSON whitespace and field order cannot change recovery status or booleans.
  {
    std::ofstream file(workspace.stateFile(42));
    file << R"({"status":"FINISHED","exit_code":0,"lines_read":2,"average_speed":123.5})";
  }
  auto recoveredState = recovery.load(42);
  assert(recoveredState.status == openpuzzle::RecoveryStatus::Finished);
  assert(recoveredState.exitCode == 0 && recoveredState.linesRead == 2);
  assert(recoveredState.averageSpeed == 123.5);
  {
    std::ofstream file(workspace.executionFile(42));
    file << R"({"echo_output":false,"command":"echo true","execution_id":7,"job_id":42})";
  }
  auto recoveredContext = recovery.buildExecutionContext(42);
  assert(!recoveredContext.echoOutput && recoveredContext.command == "echo true");
  assert(recoveredContext.executionId == 7 && recoveredContext.jobId == 42);
  {
    std::ofstream file(workspace.stateFile(42));
    file << R"({"status":"RUNNING","exit_code":"2suffix","lines_read":-1,"average_speed":"123.5suffix"})";
  }
  recoveredState = recovery.load(42);
  assert(recoveredState.status == openpuzzle::RecoveryStatus::Running);
  assert(recoveredState.exitCode == -1 && recoveredState.linesRead == 0);
  assert(recoveredState.averageSpeed == 0.0);
  {
    std::ofstream file(workspace.executionFile(42));
    file << R"({"execution_id":7,"puzzle_id":71,"job_id":42,"command":"echo true")";
  }
  recoveredContext = recovery.buildExecutionContext(42);
  assert(recoveredContext.executionId == 0 && recoveredContext.puzzleId == 0);
  assert(recoveredContext.command.empty() && recoveredContext.jobId == 42);
  fs::remove_all(root);
  std::cout << "ExecutionPersistenceRoundTripTests passed\n";
}
