#include "openpuzzle/runtime/WindowsEngineProcess.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/core/WindowsCommandLine.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <future>
#include <memory>
#include <stdexcept>
namespace fs=std::filesystem;
using namespace openpuzzle;
using Process=WindowsEngineProcess;
using Status=WindowsProcessIdentity::Status;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void publish(const fs::path& p,const std::string& t){require(writePrivateFileAtomically(p,t),"Fixture publication failed");}
std::string hex(const std::string& text){const char* d="0123456789abcdef";std::string out;for(unsigned char c:text){out+=d[c>>4];out+=d[c&15];}return out;}
void waitFile(const fs::path& p){auto end=GetTickCount64()+10000;while(!fs::exists(p)&&GetTickCount64()<end)Sleep(10);require(fs::exists(p),"Fixture readiness timed out");}
struct Child{
 windowsFile::Handle process,thread;
 Child(const fs::path& exe,const std::vector<std::string>& args):Child(exe,args,launch(exe,args)){}
 ~Child(){if(WaitForSingleObject(process.get(),0)==WAIT_TIMEOUT){TerminateProcess(process.get(),99);WaitForSingleObject(process.get(),5000);}}
 void kill(){require(TerminateProcess(process.get(),77),"Owned fixture termination failed");require(WaitForSingleObject(process.get(),5000)==WAIT_OBJECT_0,"Fixture termination timed out");}
 void finish(){require(WaitForSingleObject(process.get(),10000)==WAIT_OBJECT_0,"Fixture did not finish");DWORD c=0;require(GetExitCodeProcess(process.get(),&c)&&c==0,"Fixture failed");}
private:
 static PROCESS_INFORMATION launch(const fs::path& exe,const std::vector<std::string>& args){
  auto command=pathFromUtf8(WindowsCommandLine::build(pathUtf8(exe),args)).native();
  STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION p{};
  require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&p),"Owner fixture launch failed");return p;
 }
 Child(const fs::path&,const std::vector<std::string>&,PROCESS_INFORMATION p):process(p.hProcess),thread(p.hThread){}
};
WindowsRuntimeRecord descendant(const fs::path& log){
 auto end=GetTickCount64()+5000;std::string text;
 while(GetTickCount64()<end){text=bytes(log);if(text.find("child=")!=std::string::npos&&text.find('\n')!=std::string::npos)break;Sleep(10);}
 auto at=text.find("child=");require(at!=std::string::npos,"Descendant identity missing");at+=6;
 auto colon=text.find(':',at);auto line=text.find('\n',colon);require(colon!=std::string::npos&&line!=std::string::npos,"Descendant identity incomplete");
 return {std::stoi(text.substr(at,colon-at)),std::stoull(text.substr(colon+1,line-colon-1))};
}
int wmain(int argc,wchar_t** argv){fs::path root;try{
 if(argc==5){
  const std::wstring mode=argv[1];const fs::path fixture=argv[2];root=argv[3];const auto slot=pathUtf8(fs::path(argv[4]));
  require(SetEnvironmentVariableW(L"LOCALAPPDATA",(root/L"local").c_str()),"Child storage isolation failed");
  require(SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT",slot.c_str()),"Child slot failed");
  if(mode==L"--owner"){
   auto engine=Process::start(fixture,{"sleep"},root/L"workspace");publish(root/L"owner-ready",engine->identity().encode());Sleep(15000);return 0;
  }
  require(mode==L"--session","Invalid fixture owner mode");
  auto code=runWindowsEngineInSlot(fixture,{"gate",pathUtf8(root/L"finish-gate"),pathUtf8(root/L"engine-ready")},root/L"workspace",12000);
  publish(root/L"session-code",std::to_string(code.exitCode));
  if(code.safeStopRequested)publish(root/L"safe-observed","safe");
  if(code.stopRequested)publish(root/L"stop-observed","stop");
  return 0;
 }
 require(argc==2,"Engine fixture executable required");const fs::path sourceFixture=fs::absolute(argv[1]);const fs::path self=fs::absolute(argv[0]);
 root=fs::temp_directory_path()/fs::path(L"OpenPuzzle engine ç 中文 "+std::to_wstring(GetCurrentProcessId()));
 require(!fs::exists(root),"Fixture root must be new");WorkspaceSecurity::prepare(root);
 const auto fixture=root/L"Engine fixture ç 中文.exe";
 fs::copy_file(sourceFixture,fixture);WorkspaceSecurity::protectFile(fixture);
 require(SetEnvironmentVariableW(L"LOCALAPPDATA",(root/L"local").c_str()),"Parent storage isolation failed");
 for(const auto& invalid:std::vector<std::string>{std::string("a\0b",3),std::string("\xff",1),std::string(40000,'x')}){
  bool rejected=false;try{Process::start(fixture,{invalid},root/L"invalid command");}catch(...){rejected=true;}
  require(rejected&&!fs::exists(root/L"invalid command"),"Invalid command caused launch side effects");
 }
 auto workspace=root/L"typed arguments";
 const std::vector<std::string> args={"echo","","a b","C:\\ends\\","a\\\"b","ç 中文","%PATH% & echo INJECTED > bad.txt","\"","x\nz"};
 auto engine=Process::start(fixture,args,workspace);const auto first=engine->identity();const auto log=engine->logPath();
 require(Process::recover(workspace).record==first,"Published engine identity missing");
 require(engine->wait(5000)==0,"Typed argument fixture failed");
 std::string expected;for(std::size_t i=1;i<args.size();++i)expected+=std::to_string(i-1)+':'+hex(args[i])+"\r\n";expected+="stderr\r\n";
 require(bytes(log)==expected,"Direct Windows arguments did not round trip literally");
 require(!fs::exists(workspace/L"bad.txt"),"Shell metacharacters executed");
 auto recovered=Process::recover(workspace);require(recovered.status==Status::Inactive&&recovered.exitCode==0,"Natural completion not recovered");
 engine.reset();require(bytes(log)==expected,"Completed engine destruction changed log");
 auto cwd=Process::start(fixture,{"cwd"},workspace);require(cwd->wait(5000)==0&&bytes(cwd->logPath())==hex(pathUtf8(workspace))+"\r\n","Engine cwd not set");cwd.reset();
 for(int i=0;i<8;++i){auto fast=Process::start(fixture,{"exit259"},workspace);require(fast->identity().process.valid()&&fast->wait(5000)==259,"Fast DWORD exit lost");require(Process::recover(workspace).exitCode==259,"Exit 259 mistaken for running");}
 {SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};windowsFile::Handle event(CreateEventW(&security,TRUE,FALSE,nullptr));require(event.valid(),"Fixture event creation failed");
  auto probe=Process::start(fixture,{"probe",std::to_string(reinterpret_cast<std::uintptr_t>(event.get()))},root/L"handles");
  require(probe->wait(5000)==0&&WaitForSingleObject(event.get(),0)==WAIT_TIMEOUT,"Unrelated inheritable handle escaped into engine");}
 auto live=Process::start(fixture,{"sleep"},workspace);const auto liveRecord=bytes(Process::identityPath(workspace));
 bool denied=false;try{Process::start(fixture,{"touch",pathUtf8(root/L"duplicate-ran")},workspace);}catch(const std::exception&){denied=true;}
 require(denied&&bytes(Process::identityPath(workspace))==liveRecord&&!fs::exists(root/L"duplicate-ran"),"Occupied workspace relaunched");
 require(live->stop()==124&&Process::recover(workspace).exitCode==124,"Owned engine stop not recorded");live.reset();
 for(const auto& text:std::vector<std::string>{"incomplete",first.process.encode(),first.encode()+"extra"}){
  publish(Process::identityPath(workspace),text);denied=false;try{Process::start(fixture,{"touch",pathUtf8(root/L"unknown-ran")},workspace);}catch(...){denied=true;}
  require(denied&&Process::recover(workspace).status==Status::Unavailable&&bytes(Process::identityPath(workspace))==text&&!fs::exists(root/L"unknown-ran"),"Unknown identity overwritten");
 }
 // Restore only deliberately injected fixture bytes.
 publish(Process::identityPath(workspace),first.encode());
 {windowsFile::Handle held(CreateFileW(Process::identityPath(workspace).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));require(held.valid(),"Fixture marker lock failed");
  denied=false;try{Process::start(fixture,{"touch",pathUtf8(root/L"publication-ran")},workspace);}catch(...){denied=true;}
  require(denied&&bytes(Process::identityPath(workspace))==first.encode()&&!fs::exists(root/L"publication-ran"),"Failed publication resumed child or replaced prior marker");}
 auto completion=Process::generationPath(workspace,first)/L"exit.windows";auto foreign=first;foreign.process.creationTime++;
 publish(completion,foreign.completion(0));recovered=Process::recover(workspace);
 require(recovered.status==Status::Inactive&&!recovered.exitCode&&recovered.completionUnavailable,"Foreign completion reported success");
 auto afterForeign=Process::start(fixture,{"exit259"},workspace);require(afterForeign->wait(5000)==259&&bytes(completion)==foreign.completion(0),"Prior completion data overwritten");afterForeign.reset();
 {
  auto pending=Process::start(fixture,{"gate",pathUtf8(root/L"complete-gate"),pathUtf8(root/L"complete-ready")},root/L"completion failure");waitFile(root/L"complete-ready");
  auto resultPath=Process::generationPath(root/L"completion failure",pending->identity())/L"exit.windows";
  publish(resultPath,"preserve on failure");
  {windowsFile::Handle held(CreateFileW(resultPath.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));require(held.valid(),"Fixture completion lock failed");
   publish(root/L"complete-gate","finish");denied=false;try{pending->wait(5000);}catch(...){denied=true;}
   require(denied&&bytes(resultPath)=="preserve on failure","Failed completion publication lost prior bytes");}
  require(pending->wait(0)==0&&Process::recover(root/L"completion failure").exitCode==0,"Completion publication could not retry");
 }
 {auto tree=Process::start(fixture,{"tree"},root/L"tree");auto child=descendant(tree->logPath());require(WindowsProcessIdentity::status(child.pid,child.creationTime)==Status::Running,"Fixture descendant absent");
  require(tree->stop()==124&&WindowsProcessIdentity::status(child.pid,child.creationTime)==Status::Inactive,"Owned descendant survived stop");}
 {auto tree=Process::start(fixture,{"leave-tree"},root/L"natural tree");const auto path=tree->logPath();require(tree->wait(5000)==0,"Natural parent exit failed");auto child=descendant(path);
  require(WindowsProcessIdentity::status(child.pid,child.creationTime)==Status::Inactive,"Descendant survived natural engine completion");}
 // Eight simultaneous callers must publish one engine generation, not eight.
 std::promise<void> go;auto gate=go.get_future().share();std::vector<std::future<std::unique_ptr<Process>>> contenders;
 for(int i=0;i<8;++i)contenders.push_back(std::async(std::launch::async,[&]{gate.wait();try{return Process::start(fixture,{"sleep"},root/L"race");}catch(...){return std::unique_ptr<Process>{};}}));
 go.set_value();
 int winners=0;std::unique_ptr<Process> winner;for(auto& c:contenders){auto p=c.get();if(p){winners++;winner=std::move(p);}}
 require(winners==1,"Concurrent callers started duplicate engines");winner->stop();winner.reset();
 auto crash=root/L"crash";WorkspaceSecurity::prepare(crash);
 {Child owner(self,{"--owner",pathUtf8(fixture),pathUtf8(crash),"cuda-7"});waitFile(crash/L"owner-ready");auto record=WindowsEngineRecord::parse(bytes(crash/L"owner-ready"));require(record.has_value(),"Owner fixture record invalid");owner.kill();
  const auto end=GetTickCount64()+5000;while(WindowsProcessIdentity::status(record->process.pid,record->process.creationTime)!=Status::Inactive&&GetTickCount64()<end)Sleep(10);
  recovered=Process::recover(crash/L"workspace");require(recovered.status==Status::Inactive&&!recovered.exitCode,"Owner crash left live orphan or fabricated exit");
  auto restart=Process::start(fixture,{"exit259"},crash/L"workspace");require(restart->wait(5000)==259,"Stale workspace not recovered");}
 for(bool safe:{false,true}){
  auto session=root/(safe?L"safe session":L"stop session");WorkspaceSecurity::prepare(session);
  // Parent controller uses exactly the same isolated LOCALAPPDATA as the child.
  require(SetEnvironmentVariableW(L"LOCALAPPDATA",(session/L"local").c_str()),"Session storage selection failed");
  Child owner(self,{"--session",pathUtf8(fixture),pathUtf8(session),"opencl-3"});waitFile(session/L"engine-ready");
  if(safe){require(ClientRuntimeControl::requestSafeStop("opencl-3"),"Safe stop request failed");Sleep(100);
   require(Process::recover(session/L"workspace").status==Status::Running&&!fs::exists(session/L"session-code"),"Safe stop prematurely terminated current engine");publish(session/L"finish-gate","finish");}
  else require(ClientRuntimeControl::requestStop("opencl-3"),"Stop request failed");
  owner.finish();require(bytes(session/L"session-code")== (safe?"0":"124"),"Runtime control result incorrect");
  require(fs::exists(session/(safe?L"safe-observed":L"stop-observed")),"Runtime stop intent lost on return");
  require(ClientRuntimeControl::runtimeIdentityStatus("opencl-3")==ClientRuntimeControl::RuntimeIdentityStatus::Inactive,"Session did not release runtime");
 }
 require(SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT","cpu"),"Timeout slot selection failed");
 auto timed=runWindowsEngineInSlot(fixture,{"sleep"},root/L"timeout",100);
 require(timed.exitCode==124&&timed.timedOut&&!timed.stopRequested,"Timeout result lost or mislabelled");
 fs::remove_all(root);std::cout<<"WindowsEngineLifecycleTests passed: direct argv, owned tree, persisted exits, concurrent workspaces, crash recovery and runtime controls\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(!root.empty())std::cerr<<"Fixture retained: "<<pathUtf8(root)<<'\n';return 1;}}
