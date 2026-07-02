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
  auto temp =
      std::filesystem::temp_directory_path() / "openpuzzle_live_progress_test";
  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);

  int progressEvents = 0;
  std::string lastKeysChecked;

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
  ctx.onProgress = [&](const ExecutionResult &progress) {
    progressEvents++;
    lastKeysChecked = progress.keysChecked;
  };

  ExecutionManager manager;
  auto result = manager.run(ctx);

  if (!result.success)
    return 1;
  if (progressEvents != 1)
    return 2;
  if (lastKeysChecked != "104958263296")
    return 3;

  auto state = readFile(temp / "state.json");

  if (state.find("\"status\": \"FINISHED\"") == std::string::npos)
    return 4;
  if (state.find("\"keys_checked\": \"104958263296\"") == std::string::npos)
    return 5;

  std::filesystem::remove_all(temp);

  return 0;
}
