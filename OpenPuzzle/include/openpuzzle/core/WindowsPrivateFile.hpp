#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <limits>
namespace openpuzzle::windowsFile {
class Handle {
  HANDLE value_;
public:
  explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : value_(value) {}
  ~Handle() { if (valid()) CloseHandle(value_); }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  HANDLE get() const { return value_; }
  bool valid() const { return value_ && value_ != INVALID_HANDLE_VALUE; }
  bool close() { const HANDLE h = value_; value_ = INVALID_HANDLE_VALUE; return CloseHandle(h) != 0; }
};
// Owner-only protected DACL, applied using the opened object handle. Handles
// and locks are never inherited. Reparse points and non-disk files are refused.
class Security {
  std::vector<unsigned char> token_;
  PACL acl_ = nullptr;
  SECURITY_DESCRIPTOR descriptor_{};
  SECURITY_ATTRIBUTES attributes_{};
public:
  Security() {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw)) return;
    Handle ownedToken(raw);
    DWORD bytes = 0;
    GetTokenInformation(raw, TokenUser, nullptr, 0, &bytes);
    if (!bytes) return;
    token_.resize(bytes);
    if (!GetTokenInformation(raw, TokenUser, token_.data(), bytes, &bytes)) return;
    EXPLICIT_ACCESSW entry{};
    entry.grfAccessPermissions = FILE_ALL_ACCESS;
    entry.grfAccessMode = SET_ACCESS;
    entry.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    entry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    entry.Trustee.TrusteeType = TRUSTEE_IS_USER;
    entry.Trustee.ptstrName = reinterpret_cast<LPWSTR>(sid());
    if (SetEntriesInAclW(1, &entry, nullptr, &acl_) != ERROR_SUCCESS) return;
    if (!InitializeSecurityDescriptor(&descriptor_, SECURITY_DESCRIPTOR_REVISION) ||
        !SetSecurityDescriptorOwner(&descriptor_, sid(), FALSE) ||
        !SetSecurityDescriptorDacl(&descriptor_, TRUE, acl_, FALSE) ||
        !SetSecurityDescriptorControl(&descriptor_, SE_DACL_PROTECTED, SE_DACL_PROTECTED)) return;
    attributes_ = {sizeof(SECURITY_ATTRIBUTES), &descriptor_, FALSE};
  }
  ~Security() { if (acl_) LocalFree(acl_); }
  Security(const Security&) = delete;
  Security& operator=(const Security&) = delete;
  PSID sid() const { return token_.empty() ? nullptr : reinterpret_cast<const TOKEN_USER*>(token_.data())->User.Sid; }
  SECURITY_ATTRIBUTES* attributes() { return attributes_.lpSecurityDescriptor ? &attributes_ : nullptr; }
  bool protect(HANDLE h, bool directory = false) {
    if (!attributes() || GetFileType(h) != FILE_TYPE_DISK) return false;
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(h, &info) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
        ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) != directory ||
        (!directory && info.nNumberOfLinks != 1)) return false;
    PSID owner = nullptr;
    PSECURITY_DESCRIPTOR existing = nullptr;
    const DWORD result = GetSecurityInfo(h, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, nullptr, nullptr, nullptr, &existing);
    const bool ours = result == ERROR_SUCCESS && owner && EqualSid(owner, sid());
    if (existing) LocalFree(existing);
    if (!ours) return false;
    return SetSecurityInfo(h, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                           nullptr, nullptr, acl_, nullptr) == ERROR_SUCCESS;
  }
};
inline bool safePath(const std::filesystem::path& path) {
  if (path.empty() || !path.is_absolute()) return false;
  // Refuse alternate streams/device spellings and traversal through junctions.
  if (path.native().find(L':', 2) != std::wstring::npos) return false;
  std::filesystem::path current = path.root_path();
  for (const auto& component : path.relative_path()) {
    if (component == L"..") return false;
    current /= component;
    const DWORD attrs = GetFileAttributesW(current.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
      const DWORD error = GetLastError();
      if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return false;
    } else if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) return false;
  }
  return true;
}
inline std::optional<std::string> readPrivate(const std::filesystem::path& path, bool& missing,
                                            std::size_t limit = (std::numeric_limits<std::size_t>::max)()) {
  missing = false;
  if (!safePath(path)) return std::nullopt;
  Security security;
  if (!security.attributes()) return std::nullopt;
  Handle input(CreateFileW(path.c_str(), GENERIC_READ | READ_CONTROL | WRITE_DAC,
                          FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                          FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr));
  if (!input.valid()) { const auto error=GetLastError(); missing = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND; return std::nullopt; }
  if (!security.protect(input.get())) return std::nullopt;
  std::string contents;
  char buffer[4096];
  for (;;) {
    DWORD count = 0;
    const auto size = static_cast<DWORD>((std::min)(sizeof(buffer), limit - contents.size()));
    if (!size) return std::nullopt; // reaching the limit is not a complete read
    if (!ReadFile(input.get(), buffer, size, &count, nullptr)) return std::nullopt;
    if (!count) return contents;
    contents.append(buffer, count);
  }
}
class Lock {
  OVERLAPPED overlapped_{};
  bool locked_ = false;
public:
  explicit Lock(const std::filesystem::path& path) {
    if (!safePath(path)) return;
    Security security;
    if (!security.attributes()) return;
    // Do not allow deletion of the lock file while any contender has it open.
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE | READ_CONTROL | WRITE_DAC,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, security.attributes(), OPEN_ALWAYS,
                           FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    // Handle is owned for the whole wait; publishing never removes this file.
    owned_.reset(new Handle(h));
    if (!security.protect(h)) return;
    locked_ = LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped_) != 0;
  }
  ~Lock() { if (locked_) UnlockFileEx(owned_->get(), 0, 1, 0, &overlapped_); }
  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;
  bool locked() const { return locked_; }
private:
  std::unique_ptr<Handle> owned_;
};
}
#endif
