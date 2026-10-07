#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cwchar>
#include <string>
int wmain(int argc, wchar_t **argv) {
  const std::wstring mode = argc > 1 ? argv[1] : L"lines";
  if (mode == L"sleep") { Sleep(20000); return 0; }
  if (mode == L"closed") {
    CloseHandle(GetStdHandle(STD_OUTPUT_HANDLE));
    CloseHandle(GetStdHandle(STD_ERROR_HANDLE));
    Sleep(20000); return 0;
  }
  if (mode == L"flood") { for (;;) { std::printf("progress\r"); std::fflush(stdout); } }
  if (mode == L"code") return argc > 2 ? static_cast<int>(std::wcstol(argv[2], nullptr, 10)) : 0;
  if (mode == L"input") {
    char ch; const DWORD result = GetFileType(GetStdHandle(STD_INPUT_HANDLE));
    DWORD read = 0;
    if (ReadFile(GetStdHandle(STD_INPUT_HANDLE), &ch, 1, &read, nullptr) && read == 0 && result) {
      std::puts("input-eof"); return 0;
    }
    return 8;
  }
  if (mode == L"tree") {
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(nullptr, path, 32768)) return 10;
    std::wstring command = L"\"" + std::wstring(path) + L"\" sleep";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    if (!CreateProcessW(path, command.data(), nullptr, nullptr, TRUE,
                         CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child)) return 11;
    std::printf("child=%lu\n", child.dwProcessId); std::fflush(stdout);
    CloseHandle(child.hThread); CloseHandle(child.hProcess);
    Sleep(20000); return 0;
  }
  if (mode == L"handle") {
    const auto value = static_cast<std::uintptr_t>(std::wcstoull(argv[2], nullptr, 10));
    DWORD flags = 0;
    std::puts(GetHandleInformation(reinterpret_cast<HANDLE>(value), &flags) ? "inherited" : "not-inherited");
    return 0;
  }
  std::printf("first\rsecond\r\nthird\n");
  std::fflush(stdout);
  std::fprintf(stderr, "stderr\n");
  std::fflush(stderr);
  std::printf("tail"); std::fflush(stdout);
  return 0;
}
