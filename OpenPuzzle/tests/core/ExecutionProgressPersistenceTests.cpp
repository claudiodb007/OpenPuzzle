#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace openpuzzle;

static std::string readFile(const std::filesystem::path &path) {
  std::ifstream in(path);
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

int main() {
  auto temp = std::filesystem::temp_directory_path() /
              "openpuzzle_progress_persistence_test";
  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);

  ExecutionContext ctx;
  ctx.executionId = 1;
  ctx.puzzleId = 71;
  ctx.jobId = 42;
  ctx.rangeId = 1001;
  ctx.engine = "BitCrack";
  ctx.workspace = temp.string();
  ctx.command = "printf 'NVIDIA GeForce R 7918 / 11873MB | 1 target 1319.71 "
                "MKey/s (104,958,263,296 total) [00:01:18]\\n'";
  ctx.echoOutput = false;

  ExecutionManager manager;
  auto result = manager.run(ctx);

  if (!result.success)
    return 1;
  if (result.averageSpeed != 1319.71)
    return 2;
  if (result.keysChecked != "104958263296")
    return 3;

  auto state = readFile(temp / "state.json");

  if (state.find("\"keys_checked\": \"104958263296\"") == std::string::npos)
    return 4;

  std::filesystem::remove_all(temp);

  return 0;
}
