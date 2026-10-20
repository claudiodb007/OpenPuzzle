#pragma once
#ifdef _WIN32
#include "openpuzzle/engines/WindowsRunProfile.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include <atomic>

namespace openpuzzle::ui {
inline std::optional<std::filesystem::path> loadWindowsUiAutomaticProfile(const std::filesystem::path& root,const std::string& backend){
 if(backend!="cuda"&&backend!="opencl")return {};
 const auto directory=root/"profiles"/backend;bool missing=false;
 const auto name=windowsFile::readPrivate(directory/"automatic-selected.txt",missing,256);
 if(!name){if(missing)return {};throw std::runtime_error("Automatic profile selection unreadable; preserved");}
 if(name->size()<12||name->rfind("auto-",0)!=0||!name->ends_with(".json")||name->find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-.")!=std::string::npos)
  throw std::runtime_error("Automatic profile selection invalid; preserved");
 const auto selected=directory/pathFromUtf8(*name);const auto record=windowsFile::readPrivate(selected,missing,16384);
 if(!record)throw std::runtime_error("Saved automatic profile missing; selection preserved");
 const auto profile=WindowsRunProfile::parse(*record);
 if(profile.backend!=backend||!profile.autoTuned||(profile.gpuUuid.empty()&&profile.pciBusId.empty()))throw std::runtime_error("Saved automatic profile is not validated and physically bound");
 return selected;
}
// A remeasurement gets a new private file. Only the owned selection pointer is
// updated, so earlier measurements and the original result.json survive.
inline std::filesystem::path storeWindowsUiAutomaticProfile(const std::filesystem::path& root,const std::filesystem::path& source){
 bool missing=false;const auto contents=windowsFile::readPrivate(source,missing,16384);
 if(!contents)throw std::runtime_error("Automatic profile missing; report preserved");
 const auto profile=WindowsRunProfile::parse(*contents);
 if(!profile.autoTuned||profile.backend=="cpu"||(profile.gpuUuid.empty()&&profile.pciBusId.empty()))throw std::runtime_error("Automatic GPU profile requires validation and physical binding");
 const auto directory=root/"profiles"/profile.backend;WorkspaceSecurity::prepare(directory);
 const auto pointer=directory/"automatic-selected.txt";windowsFile::Lock lock(pointer.wstring()+L".lock");
 if(!lock.locked())throw std::runtime_error("Automatic profile publication busy; report preserved");
 (void)loadWindowsUiAutomaticProfile(root,profile.backend); // Preserve malformed/occupied selections.
 static std::atomic<unsigned long long> serial{0};
 const auto name="auto-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())+"-"+std::to_string(serial++)+".json";
 const auto target=directory/pathFromUtf8(name);
 if(!windowsFile::safePath(target)||std::filesystem::exists(target)||!writePrivateFileAtomically(target,*contents))throw std::runtime_error("Cannot save automatic profile; report preserved");
 const auto saved=windowsFile::readPrivate(target,missing,16384);
 if(!saved||*saved!=*contents||!writePrivateFileAtomically(pointer,name))throw std::runtime_error("Automatic profile selection could not be saved; measurements preserved");
 return target;
}
// Persist the first measured profile where startup already discovers it.
// Existing files, including damaged files, are never replaced.
inline std::filesystem::path storeFirstWindowsUiProfile(const std::filesystem::path& root,
 const std::filesystem::path& source){
 bool missing=false;const auto contents=windowsFile::readPrivate(source,missing,16384);
 if(!contents)throw std::runtime_error("Measured profile unavailable; diagnostic report preserved");
 const auto profile=WindowsRunProfile::parse(*contents);
 if(profile.backend!="cpu"&&profile.gpuUuid.empty()&&profile.pciBusId.empty())throw std::runtime_error("Measured GPU profile is not physically bound");
 const auto target=root/"profiles"/profile.backend/"result.json";
 if(!windowsFile::safePath(target))throw std::runtime_error("Unsafe profile destination; measured result preserved");
 const auto attrs=GetFileAttributesW(target.c_str());
 if(attrs!=INVALID_FILE_ATTRIBUTES){
  if(attrs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Occupied profile destination; preserved");
  return source;
 }
 const auto absentError=GetLastError();
 if(absentError!=ERROR_FILE_NOT_FOUND&&absentError!=ERROR_PATH_NOT_FOUND)throw std::runtime_error("Profile destination unavailable; preserved");
 WorkspaceSecurity::prepare(target.parent_path());
 static std::atomic<unsigned long long> sequence{0};
 auto temporary=target;temporary+=L".new."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64())+L"."+std::to_wstring(sequence++);
 if(!writePrivateFileAtomically(temporary,*contents))throw std::runtime_error("Could not stage measured profile; diagnostic report preserved");
 // No MOVEFILE_REPLACE_EXISTING: a concurrent file is preserved atomically.
 if(!MoveFileExW(temporary.c_str(),target.c_str(),0)){
  const auto error=GetLastError();DeleteFileW(temporary.c_str());
  if(error==ERROR_FILE_EXISTS||error==ERROR_ALREADY_EXISTS)return source;
  throw std::runtime_error("Could not save first profile; diagnostic report preserved");
 }
 const auto saved=windowsFile::readPrivate(target,missing,16384);
 if(!saved||*saved!=*contents)throw std::runtime_error("Saved profile readback failed; diagnostic report preserved");
 return target;
}
}
#endif
