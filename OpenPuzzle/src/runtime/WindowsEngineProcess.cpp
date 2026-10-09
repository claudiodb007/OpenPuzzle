#include "openpuzzle/runtime/WindowsEngineProcess.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/core/WindowsCommandLine.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include <atomic>
#include <limits>
#include <stdexcept>
#include <thread>
#include <chrono>
namespace openpuzzle {
namespace {
namespace fs=std::filesystem;
using Handle=windowsFile::Handle;
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
std::wstring wide(const std::string& text){
  require(text.size()<=131068 && text.find('\0')==std::string::npos,"Invalid Windows command bytes");
  auto size=static_cast<int>(text.size());
  int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),size,nullptr,0);
  require(n>0 && n<32767,"Windows command is invalid UTF-8 or too long");
  std::wstring out(n,L'\0');
  require(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),size,out.data(),n)==n,"Windows command conversion failed");
  return out;
}
class Attributes {
  std::vector<unsigned char> bytes_;
  LPPROC_THREAD_ATTRIBUTE_LIST list_=nullptr;
  bool ready_=false;
public:
  Attributes(HANDLE* handles,SIZE_T count){
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    if(!bytes)return;
    bytes_.resize(bytes);list_=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(bytes_.data());
    ready_=InitializeProcThreadAttributeList(list_,1,0,&bytes)!=0;
    if(ready_&&!UpdateProcThreadAttribute(list_,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,count*sizeof(HANDLE),nullptr,nullptr)){
      DeleteProcThreadAttributeList(list_);ready_=false;
    }
  }
  ~Attributes(){if(ready_)DeleteProcThreadAttributeList(list_);}
  LPPROC_THREAD_ATTRIBUTE_LIST get()const{return ready_?list_:nullptr;}
};
fs::path lockPath(const fs::path& root){return root/L"engine.windows.lock";}
bool jobEmpty(HANDLE job){
  JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
  return QueryInformationJobObject(job,JobObjectBasicAccountingInformation,&info,sizeof(info),nullptr)&&info.ActiveProcesses==0;
}
} // namespace
struct WindowsEngineProcess::Impl {
  std::unique_ptr<Handle> job,process,log;
  fs::path workspace,generation;
  WindowsEngineRecord record;
  std::optional<std::uint32_t> finished;
  ~Impl(){
    if(job&&job->valid())TerminateJobObject(job->get(),125);
    if(process&&process->valid())WaitForSingleObject(process->get(),5000);
  }
};
WindowsEngineProcess::WindowsEngineProcess(std::unique_ptr<Impl> impl):impl_(std::move(impl)){}
WindowsEngineProcess::~WindowsEngineProcess(){try{if(impl_)stop(125);}catch(...) {}}
fs::path WindowsEngineProcess::identityPath(const fs::path& workspace){return workspace/L"engine.windows.identity";}
fs::path WindowsEngineProcess::generationPath(const fs::path& workspace,const WindowsEngineRecord& record){
  require(record.valid(),"Invalid Windows engine generation");return workspace/pathFromUtf8("engine-"+record.generation);
}
WindowsEngineProcess::Recovery WindowsEngineProcess::recover(const fs::path& workspace){
  Recovery out;
  try{
    bool missing=false;auto text=windowsFile::readPrivate(identityPath(workspace),missing,225);
    if(missing){out.status=WindowsProcessIdentity::Status::Inactive;return out;}
    if(!text)return out;
    out.record=WindowsEngineRecord::parse(*text);if(!out.record)return out;
    out.status=WindowsProcessIdentity::status(out.record->process.pid,out.record->process.creationTime);
    if(out.status==WindowsProcessIdentity::Status::Inactive){
      auto completion=windowsFile::readPrivate(generationPath(workspace,*out.record)/L"exit.windows",missing,241);
      if(completion)out.exitCode=WindowsEngineRecord::completionCode(*completion,*out.record);
      out.completionUnavailable=!missing&&!out.exitCode;
    }
  }catch(...){}
  return out;
}
std::unique_ptr<WindowsEngineProcess> WindowsEngineProcess::start(const fs::path& executable,
    const std::vector<std::string>& arguments,const fs::path& workspace,
    const std::function<bool(const WindowsEngineRecord&)>& publishState){
  require(executable.is_absolute()&&fs::is_regular_file(executable),"Windows engine executable must be an existing absolute file");
  require(workspace.is_absolute()&&windowsFile::safePath(workspace),"Windows engine workspace must be an absolute private path");
  auto command=wide(WindowsCommandLine::build(pathUtf8(executable),arguments));
  WorkspaceSecurity::prepare(workspace);
  windowsFile::Lock lock(lockPath(workspace));require(lock.locked(),"Could not lock Windows engine workspace");
  require(recover(workspace).status==WindowsProcessIdentity::Status::Inactive,"Windows engine identity is active or unavailable; preserved");
  auto impl=std::make_unique<Impl>();impl->workspace=workspace;
  static std::atomic<unsigned long long> sequence{0};
  impl->record.generation=std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64())+'-'+std::to_string(sequence++);
  impl->generation=workspace/pathFromUtf8("engine-"+impl->record.generation);
  windowsFile::Security security;require(security.attributes()!=nullptr,"Private engine security unavailable");
  require(CreateDirectoryW(impl->generation.c_str(),security.attributes())!=0,"Could not create a new engine generation");
  WorkspaceSecurity::prepare(impl->generation);
  security.attributes()->bInheritHandle=TRUE;
  impl->log=std::make_unique<Handle>(CreateFileW((impl->generation/L"engine.log").c_str(),GENERIC_WRITE|READ_CONTROL|WRITE_DAC,
      FILE_SHARE_READ,security.attributes(),CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
  require(impl->log->valid()&&security.protect(impl->log->get()),"Could not create a private engine log");
  SECURITY_ATTRIBUTES inherited{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
  Handle input(CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&inherited,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
  require(input.valid(),"Could not create engine EOF input");
  impl->job=std::make_unique<Handle>(CreateJobObjectW(nullptr,nullptr));require(impl->job->valid(),"Could not create engine Job Object");
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  require(SetInformationJobObject(impl->job->get(),JobObjectExtendedLimitInformation,&limits,sizeof(limits)),"Could not contain Windows engine tree");
  HANDLE selected[]={impl->log->get(),input.get()};Attributes attributes(selected,2);require(attributes.get()!=nullptr,"Could not restrict engine handle inheritance");
  STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput=input.get();startup.StartupInfo.hStdOutput=impl->log->get();startup.StartupInfo.hStdError=impl->log->get();startup.lpAttributeList=attributes.get();
  PROCESS_INFORMATION created{};
  require(CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,
      CREATE_SUSPENDED|CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,workspace.c_str(),&startup.StartupInfo,&created),"Could not create suspended Windows engine");
  impl->process=std::make_unique<Handle>(created.hProcess);Handle thread(created.hThread);
  // A suspended child must also be terminated if job assignment fails.
  if(!AssignProcessToJobObject(impl->job->get(),impl->process->get())){
    TerminateProcess(impl->process->get(),125);WaitForSingleObject(impl->process->get(),5000);
    throw std::runtime_error("Windows engine could not join its owned Job Object");
  }
  FILETIME creation{},exit{},kernel{},user{};
  require(created.dwProcessId<=static_cast<DWORD>((std::numeric_limits<int>::max)())&&
      GetProcessTimes(impl->process->get(),&creation,&exit,&kernel,&user),"Could not capture Windows engine identity");
  impl->record.process={static_cast<int>(created.dwProcessId),(static_cast<std::uint64_t>(creation.dwHighDateTime)<<32)|creation.dwLowDateTime};
  require(impl->record.valid(),"Windows engine identity is incomplete");
  // Publish a complete identity while the child is still suspended; fast engines
  // cannot disappear before their generation is recorded.
  require(writePrivateFileAtomically(identityPath(workspace),impl->record.encode()),"Could not publish engine identity; previous marker preserved");
  // Persist the assignment while the contained child is still suspended.
  // Publication failure terminates that child without executing its first instruction.
  if(publishState)require(publishState(impl->record),"Could not persist Windows assignment; engine was not resumed");
  auto result=std::unique_ptr<WindowsEngineProcess>(new WindowsEngineProcess(std::move(impl)));
  if(ResumeThread(thread.get())==static_cast<DWORD>(-1)){
    result->stop(125);throw std::runtime_error("Could not resume contained Windows engine");
  }
  return result;
}
const WindowsEngineRecord& WindowsEngineProcess::identity()const{return impl_->record;}
fs::path WindowsEngineProcess::logPath()const{return impl_->generation/L"engine.log";}
std::optional<std::uint32_t> WindowsEngineProcess::wait(std::uint32_t milliseconds){
  if(impl_->finished)return impl_->finished;
  require(milliseconds!=INFINITE,"An infinite engine wait would prevent runtime control polling");
  auto waited=WaitForSingleObject(impl_->process->get(),milliseconds);
  if(waited==WAIT_TIMEOUT)return std::nullopt;
  require(waited==WAIT_OBJECT_0,"Could not determine Windows engine termination");
  DWORD code=0;require(GetExitCodeProcess(impl_->process->get(),&code),"Could not read Windows engine exit code");
  if(!jobEmpty(impl_->job->get()))require(TerminateJobObject(impl_->job->get(),125),"Could not clean up engine descendants");
  const auto deadline=GetTickCount64()+5000;
  while(!jobEmpty(impl_->job->get())&&GetTickCount64()<deadline)Sleep(10);
  require(jobEmpty(impl_->job->get()),"Windows engine descendants still present");
  if(impl_->log->valid()){
    require(FlushFileBuffers(impl_->log->get()),"Could not flush engine log");
    require(impl_->log->close(),"Could not close engine log");
  }
  require(writePrivateFileAtomically(impl_->generation/L"exit.windows",impl_->record.completion(code)),"Could not publish engine completion; identity retained");
  impl_->finished=code;return impl_->finished;
}
std::uint32_t WindowsEngineProcess::stop(std::uint32_t code){
  if(auto already=wait(0))return *already;
  require(TerminateJobObject(impl_->job->get(),code),"Could not stop owned Windows engine tree");
  auto completed=wait(5000);require(completed.has_value(),"Windows engine stop was not confirmed");return *completed;
}
WindowsEngineSessionResult runWindowsEngineInSlot(const fs::path& executable,const std::vector<std::string>& arguments,
    const fs::path& workspace,std::uint32_t timeoutMilliseconds){
  const auto slot=client::ClientStateStore::executionSlot();
  require(ClientRuntimeControl::acquire(),"Windows runtime slot is active or unavailable");
  struct Owner{bool active=true;~Owner(){if(active)ClientRuntimeControl::release();}} owner;
  auto engine=WindowsEngineProcess::start(executable,arguments,workspace);
  const auto began=GetTickCount64();WindowsEngineSessionResult result;
  for(;;){
    if(auto finished=engine->wait(25)){result.exitCode=*finished;break;}
    result.stopRequested=ClientRuntimeControl::stopRequested(slot);
    result.timedOut=timeoutMilliseconds&&GetTickCount64()-began>=timeoutMilliseconds;
    if(result.stopRequested||result.timedOut){
      result.exitCode=engine->stop(124);break;
    }
    // Safe-stop is deliberately not a forced termination. No next engine is
    // started by this function; a safe request is consumed only on release.
  }
  result.safeStopRequested=ClientRuntimeControl::safeStopRequested(slot);
  require(ClientRuntimeControl::release(),"Windows runtime ownership could not be released; retained");owner.active=false;
  return result;
}
}
