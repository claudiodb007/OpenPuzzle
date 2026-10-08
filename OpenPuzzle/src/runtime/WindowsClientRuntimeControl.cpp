#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WindowsProcessIdentity.hpp"
#include "openpuzzle/runtime/WindowsRuntimeRecord.hpp"
#include "openpuzzle/runtime/ExecutionSlot.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include <limits>
#include <system_error>

namespace openpuzzle {
namespace {
using Record = WindowsRuntimeRecord;
using Status = ClientRuntimeControl::RuntimeIdentityStatus;
namespace fs = std::filesystem;
struct Read {
  bool missing = false;
  std::optional<Record> record;
};
Read read(const fs::path& path) {
  Read result;
  const auto bytes = windowsFile::readPrivate(path, result.missing, 129);
  if (bytes) result.record = Record::parse(*bytes);
  return result;
}
Status status(const Read& marker) {
  if (marker.missing) return Status::Inactive;
  if (!marker.record) return Status::Unavailable;
  switch (WindowsProcessIdentity::status(marker.record->pid, marker.record->creationTime)) {
    case WindowsProcessIdentity::Status::Inactive: return Status::Inactive;
    case WindowsProcessIdentity::Status::Running: return Status::Running;
    default: return Status::Unavailable;
  }
}
std::optional<Record> self() {
  const auto pid = GetCurrentProcessId();
  if (pid > static_cast<DWORD>((std::numeric_limits<int>::max)())) return std::nullopt;
  const auto creation = WindowsProcessIdentity::creationTime(pid);
  if (!creation || !*creation) return std::nullopt;
  return Record{static_cast<int>(pid), *creation};
}
fs::path controlPath(const std::string& prefix, const std::string& slot, const std::string& extension) {
  return localPaths::dataDirectory() / (prefix + ExecutionSlot::fileSuffix(slot) + extension);
}
fs::path lockPath(const std::string& slot) {
  return fs::path(ClientRuntimeControl::pidPath(slot)).concat(L".lock");
}
// A stable per-slot lock serializes cooperating publishers/controllers. Refuse
// unreadable or foreign request bytes instead of treating them as disposable.
bool removeRequest(const fs::path& path, const std::optional<Record>& permitted) {
  const auto request = read(path);
  if (request.missing) return true;
  if (!request.record || !permitted || *request.record != *permitted) return false;
  std::error_code error;
  const bool removed = fs::remove(path, error);
  return removed && !error;
}
bool clearRequest(const std::string& slot, bool safe) {
  try {
    WorkspaceSecurity::prepare(ClientRuntimeControl::pidPath(slot).parent_path());
    windowsFile::Lock lock(lockPath(slot));
    if (!lock.locked()) return false;
    const auto marker = read(ClientRuntimeControl::pidPath(slot));
    const auto own = self();
    const auto path = safe ? ClientRuntimeControl::safeStopPath(slot) : ClientRuntimeControl::stopPath(slot);
    const auto request = read(path);
    if (request.missing) return true;
    if (!own || !marker.record || *marker.record != *own) return false;
    return removeRequest(path, own);
  } catch (...) { return false; }
}
bool request(const std::string& slot, bool safe) {
  try {
    WorkspaceSecurity::prepare(ClientRuntimeControl::pidPath(slot).parent_path());
    windowsFile::Lock lock(lockPath(slot));
    if (!lock.locked()) return false;
    const auto marker = read(ClientRuntimeControl::pidPath(slot));
    if (status(marker) != Status::Running) return false;
    const auto path = safe ? ClientRuntimeControl::safeStopPath(slot) : ClientRuntimeControl::stopPath(slot);
    const auto previous = read(path);
    if (!previous.missing) return previous.record && *previous.record == *marker.record;
    return writePrivateFileAtomically(path, marker.record->encode());
  } catch (...) { return false; }
}
bool requested(const std::string& slot, bool safe) {
  try {
    const auto marker = read(ClientRuntimeControl::pidPath(slot));
    const auto path = safe ? ClientRuntimeControl::safeStopPath(slot) : ClientRuntimeControl::stopPath(slot);
    const auto request = read(path);
    return marker.record && request.record && *marker.record == *request.record && status(marker) == Status::Running;
  } catch (...) { return false; }
}
std::optional<Record> record(const std::string& slot) {
  try { return read(ClientRuntimeControl::pidPath(slot)).record; }
  catch (...) { return std::nullopt; }
}
std::string currentSlot() { return client::ClientStateStore::executionSlot(); }
} // namespace

std::filesystem::path ClientRuntimeControl::pidPath() { return pidPath(currentSlot()); }
std::filesystem::path ClientRuntimeControl::pidPath(const std::string& slot) { return controlPath("runtime", slot, ".pid"); }
std::filesystem::path ClientRuntimeControl::safeStopPath() { return safeStopPath(currentSlot()); }
std::filesystem::path ClientRuntimeControl::safeStopPath(const std::string& slot) { return controlPath("safestop", slot, ".requested"); }
std::filesystem::path ClientRuntimeControl::stopPath(const std::string& slot) { return controlPath("stop", slot, ".requested"); }
std::optional<int> ClientRuntimeControl::runtimePid() { return runtimePid(currentSlot()); }
std::optional<int> ClientRuntimeControl::runtimePid(const std::string& slot) { const auto r=record(slot);return r?std::optional(r->pid):std::nullopt; }
std::optional<std::string> ClientRuntimeControl::runtimeBootId() { return runtimeBootId(currentSlot()); }
std::optional<std::string> ClientRuntimeControl::runtimeBootId(const std::string& slot) {
  // The legacy method name is retained for callers; on Windows this is a format
  // discriminator, never a fabricated Linux boot identifier.
  return record(slot) ? std::optional(std::string(Record::platform)) : std::nullopt;
}
std::optional<std::uint64_t> ClientRuntimeControl::runtimeStartTime() { return runtimeStartTime(currentSlot()); }
std::optional<std::uint64_t> ClientRuntimeControl::runtimeStartTime(const std::string& slot) { const auto r=record(slot);return r?std::optional(r->creationTime):std::nullopt; }
bool ClientRuntimeControl::running() { return running(currentSlot()); }
bool ClientRuntimeControl::running(const std::string& slot) { return runtimeIdentityStatus(slot)==Status::Running; }
ClientRuntimeControl::RuntimeIdentityStatus ClientRuntimeControl::runtimeIdentityStatus(const std::string& slot) {
  try { return status(read(pidPath(slot))); } catch (...) { return Status::Unavailable; }
}
std::vector<std::string> ClientRuntimeControl::executionSlots() {
  std::vector<std::string> slots={"primary","gpu","cpu","cuda","opencl"};
  try { const auto dynamic=ExecutionSlot::discoverGpuSlots(localPaths::dataDirectory());slots.insert(slots.end(),dynamic.begin(),dynamic.end()); }catch(...) {}
  return slots;
}
bool ClientRuntimeControl::acquire() {
  try {
    const auto slot=currentSlot();const auto path=pidPath(slot);
    WorkspaceSecurity::prepare(path.parent_path());
    windowsFile::Lock lock(lockPath(slot));
    if (!lock.locked()) return false;
    const auto previous=read(path);
    if (status(previous)!=Status::Inactive) return false;
    const auto own=self();if(!own)return false;
    if (!removeRequest(stopPath(slot),previous.record) || !removeRequest(safeStopPath(slot),previous.record)) return false;
    return writePrivateFileAtomically(path,own->encode());
  }catch(...) { return false; }
}
bool ClientRuntimeControl::release() {
  try {
    const auto slot=currentSlot();const auto path=pidPath(slot);
    WorkspaceSecurity::prepare(path.parent_path());
    windowsFile::Lock lock(lockPath(slot));if(!lock.locked())return false;
    const auto marker=read(path);if(marker.missing)return true;
    const auto own=self();if(!own || !marker.record || *marker.record!=*own)return false;
    if (!removeRequest(stopPath(slot),own) || !removeRequest(safeStopPath(slot),own))return false;
    std::error_code error;return fs::remove(path,error) && !error;
  }catch(...) { return false; }
}
bool ClientRuntimeControl::requestStop() { return requestStop(currentSlot()); }
bool ClientRuntimeControl::requestStop(const std::string& slot) { return request(slot,false); }
bool ClientRuntimeControl::requestSafeStop() { return requestSafeStop(currentSlot()); }
bool ClientRuntimeControl::requestSafeStop(const std::string& slot) { return request(slot,true); }
bool ClientRuntimeControl::safeStopRequested() { return safeStopRequested(currentSlot()); }
bool ClientRuntimeControl::safeStopRequested(const std::string& slot) { return requested(slot,true); }
bool ClientRuntimeControl::clearSafeStop() { return clearSafeStop(currentSlot()); }
bool ClientRuntimeControl::clearSafeStop(const std::string& slot) { return clearRequest(slot,true); }
bool ClientRuntimeControl::stopRequested(const std::string& slot) { return requested(slot,false); }
bool ClientRuntimeControl::clearStop(const std::string& slot) { return clearRequest(slot,false); }
} // namespace openpuzzle
