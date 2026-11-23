#include "NativeRunFiles.hpp"
#include "TargetInput.hpp"

#include <fstream>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

template<class Operation>
void requireRefusal(Operation operation, const char* message) {
  try {
    operation();
  } catch (const std::runtime_error&) {
    return;
  }
  throw std::runtime_error(message);
}

// Use the same suspended-child and Job Object pattern as WindowsEngineProcess.
void verifyChild(const wchar_t* executable, bool contained, DWORD owner,
                 DWORD expectedExit) {
  using openpuzzle::windowsFile::Handle;
  Handle job(CreateJobObjectW(nullptr, nullptr));
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  require(job.valid() && SetInformationJobObject(
      job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)),
      "Cannot create fixture job");
  auto command = std::wstring(L"\"") + executable + L"\" --owner " + std::to_wstring(owner);
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION created{};
  require(CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE,
                         CREATE_SUSPENDED, nullptr, nullptr, &startup, &created),
          "Cannot launch fixture child");
  Handle child(created.hProcess), thread(created.hThread);
  if ((contained && !AssignProcessToJobObject(job.get(), child.get())) ||
      ResumeThread(thread.get()) == static_cast<DWORD>(-1)) {
    TerminateProcess(child.get(), 99);
    throw std::runtime_error("Cannot start fixture child");
  }
  if (WaitForSingleObject(child.get(), 5000) != WAIT_OBJECT_0) {
    TerminateProcess(child.get(), 99);
    throw std::runtime_error("Fixture child did not exit");
  }
  DWORD code = 0;
  require(GetExitCodeProcess(child.get(), &code), "Cannot read fixture exit code");
  if (code != expectedExit) {
    throw std::runtime_error("Fixture expected exit " + std::to_string(expectedExit) +
                             ", received " + std::to_string(code));
  }
}
}

int wmain(int argc, wchar_t** argv) {
  if (argc == 3 && std::wstring(argv[1]) == L"--owner") {
    try {
      const opvNative::Owner owner(std::stoul(argv[2]));
      return owner.alive() ? 0 : 3;
    } catch (const std::exception& error) {
      std::cerr << error.what() << '\n';
      return 2;
    }
  }
  try {
    namespace fs = std::filesystem;
    if (argc == 2 && std::wstring(argv[1]) == L"--ownership") {
      const auto executable = fs::absolute(argv[0]);
      verifyChild(executable.c_str(), false, GetCurrentProcessId(), 2);
      verifyChild(executable.c_str(), true, 4, 2);
      verifyChild(executable.c_str(), true, GetCurrentProcessId(), 0);
      std::cout << "Vanity Windows process ownership passed\n";
      return 0;
    }
    require(argc == 2 && std::wstring(argv[1]) == L"--files", "Expected --files or --ownership");
    const auto workspace = fs::temp_directory_path() /
        (L"openpuzzle-vanity-test-" + std::to_wstring(GetCurrentProcessId()) +
         L"-" + std::to_wstring(GetTickCount64()));
    require(fs::create_directory(workspace), "Cannot create fixture workspace");
    const struct Cleanup {
      fs::path path;
      ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); }
    } cleanup{workspace};

    const auto target = workspace / L"alvo-\u00e7.txt";
    {
      std::ofstream file(target, std::ios::binary);
      file << "OPV-TARGETS-1\r\nhash160 0000000000000000000000000000000000000000\r\n";
    }
    require(opvTargets::load(target).size() == 1, "UTF-16 target path or CRLF refused");
    requireRefusal([&]{ opvNative::readTargets(L"relative.txt"); }, "Relative target accepted");
    requireRefusal([&]{ opvNative::readTargets(workspace); }, "Directory accepted as target");
    const auto oversized = workspace / L"oversized.txt";
    {
      std::ofstream file(oversized, std::ios::binary);
      file << std::string(8193, 'a');
    }
    requireRefusal([&]{ opvNative::readTargets(oversized); }, "Target size bound missing");
    requireRefusal([&]{ opvNative::Results result(target); }, "Existing file overwritten");
    const auto results = workspace / L"resultado-\u00e7.txt";
    {
      opvNative::Results file(results);
      file.append("synthetic record\n");
      requireRefusal([&]{ opvNative::readTargets(results); }, "Writer allowed concurrent reading");
    }
    require(opvNative::readTargets(results) == "synthetic record\n", "Result not preserved");
    requireRefusal([&]{ opvNative::Results file(results); }, "Existing result overwritten");
    std::cout << "Vanity Windows native files passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
