#include "openpuzzle/core/WindowsProcessRunner.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace openpuzzle {
namespace {
class Handle {
public:
  explicit Handle(HANDLE value = nullptr) : value_(value) {}
  ~Handle() { reset(); }
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
  HANDLE get() const { return value_; }
  bool valid() const { return value_ && value_ != INVALID_HANDLE_VALUE; }
  void reset(HANDLE next = nullptr) {
    if (valid()) CloseHandle(value_);
    value_ = next;
  }
private:
  HANDLE value_;
};
class Attributes {
public:
  Attributes() {
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    if (!bytes) return;
    data_.resize(bytes);
    list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(data_.data());
    ready_ = InitializeProcThreadAttributeList(list_, 1, 0, &bytes) != FALSE;
  }
  ~Attributes() { if (ready_) DeleteProcThreadAttributeList(list_); }
  bool inherit(HANDLE *handles, SIZE_T bytes) {
    return ready_ && UpdateProcThreadAttribute(list_, 0,
        PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles, bytes, nullptr, nullptr);
  }
  LPPROC_THREAD_ATTRIBUTE_LIST get() const { return list_; }
private:
  std::vector<unsigned char> data_;
  LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
  bool ready_ = false;
};
std::wstring wideCommand(const std::string &command) {
  if (command.empty() || command.find('\0') != std::string::npos ||
      command.size() > 30000) return {};
  const int size = static_cast<int>(command.size());
  const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                       command.data(), size, nullptr, 0);
  if (count <= 0) return {};
  std::wstring result(static_cast<std::size_t>(count), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, command.data(), size,
                          result.data(), count) != count) return {};
  return result;
}
bool emit(std::string &pending, const IProcessRunner::LineCallback &onLine,
          const IProcessRunner::StopPredicate &stop) {
  for (;;) {
    const auto end = pending.find_first_of("\r\n");
    if (end == std::string::npos) return false;
    const auto line = pending.substr(0, end);
    const auto next = pending.find_first_not_of("\r\n", end);
    pending.erase(0, next == std::string::npos ? pending.size() : next);
    if (!line.empty() && onLine) onLine(line);
    if (stop && stop()) return true;
  }
}
} // namespace

ProcessResult WindowsProcessRunner::run(const std::string &command,
    const LineCallback &onLine, int maxSeconds, const StopPredicate &stop) const {
  ProcessResult result;
  const auto wide = wideCommand(command);
  if (wide.empty() || (stop && stop())) return result;
  std::array<wchar_t, MAX_PATH + 1> systemDirectory{};
  const auto count = GetSystemDirectoryW(systemDirectory.data(),
                                        static_cast<UINT>(systemDirectory.size()));
  if (!count || count >= systemDirectory.size()) return result;
  const std::wstring shell = std::wstring(systemDirectory.data()) + L"\\cmd.exe";
  std::wstring invocation = L"\"" + shell + L"\" /d /s /c \"" + wide + L"\"";
  if (invocation.size() >= 32767) return result;

  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  HANDLE read = nullptr, write = nullptr;
  if (!CreatePipe(&read, &write, &security, 0)) return result;
  Handle output(read), writer(write);
  if (!SetHandleInformation(output.get(), HANDLE_FLAG_INHERIT, 0)) return result;
  Handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!input.valid()) return result;
  Handle job(CreateJobObjectW(nullptr, nullptr));
  if (!job.valid()) return result;
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                &limits, sizeof(limits))) return result;

  HANDLE inherited[] = {writer.get(), input.get()};
  Attributes attributes;
  if (!attributes.inherit(inherited, sizeof(inherited))) return result;
  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = input.get();
  startup.StartupInfo.hStdOutput = writer.get();
  startup.StartupInfo.hStdError = writer.get();
  startup.lpAttributeList = attributes.get();
  PROCESS_INFORMATION info{};
  if (!CreateProcessW(shell.c_str(), invocation.data(), nullptr, nullptr, TRUE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
        nullptr, nullptr, &startup.StartupInfo, &info)) return result;
  Handle process(info.hProcess), thread(info.hThread);
  // Never resume an uncontained command, even inside an incompatible parent job.
  if (!AssignProcessToJobObject(job.get(), process.get()) ||
      ResumeThread(thread.get()) == static_cast<DWORD>(-1)) {
    TerminateProcess(process.get(), 125);
    WaitForSingleObject(process.get(), 5000);
    return result;
  }
  writer.reset();
  input.reset();
  thread.reset();
  result.started = true;
  const auto started = std::chrono::steady_clock::now();
  std::string pending;
  bool interrupted = false;
  bool failed = false;
  bool pipeEnded = false;
  bool processEnded = false;
  try {
    for (;;) {
      // Deadline checks also run when the child is silent or emits continuously.
      interrupted = (stop && stop()) || (maxSeconds > 0 &&
          std::chrono::steady_clock::now() - started >= std::chrono::seconds(maxSeconds));
      if (interrupted) break;
      DWORD available = 0;
      if (!pipeEnded && !PeekNamedPipe(output.get(), nullptr, 0, nullptr, &available, nullptr)) {
        if (GetLastError() == ERROR_BROKEN_PIPE) pipeEnded = true;
        else { failed = true; break; }
      }
      if (available) {
        std::array<char, 4096> bytes{};
        DWORD received = 0;
        if (!ReadFile(output.get(), bytes.data(),
                      (std::min)(available, static_cast<DWORD>(bytes.size())), &received, nullptr)) {
          if (GetLastError() == ERROR_BROKEN_PIPE) pipeEnded = true;
          else { failed = true; break; }
        } else if (received) {
          pending.append(bytes.data(), received);
          if (emit(pending, onLine, stop)) { interrupted = true; break; }
        }
      }
      const DWORD wait = WaitForSingleObject(process.get(), 0);
      if (wait == WAIT_FAILED) { failed = true; break; }
      processEnded = wait == WAIT_OBJECT_0;
      if (processEnded && pipeEnded) break;
      // A child may close output while still running. Continue waiting/checking stop.
      if (!available) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (interrupted || failed) {
      if (!TerminateJobObject(job.get(), interrupted ? 124 : 125)) failed = true;
      if (WaitForSingleObject(process.get(), 5000) != WAIT_OBJECT_0) failed = true;
    }
    if (!pending.empty() && onLine) onLine(pending);
  } catch (...) {
    TerminateJobObject(job.get(), 125);
    WaitForSingleObject(process.get(), 5000);
    throw;
  }
  DWORD exitCode = 0;
  if (failed || !GetExitCodeProcess(process.get(), &exitCode)) result.exitCode = -1;
  else result.exitCode = interrupted ? 124 : static_cast<int>(exitCode);
  return result;
}
} // namespace openpuzzle
