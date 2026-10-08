#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WindowsProcessIdentity.hpp"
#include "openpuzzle/runtime/WindowsRuntimeRecord.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace openpuzzle;
namespace fs=std::filesystem;
using Control=ClientRuntimeControl;
using Status=Control::RuntimeIdentityStatus;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::string bytes(const fs::path& path){std::ifstream input(path,std::ios::binary);return {std::istreambuf_iterator<char>(input),{}};}
void publish(const fs::path& path,const std::string& text){require(writePrivateFileAtomically(path,text),"Fixture publication failed");}
void select(const std::string& slot){require(SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT",slot.c_str()),"Fixture slot selection failed");}
struct Child {
  explicit Child(const fs::path& exe,const fs::path& root,const std::wstring& mode,const std::string& slot,const fs::path& output,bool suspended=false){
    std::wstring command=L"\""+exe.native()+L"\" "+mode+L" \""+root.native()+L"\" "+std::wstring(slot.begin(),slot.end())+L" \""+output.native()+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION info{};
    require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,suspended?CREATE_SUSPENDED:0,nullptr,nullptr,&startup,&info),"Fixture launch failed");
    process=std::make_unique<windowsFile::Handle>(info.hProcess);thread=std::make_unique<windowsFile::Handle>(info.hThread);pid=info.dwProcessId;
  }
  ~Child(){if(running()){TerminateProcess(process->get(),99);WaitForSingleObject(process->get(),5000);}}
  bool running()const{return process && WaitForSingleObject(process->get(),0)==WAIT_TIMEOUT;}
  void resume(){require(ResumeThread(thread->get())!=static_cast<DWORD>(-1),"Fixture resume failed");}
  void finish(){require(WaitForSingleObject(process->get(),15000)==WAIT_OBJECT_0,"Fixture wait timed out");DWORD code=0;require(GetExitCodeProcess(process->get(),&code)&&code==0,"Fixture child failed");}
  void terminate(){require(TerminateProcess(process->get(),77),"Owned fixture termination failed");require(WaitForSingleObject(process->get(),5000)==WAIT_OBJECT_0,"Owned fixture termination timed out");}
  DWORD pid=0;
  std::unique_ptr<windowsFile::Handle> process,thread;
};
void waitFile(const fs::path& path){const auto end=GetTickCount64()+10000;while(!fs::exists(path)&&GetTickCount64()<end)Sleep(10);require(fs::exists(path),"Fixture readiness timed out");}
void denyRead(HANDLE handle){
  windowsFile::Security security;
  EXPLICIT_ACCESSW entries[2]{};
  for(auto& entry:entries){entry.Trustee.TrusteeForm=TRUSTEE_IS_SID;entry.Trustee.TrusteeType=TRUSTEE_IS_USER;entry.Trustee.ptstrName=reinterpret_cast<LPWSTR>(security.sid());}
  entries[0].grfAccessMode=DENY_ACCESS;entries[0].grfAccessPermissions=FILE_READ_DATA;
  entries[1].grfAccessMode=SET_ACCESS;entries[1].grfAccessPermissions=FILE_ALL_ACCESS;
  PACL acl=nullptr;require(SetEntriesInAclW(2,entries,nullptr,&acl)==ERROR_SUCCESS,"Fixture denial ACL failed");
  const auto result=SetSecurityInfo(handle,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,nullptr,nullptr,acl,nullptr);
  LocalFree(acl);require(result==ERROR_SUCCESS,"Fixture denial application failed");
}
int wmain(int argc,wchar_t** argv){
  fs::path root;
  try {
    if(argc==5){
      root=argv[2];const std::wstring mode=argv[1];const auto slot=pathUtf8(fs::path(argv[3]));const fs::path output=argv[4];
      require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()),"Child LOCALAPPDATA failed");select(slot);
      if(mode==L"foreign-release"){
        require(!Control::release(),"Foreign process released active runtime");
        require(!Control::clearSafeStop(slot),"Foreign process cleared active safe stop");
        publish(output,"denied");return 0;
      }
      require(mode==L"hold","Unknown fixture mode");
      if(!Control::acquire()){publish(output,"busy");return 0;}
      publish(output,"acquired");
      const auto deadline=GetTickCount64()+15000;
      const auto gate=root/("poll-"+slot);
      while(GetTickCount64()<deadline){
        if(fs::exists(gate) && (Control::stopRequested(slot)||Control::safeStopRequested(slot))){
          require(Control::release(),"Fixture cooperative release failed");publish(fs::path(output).concat(L".finished"),"stopped");return 0;
        }
        Sleep(10);
      }
      throw std::runtime_error("Fixture did not receive stop request");
    }
    require(argc==1,"Unexpected arguments");
    root=fs::temp_directory_path()/fs::path(L"OpenPuzzle runtime ç 中文 "+std::to_wstring(GetCurrentProcessId()));
    require(!fs::exists(root),"Fixture root must be new");fs::create_directories(root);
    require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()),"Isolated LOCALAPPDATA failed");select("primary");
    const fs::path exe=fs::absolute(argv[0]);
    const auto marker=Control::pidPath("primary");WorkspaceSecurity::prepare(marker.parent_path());
    require(Control::runtimeIdentityStatus("primary")==Status::Inactive,"Missing slot must be inactive");
    require(Control::acquire()&&Control::running("primary"),"Initial runtime acquisition failed");
    const auto own=WindowsRuntimeRecord::parse(bytes(marker));require(own && own->pid==static_cast<int>(GetCurrentProcessId()),"Runtime ownership missing");
    require(Control::runtimePid("primary")==own->pid && Control::runtimeStartTime("primary")==own->creationTime,"Published identity disagrees");
    require(Control::runtimeBootId("primary")==std::string(WindowsRuntimeRecord::platform),"Windows format discriminator missing");
    require(!Control::acquire(),"Duplicate runtime acquired occupied slot");
    const auto foreignRequest=WindowsRuntimeRecord{own->pid,own->creationTime+1}.encode();
    publish(Control::stopPath("primary"),foreignRequest);
    require(!Control::stopRequested("primary")&&!Control::requestStop("primary")&&!Control::release(),"Foreign stop identity applied to current runtime");
    require(bytes(marker)==own->encode()&&bytes(Control::stopPath("primary"))==foreignRequest,"Foreign request bytes changed");
    fs::remove(Control::stopPath("primary")); // Remove only this deliberately injected fixture.
    require(Control::requestSafeStop("primary")&&Control::safeStopRequested("primary"),"Safe stop not recorded");
    const auto safeBytes=bytes(Control::safeStopPath("primary"));
    require(Control::requestSafeStop("primary") && bytes(Control::safeStopPath("primary"))==safeBytes,"Repeated safe stop changed identity");
    {Child other(exe,root,L"foreign-release","primary",root/L"foreign");other.finish();}
    require(bytes(marker)==own->encode() && bytes(Control::safeStopPath("primary"))==safeBytes,"Foreign release altered controls");
    require(Control::requestStop("primary")&&Control::stopRequested("primary"),"Cooperative stop not recorded");
    require(Control::requestStop("primary"),"Cooperative stop not idempotent");
    require(WindowsProcessIdentity::status(own->pid,own->creationTime)==WindowsProcessIdentity::Status::Running,"Stop request forcibly ended caller");
    {
      windowsFile::Handle held(CreateFileW(marker.c_str(),READ_CONTROL|WRITE_DAC,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
      require(held.valid(),"Fixture ACL handle failed");denyRead(held.get());
      require(Control::runtimeIdentityStatus("primary")==Status::Unavailable,"Unreadable marker reported inactive");
      require(!Control::acquire() && !Control::requestStop("primary") && !Control::requestSafeStop("primary") && !Control::release(),"Unreadable identity authorized mutation");
      windowsFile::Security restore;require(restore.protect(held.get()),"Fixture ACL restoration failed");
    }
    require(bytes(marker)==own->encode() && bytes(Control::safeStopPath("primary"))==safeBytes,"Unreadable marker control data changed");
    require(Control::clearStop("primary") && Control::clearSafeStop("primary"),"Owner failed to clear requests");
    require(Control::release() && !fs::exists(marker),"Owner release failed");
    for(const auto& text:std::vector<std::string>{"incomplete\n",own->encode()+"extra",std::to_string(own->pid)+"\nlinux-boot-id\n123\n"}){
      publish(marker,text);publish(Control::safeStopPath("primary"),"preserve");
      require(Control::runtimeIdentityStatus("primary")==Status::Unavailable,"Invalid/foreign marker reported free");
      require(!Control::acquire()&&!Control::release()&&!Control::requestStop("primary"),"Invalid/foreign marker authorized control");
      require(bytes(marker)==text && bytes(Control::safeStopPath("primary"))=="preserve","Invalid/foreign data lost");
      fs::remove(marker);fs::remove(Control::safeStopPath("primary"));
    }
    publish(marker,WindowsRuntimeRecord{own->pid,own->creationTime+1}.encode());
    require(Control::runtimeIdentityStatus("primary")==Status::Inactive && !Control::requestStop("primary"),"Reused PID identity matched");
    require(WindowsProcessIdentity::status(own->pid,own->creationTime)==WindowsProcessIdentity::Status::Running,"Mismatched record terminated caller");
    require(Control::acquire() && Control::release(),"Confirmed stale marker not replaceable");
    // Eight actual processes race for one slot; the winner waits for a bound
    // request and releases cooperatively. Other contenders never clear requests.
    select("cuda-6");std::vector<std::unique_ptr<Child>> children;
    for(int i=0;i<8;++i)children.emplace_back(std::make_unique<Child>(exe,root,L"hold","cuda-6",root/("race-"+std::to_string(i)),true));
    for(auto& child:children)child->resume();
    int winners=0;
    for(int i=0;i<8;++i){waitFile(root/("race-"+std::to_string(i)));const auto result=bytes(root/("race-"+std::to_string(i)));require(result=="acquired"||result=="busy","Unexpected race result");winners+=result=="acquired";}
    require(winners==1 && Control::running("cuda-6"),"Concurrent processes acquired the same slot");
    const auto slots=Control::executionSlots();require(std::find(slots.begin(),slots.end(),"cuda-6")!=slots.end(),"Dynamic runtime slot missing");
    require(Control::requestSafeStop("cuda-6")&&Control::requestSafeStop("cuda-6"),"Winner safe stop request failed");
    publish(root/L"poll-cuda-6","go");for(auto& child:children)child->finish();children.clear();
    require(Control::runtimeIdentityStatus("cuda-6")==Status::Inactive && !fs::exists(Control::safeStopPath("cuda-6")),"Cooperative safe stop did not release slot");
    // A force-killed owned test process leaves a stale identity. Only a proven
    // stale record authorizes replacement and cleanup of its matching requests.
    select("cuda-8");
    require(client::ClientStateStore::executionSlot()=="cuda-8","Windows environment slot change was not observed");
    {
      Child crashed(exe,root,L"hold","cuda-8",root/L"crash-ready");waitFile(root/L"crash-ready");require(bytes(root/L"crash-ready")=="acquired","Crash fixture failed to acquire");
      require(Control::requestSafeStop("cuda-8"),"Crash fixture request failed");crashed.terminate();
      require(Control::runtimeIdentityStatus("cuda-8")==Status::Inactive,"Exited fixture still running");
      require(Control::acquire() && !Control::safeStopRequested("cuda-8"),"Stale fixture not recovered");
      require(Control::runtimePid("cuda-8")==static_cast<int>(GetCurrentProcessId()) && !fs::exists(Control::safeStopPath("cuda-8")),"Recovered slot retained old controls or acquired another slot");
      require(Control::release(),"Recovered slot release failed");
    }
    select("opencl-2");
    {
      Child stopped(exe,root,L"hold","opencl-2",root/L"stop-ready");waitFile(root/L"stop-ready");
      require(Control::requestStop("opencl-2") && Control::requestStop("opencl-2"),"Stop request failed");
      publish(root/L"poll-opencl-2","go");stopped.finish();
      require(bytes(root/L"stop-ready.finished")=="stopped" && !fs::exists(Control::pidPath("opencl-2")),"Cooperative stop failed to finish");
    }
    require(SetEnvironmentVariableW(L"LOCALAPPDATA",nullptr),"Fixture missing LOCALAPPDATA failed");
    require(Control::runtimeIdentityStatus("primary")==Status::Unavailable&&!Control::acquire()&&!Control::requestStop("primary"),"Missing storage used a fallback");
    require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()),"Fixture LOCALAPPDATA restore failed");
    fs::remove_all(root);
    std::cout<<"WindowsRuntimeControlTests passed: ownership, unavailable/foreign identity retention, concurrent slots, bound cooperative requests and stale recovery\n";return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';if(!root.empty())std::cerr<<"Fixture retained: "<<pathUtf8(root)<<'\n';return 1;}
}
