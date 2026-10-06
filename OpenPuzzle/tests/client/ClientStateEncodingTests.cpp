#include "openpuzzle/client/ClientStateStore.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <locale>
#include <string>
#include <unistd.h>

namespace {
struct GroupedNumbers : std::numpunct<char> {
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\3"; }
};
std::string read(const std::filesystem::path& path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), {}};
}
}

int main() {
  namespace fs = std::filesystem;
  using namespace openpuzzle::client;
  const char* previous = std::getenv("HOME");
  const std::string previousHome = previous ? previous : "";
  const fs::path root = fs::temp_directory_path() /
      ("openpuzzle-state-encoding-" + std::to_string(getpid()));
  fs::remove_all(root); fs::create_directories(root);
  assert(setenv("HOME", root.c_str(), 1) == 0);
  ClientExecutionState state;
  state.active = true; state.assignmentId = "assignment"; state.clientId = "client";
  state.puzzle = 71; state.rangeId = 1234; state.pid = 12345;
  state.workspace = "/tmp/space \"quoted\"/back\\slash\r\n\t";
  state.command = "printf 'with\rcarriage\nnewline' \\r \\n \\\"quote\\\"";
  state.gpuName = "GPU\rname"; state.processStartTime = 123456789;
  const auto previousLocale = std::locale();
  std::locale::global(std::locale(std::locale::classic(), new GroupedNumbers));
  assert(ClientStateStore::save(state, "cuda-2"));
  auto loaded = ClientStateStore::load("cuda-2");
  assert(loaded);
  assert(loaded->workspace == state.workspace && loaded->command == state.command);
  assert(loaded->gpuName == state.gpuName);
  assert(loaded->rangeId == 1234 && loaded->pid == 12345);
  assert(loaded->processStartTime == state.processStartTime);
  std::locale::global(previousLocale);
  const auto path = ClientStateStore::path("cuda-2");
  const auto original = read(path);

  // Invalid identity numbers remain unavailable (zero), without hiding the
  // occupied assignment. A minus sign must not become UINT64_MAX.
  for (const std::string value : {"-1", "+1", "1suffix", "18446744073709551616"}) {
    auto text = original;
    const std::string marker = "process_start_time=123456789";
    text.replace(text.find(marker), marker.size(), "process_start_time=" + value);
    { std::ofstream output(path); output << text; }
    loaded = ClientStateStore::load("cuda-2");
    assert(loaded && loaded->active && loaded->processStartTime == 0);
    assert(loaded->assignmentId == state.assignmentId);
    assert(read(path) == text);
  }
  state.processStartTime = std::numeric_limits<std::uint64_t>::max();
  assert(ClientStateStore::save(state, "cuda-2"));
  loaded = ClientStateStore::load("cuda-2");
  assert(loaded && loaded->processStartTime == state.processStartTime);

  // Missing identity fields in older files retain their established defaults.
  auto text = read(path);
  const auto a = text.find("process_start_time=");
  text.erase(a, text.find('\n', a) + 1 - a);
  { std::ofstream output(path); output << text; }
  loaded = ClientStateStore::load("cuda-2");
  assert(loaded && loaded->processStartTime == 0);
  assert(loaded->command == state.command);
  assert(!ClientStateStore::load("opencl-2"));
  if (previous) assert(setenv("HOME", previousHome.c_str(), 1) == 0);
  else assert(unsetenv("HOME") == 0);
  fs::remove_all(root);
  std::cout << "ClientStateEncodingTests passed\n";
}
