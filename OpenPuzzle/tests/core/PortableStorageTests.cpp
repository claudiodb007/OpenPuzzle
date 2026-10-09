#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/JsonString.hpp"
#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/client/ClientIdentity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/ExecutionPersistence.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/core/RecoveryManager.hpp"
#include <atomic>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif
namespace fs = std::filesystem;
using namespace openpuzzle;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::string read(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
void expectPrivate(const fs::path& path) {
#ifdef _WIN32
  windowsFile::Handle file(CreateFileW(path.c_str(), READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  require(file.valid(), "Unable to query file ACL");
  PACL acl = nullptr; PSECURITY_DESCRIPTOR descriptor = nullptr;
  require(GetSecurityInfo(file.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
      nullptr, nullptr, &acl, nullptr, &descriptor) == ERROR_SUCCESS, "Unable to read DACL");
  SECURITY_DESCRIPTOR_CONTROL control = 0; DWORD revision = 0;
  const bool protectedAcl = GetSecurityDescriptorControl(descriptor, &control, &revision) && (control & SE_DACL_PROTECTED);
  windowsFile::Security security;
  void* raw = nullptr;
  const bool privateAcl = acl && acl->AceCount == 1 && GetAce(acl, 0, &raw) &&
      static_cast<ACE_HEADER*>(raw)->AceType == ACCESS_ALLOWED_ACE_TYPE &&
      EqualSid(&static_cast<ACCESS_ALLOWED_ACE*>(raw)->SidStart, security.sid());
  LocalFree(descriptor);
  require(protectedAcl && privateAcl, "DACL must be protected and allow only the current user");
#else
  const auto expected = fs::is_directory(path) ? fs::perms::owner_all : fs::perms::owner_read | fs::perms::owner_write;
  require(fs::status(path).permissions() == expected, "Unexpected private permissions");
#endif
}
struct Environment {
#ifdef _WIN32
  std::wstring old;
  bool existed;
  Environment(const fs::path& root) {
    const DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);existed=n!=0;
    if(n) { old.resize(n);const auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",old.data(),n);old.resize(count); }
    require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()),"Unable to set isolated LOCALAPPDATA");
  }
  ~Environment() { SetEnvironmentVariableW(L"LOCALAPPDATA",existed?old.c_str():nullptr); }
#else
  std::string old;
  bool existed;
  Environment(const fs::path& root) {
    const auto p=std::getenv("HOME");existed=p!=nullptr;if(p)old=p;
    require(setenv("HOME",root.c_str(),1)==0,"Unable to set isolated HOME");
  }
  ~Environment() { if(existed)setenv("HOME",old.c_str(),1);else unsetenv("HOME"); }
#endif
};
int main() {
  fs::path root;
  try {
#ifdef _WIN32
    const auto pid=GetCurrentProcessId();
#else
    const auto pid=getpid();
#endif
    root=fs::temp_directory_path()/pathFromUtf8("OpenPuzzle armazenamento ç 中文 "+std::to_string(pid));
    require(!fs::exists(root),"Test workspace must be new");
    fs::create_directories(root);
    Environment environment(root);
    const auto config=localPaths::configDirectory();
    const auto data=localPaths::dataDirectory();
#ifdef _WIN32
    require(config==root/L"OpenPuzzle"/L"config" && data==root/L"OpenPuzzle"/L"data","Unexpected Windows storage paths");
#else
    require(config==root/".config/OpenPuzzle" && data==root/".local/share/OpenPuzzle","Linux paths changed");
#endif
    require(!client::ClientStateStore::load("primary") &&
            !client::ClientStateStore::load("cuda-7"),
            "Absent client state unexpectedly loaded");
    require(!fs::exists(config) && !fs::exists(data),
            "Reading absent client state created storage directories");
    WorkspaceSecurity::prepare(config);expectPrivate(config);
    const auto file=config/pathFromUtf8("ficheiro ç 中文.txt");
    const std::string bytes = std::string("quotes \\\nç 中文") + '\0' + "binary";
    require(writePrivateFileAtomically(file,bytes),"First private atomic write failed");
    require(read(file)==bytes,"Binary/Unicode data changed");expectPrivate(file);
    require(writePrivateFileAtomically(file,"updated\n"),"Atomic replacement failed");
    require(read(file)=="updated\n","Replacement content missing");
    const auto blocked=config/"directory-target";fs::create_directory(blocked);
    require(!writePrivateFileAtomically(blocked,"bad"),"Directory target accepted");
    require(fs::is_directory(blocked),"Directory target altered");
    require(!writePrivateFileAtomically(config/"missing"/"file","bad"),"Missing parent accepted");
#ifdef _WIN32
    windowsFile::Handle held(CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    require(held.valid(),"Unable to simulate sharing violation");
    require(!writePrivateFileAtomically(file,"must not publish"),"Sharing violation accepted");
    require(read(file)=="updated\n","Previous bytes lost after publication failure");
    require(held.close(),"Unable to release blocked destination");
    require(!writePrivateFileAtomically(fs::path(file).concat(L":stream"),"bad"),"Alternate stream accepted");
    require(!fs::exists(fs::path(file).concat(L":stream")),"Alternate stream created");
#endif
    for(const auto& entry:fs::directory_iterator(config)) require(pathUtf8(entry.path().filename()).find(".tmp.")==std::string::npos,"Temporary file leaked");
    std::atomic<int> writes{0};std::vector<std::thread> threads;
    for(int i=0;i<8;++i)threads.emplace_back([&,i]{if(writePrivateFileAtomically(file,std::string(65536,static_cast<char>('a'+i))))++writes;});
    for(auto& thread:threads)thread.join();
    const auto final=read(file);
    require(writes>0 && final.size()==65536 && final.find_first_not_of(final.front())==std::string::npos,"Concurrent atomic writes mixed content");
    const auto identityPath=config/"client.id";
    require(writePrivateFileAtomically(identityPath,"incomplete\n"),"Unable to create invalid identity");
    require(client::ClientIdentity::loadOrCreate().empty(),"Invalid identity regenerated");
    require(read(identityPath)=="incomplete\n","Invalid identity was modified");
    fs::remove(identityPath);
    const auto identity=client::ClientIdentity::loadOrCreate();
    require(identity.size()==36 && identity[14]=='4',"UUID creation failed");
    expectPrivate(identityPath);expectPrivate(fs::path(identityPath).concat(".lock"));
    std::vector<std::string> ids(8);threads.clear();
    for(int i=0;i<8;++i)threads.emplace_back([&,i]{ids[i]=client::ClientIdentity::loadOrCreate();});
    for(auto& thread:threads)thread.join();
    for(const auto& value:ids)require(value==identity,"Concurrent identity changed");
    require(writePrivateFileAtomically(identityPath,identity+"\r\n"),"Unable to write CRLF identity");
    require(client::ClientIdentity::loadOrCreate()==identity,"CRLF identity changed");
    Configuration settings;settings.engine.executable=pathUtf8(root/pathFromUtf8("motores ç 中文/engine.exe"));
    settings.gpu.device=7;settings.gpu.thermal.warningC=74.5;
    require(ConfigurationManager::save(settings),"Configuration save failed");
    const auto loaded=ConfigurationManager::loadChecked();
    require(loaded && loaded->engine.executable==settings.engine.executable && loaded->gpu.device==7 && loaded->gpu.thermal.warningC==74.5,"Configuration round trip failed");
    const auto configFile=pathFromUtf8(ConfigurationManager::configPath());expectPrivate(configFile);
    const auto legacyDirectory = root/pathFromUtf8("motores ç 中文");
    require(writePrivateFileAtomically(configFile,"{\"cuda\":\""+escapeJsonString(pathUtf8(legacyDirectory/"cuBitCrack.exe"))+"\"}"),"Unable to prepare legacy engine path");
    const auto legacy = ConfigurationManager::loadChecked();
#ifdef _WIN32
    const auto openclFilename = "clBitCrack.exe";
#else
    const auto openclFilename = "clBitCrack";
#endif
    require(legacy && legacy->bitcrack.openclPath==pathUtf8(legacyDirectory/openclFilename),"Legacy Unicode engine path changed");
    require(writePrivateFileAtomically(configFile,"{broken"),"Unable to prepare invalid configuration");
    require(!ConfigurationManager::loadChecked(),"Invalid configuration accepted");
    require(read(configFile)=="{broken","Invalid configuration modified");
    const auto workspace=data/"jobs"/"00000042";WorkspaceSecurity::prepare(workspace);
    ExecutionContext context;context.jobId=42;context.puzzleId=71;context.rangeId=123;
    context.workspace=pathUtf8(workspace);context.command="quoted \\\" command ç 中文\n";context.engine="bitcrack";
    ExecutionPersistence persistence;persistence.writeExecutionFile(context);
    ExecutionResult result;result.exitCode=0;result.linesRead=12;result.averageSpeed=1.25;
    persistence.writeStateFile(context,"FINISHED",result);
    RecoveryManager recovery{WorkspaceManager(data)};
    const auto restored=recovery.buildExecutionContext(42);const auto state=recovery.load(42);
    require(restored.workspace==context.workspace && restored.command==context.command && state.exitCode==0 && state.linesRead==12,"Execution recovery round trip failed");
    client::ClientExecutionState clientState;clientState.active=true;clientState.assignmentId="fixture";clientState.clientId=identity;
    clientState.puzzle=71;clientState.rangeId=123;clientState.pid=1;clientState.workspace=pathUtf8(workspace);clientState.command=context.command;
    require(client::ClientStateStore::save(clientState,"cuda-7"),"Client state save failed");
    const auto restoredState=client::ClientStateStore::load("cuda-7");
    require(restoredState && restoredState->workspace==clientState.workspace && restoredState->command==clientState.command,"Client state round trip failed");
    require(client::ClientStateStore::path("cuda-7").parent_path()==data,"Client state path escaped storage root");
    require(client::ClientStateStore::remove("cuda-7"),"Client state removal failed");
#ifdef _WIN32
    require(SetEnvironmentVariableW(L"LOCALAPPDATA",nullptr),"Unable to test unavailable LOCALAPPDATA");
    require(!ConfigurationManager::loadChecked() && !ConfigurationManager::save(settings),"Missing LOCALAPPDATA used fallback storage");
    require(client::ClientIdentity::loadOrCreate().empty(),"Missing LOCALAPPDATA created identity elsewhere");
    require(!client::ClientStateStore::save(clientState,"cuda-7") && !client::ClientStateStore::load("cuda-7"),"Missing LOCALAPPDATA accessed state elsewhere");
    require(SetEnvironmentVariableW(L"LOCALAPPDATA",L"relative-folder"),"Unable to test relative LOCALAPPDATA");
    require(!ConfigurationManager::loadChecked() && !ConfigurationManager::save(settings),"Relative LOCALAPPDATA accepted");
    require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()),"Unable to restore isolated LOCALAPPDATA");
#endif
    fs::remove_all(root);
    std::cout<<"PortableStorageTests passed: paths, Unicode, private access, atomic replacement, failures, concurrency, identity and round trips\n";
    return 0;
  } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';if(!root.empty())std::cerr<<"Fixture retained for diagnosis: "<<pathUtf8(root)<<'\n';return 1; }
}
