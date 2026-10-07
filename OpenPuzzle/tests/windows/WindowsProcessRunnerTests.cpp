#include "openpuzzle/core/ProcessRunnerFactory.hpp"
#include "openpuzzle/core/WindowsProcessRunner.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace openpuzzle;
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
std::string utf8(const std::wstring &text) {
  const int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  std::string result(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), n, nullptr, nullptr);
  return result;
}
int wmain(int argc, wchar_t **argv) {
 try {
  require(argc == 2, "fixture path required");
  const std::filesystem::path fixture = std::filesystem::absolute(argv[1]);
  const std::string command = "\"" + utf8(fixture.wstring()) + "\" ";
  auto runner = ProcessRunnerFactory::create();
  require(dynamic_cast<WindowsProcessRunner *>(runner.get()), "factory must use Windows runner");
  std::vector<std::string> lines;
  auto collect = [&](const std::string &line) { lines.push_back(line); };
  auto result = runner->run(command + "lines", collect, 3);
  require(result.started && result.exitCode == 0, "successful child exit");
  require(lines == std::vector<std::string>({"first", "second", "third", "stderr", "tail"}), "CR/LF, stderr and final partial line");
  for (int code : {7, 259}) {
    result = runner->run(command + "code " + std::to_string(code), {}, 3);
    require(result.started && result.exitCode == code, "nonzero child exit preserved, including STILL_ACTIVE numeric value");
  }
  lines.clear(); result = runner->run(command + "input", collect, 3);
  require(result.exitCode == 0 && lines == std::vector<std::string>({"input-eof"}), "stdin must be EOF");
  result = runner->run("", {}, 1); require(!result.started, "empty command rejected");
  result = runner->run(std::string("echo ok\0extra", 13), {}, 1); require(!result.started, "embedded NUL rejected");
  result = runner->run(std::string("\xff", 1), {}, 1); require(!result.started, "invalid UTF-8 rejected");
  result = runner->run(command + "sleep", {}, 1, [] { return true; });
  require(!result.started, "preexisting cancellation must prevent launch");
  for (const std::string mode : {"sleep", "closed", "flood"}) {
    const auto before = std::chrono::steady_clock::now();
    result = runner->run(command + mode, {}, 1);
    require(result.started && result.exitCode == 124, "timeout must terminate silent, closed-output or continuous-output child");
    require(std::chrono::steady_clock::now() - before < std::chrono::seconds(6), "timeout must be bounded");
  }
  int delivered = 0;
  result = runner->run(command + "flood", [&](const std::string &) { ++delivered; }, 10, [&] { return delivered >= 2; });
  require(result.exitCode == 124 && delivered >= 2, "callback-driven cancellation");
  HANDLE child = nullptr;
  result = runner->run(command + "tree", [&](const std::string &line) {
    if (line.starts_with("child=")) child = OpenProcess(SYNCHRONIZE, FALSE, std::stoul(line.substr(6)));
  }, 1);
  require(result.exitCode == 124 && child, "descendant captured");
  const DWORD ended = WaitForSingleObject(child, 5000); CloseHandle(child); child = nullptr;
  require(ended == WAIT_OBJECT_0, "timeout must stop the entire descendant tree");
  bool callbackThrown = false;
  try { runner->run(command + "tree", [&](const std::string &line) {
      if (line.starts_with("child=")) {
        child = OpenProcess(SYNCHRONIZE, FALSE, std::stoul(line.substr(6)));
        throw std::runtime_error("intentional callback failure");
      }
    }, 5); }
  catch (const std::runtime_error &) { callbackThrown = true; }
  require(callbackThrown && child, "callback exception propagated");
  const DWORD exceptionEnded = WaitForSingleObject(child, 5000); CloseHandle(child);
  require(exceptionEnded == WAIT_OBJECT_0, "callback failure must clean descendants");
  SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
  HANDLE foreign = CreateEventW(&security, TRUE, FALSE, nullptr); require(foreign, "event fixture");
  lines.clear(); result = runner->run(command + "handle " + std::to_string(reinterpret_cast<std::uintptr_t>(foreign)), collect, 3);
  CloseHandle(foreign);
  require(result.exitCode == 0 && lines == std::vector<std::string>({"not-inherited"}), "unrelated inheritable handle must not leak");
  const auto directory = std::filesystem::temp_directory_path() /
      (L"OpenPuzzle process espa\u00e7os " + std::to_wstring(GetCurrentProcessId()));
  std::filesystem::create_directories(directory);
  const auto copied = directory / L"fixture com espa\u00e7os.exe";
  std::filesystem::copy_file(fixture, copied, std::filesystem::copy_options::overwrite_existing);
  result = runner->run("\"" + utf8(copied.wstring()) + "\" code 0", {}, 3);
  std::filesystem::remove_all(directory);
  require(result.started && result.exitCode == 0, "Unicode executable path with spaces");
  DWORD initialHandles = 0, finalHandles = 0;
  require(GetProcessHandleCount(GetCurrentProcess(), &initialHandles), "initial handle count");
  for (int i = 0; i < 20; ++i) require(runner->run(command + "code 0", {}, 3).exitCode == 0, "repeated launches");
  require(GetProcessHandleCount(GetCurrentProcess(), &finalHandles) && finalHandles <= initialHandles + 2, "launch handle leakage");
  std::cout << "WindowsProcessRunnerTests passed\n"; return 0;
 } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
