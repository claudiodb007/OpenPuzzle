#include "openpuzzle/client/ClientIdentity.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include <winioctl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace openpuzzle;
namespace fs = std::filesystem;
void require(bool condition,const char* text) { if(!condition)throw std::runtime_error(text); }
std::string read(const fs::path& path) { std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}}; }
struct Child {
  explicit Child(const fs::path& executable,const std::wstring& args,bool suspended=false) {
    std::wstring command=L"\""+executable.native()+L"\" "+args;
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION info{};
    require(CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,
        suspended?CREATE_SUSPENDED:0,nullptr,nullptr,&startup,&info),"Fixture child launch failed");
    process_=std::make_unique<windowsFile::Handle>(info.hProcess);
    thread_=std::make_unique<windowsFile::Handle>(info.hThread);
  }
  ~Child() { if(process_ && WaitForSingleObject(process_->get(),0)==WAIT_TIMEOUT) { TerminateProcess(process_->get(),99);WaitForSingleObject(process_->get(),5000); } }
  void resume() { require(ResumeThread(thread_->get())!=static_cast<DWORD>(-1),"Fixture resume failed"); }
  void finish() { require(WaitForSingleObject(process_->get(),15000)==WAIT_OBJECT_0,"Fixture timed out");DWORD code=0;require(GetExitCodeProcess(process_->get(),&code)&&code==0,"Fixture failed"); }
  bool running() { return WaitForSingleObject(process_->get(),0)==WAIT_TIMEOUT; }
  void terminate() { require(TerminateProcess(process_->get(),77),"Fixture termination failed");require(WaitForSingleObject(process_->get(),5000)==WAIT_OBJECT_0,"Fixture termination timed out"); }
private:
  std::unique_ptr<windowsFile::Handle> process_,thread_;
};
void junction(const fs::path& link,const fs::path& target) {
  require(CreateDirectoryW(link.c_str(),nullptr),"Fixture junction directory failed");
  windowsFile::Handle handle(CreateFileW(link.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr));
  require(handle.valid(),"Fixture junction open failed");
  struct Buffer { DWORD tag;WORD length,reserved,subOffset,subLength,printOffset,printLength;WCHAR names[2048]; } data{};
  const auto substitute=L"\\??\\"+target.native();const auto printable=target.native();
  require(substitute.size()+printable.size()+2<2048,"Fixture junction target too long");
  data.tag=IO_REPARSE_TAG_MOUNT_POINT;
  data.subLength=static_cast<WORD>(substitute.size()*sizeof(WCHAR));
  data.printOffset=static_cast<WORD>((substitute.size()+1)*sizeof(WCHAR));
  data.printLength=static_cast<WORD>(printable.size()*sizeof(WCHAR));
  std::copy(substitute.begin(),substitute.end(),data.names);
  std::copy(printable.begin(),printable.end(),data.names+substitute.size()+1);
  data.length=static_cast<WORD>(8+(substitute.size()+printable.size()+2)*sizeof(WCHAR));
  DWORD used=0;
  require(DeviceIoControl(handle.get(),FSCTL_SET_REPARSE_POINT,&data,data.length+8,nullptr,0,&used,nullptr),"Fixture junction creation failed");
}
int wmain(int argc,wchar_t** argv) {
  fs::path root;
  try {
    if(argc>=4) {
      const std::wstring mode=argv[1];root=argv[2];
      require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()),"Fixture LOCALAPPDATA failed");
      const auto config=localPaths::configDirectory();WorkspaceSecurity::prepare(config);
      if(mode==L"identity") {
        const auto id=client::ClientIdentity::loadOrCreate();
        require(id.size()==36,"Fixture identity failed");
        require(writePrivateFileAtomically(fs::path(argv[3]),id),"Fixture result publication failed");
        return 0;
      }
      if(mode==L"hold-lock") {
        windowsFile::Lock lock(config/L"client.id.lock");require(lock.locked(),"Fixture lock failed");
        require(writePrivateFileAtomically(fs::path(argv[3]),"ready"),"Fixture ready publication failed");
        Sleep(30000);return 0;
      }
      throw std::runtime_error("Unknown fixture mode");
    }
    require(argc==1,"Unexpected test arguments");
    root=fs::temp_directory_path()/fs::path(L"OpenPuzzle identity ç 中文 "+std::to_wstring(GetCurrentProcessId()));
    require(!fs::exists(root),"Fixture directory already exists");fs::create_directories(root);
    require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()),"Isolated LOCALAPPDATA failed");
    const auto executable=fs::absolute(argv[0]);
    const auto config=localPaths::configDirectory();WorkspaceSecurity::prepare(config);
    std::vector<std::unique_ptr<Child>> children;
    for(int i=0;i<8;++i) {
      const auto output=root/(L"result-"+std::to_wstring(i));
      children.emplace_back(std::make_unique<Child>(executable,L"identity \""+root.native()+L"\" \""+output.native()+L"\"",true));
    }
    for(auto& child:children)child->resume();
    for(auto& child:children)child->finish();
    const auto id=client::ClientIdentity::loadOrCreate();require(id.size()==36,"First-start identity failed");
    for(int i=0;i<8;++i)require(read(root/(L"result-"+std::to_wstring(i)))==id,"Concurrent first starts published different identities");
    children.clear();
    const auto ready=root/L"lock-ready";
    Child holder(executable,L"hold-lock \""+root.native()+L"\" \""+ready.native()+L"\"");
    const auto deadline=GetTickCount64()+5000;
    while(!fs::exists(ready) && holder.running() && GetTickCount64()<deadline)Sleep(10);
    require(fs::exists(ready),"Lock holder did not become ready");
    Child waiting(executable,L"identity \""+root.native()+L"\" \""+(root/L"after-stop").native()+L"\"");
    Sleep(150);require(waiting.running() && !fs::exists(root/L"after-stop"),"Identity bypassed active lock");
    holder.terminate();waiting.finish();
    require(read(root/L"after-stop")==id,"Lock owner termination changed identity");
    const auto path=config/L"client.id";const auto alias=root/L"hardlink";
    require(CreateHardLinkW(alias.c_str(),path.c_str(),nullptr),"Fixture hardlink failed");
    require(client::ClientIdentity::loadOrCreate().empty(),"Hardlinked identity accepted");
    require(read(path)==id+"\n" && read(alias)==id+"\n","Hardlinked identity modified");
    fs::remove(alias);
    const auto outside=root/L"outside";fs::create_directory(outside);
    require(writePrivateFileAtomically(outside/L"sentinel","untouched"),"Fixture sentinel failed");
    junction(config/L"redirect",outside);
    require(!writePrivateFileAtomically(config/L"redirect"/L"sentinel","bad"),"Junction write accepted");
    bool rejected=false;try { WorkspaceSecurity::prepare(config/L"redirect"); }catch(...) { rejected=true; }
    require(rejected && read(outside/L"sentinel")=="untouched","Junction target changed");
    require(RemoveDirectoryW((config/L"redirect").c_str()),"Fixture junction cleanup failed");
    fs::remove_all(root);
    std::cout<<"WindowsStorageIdentityTests passed: concurrent first starts, lock release after termination, hardlink and junction refusal\n";
    return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(!root.empty())std::cerr<<"Fixture retained: "<<pathUtf8(root)<<'\n';return 1;}
}
