#include "WindowsLocalHttpFixture.hpp"
#include "openpuzzle/runtime/WindowsRuntimeSession.hpp"
#include "openpuzzle/runtime/WindowsExecutionState.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include <atomic>
#include <functional>
#include <iostream>
using namespace openpuzzle;
namespace fs=std::filesystem;
using Control=ClientRuntimeControl;using Store=client::ClientStateStore;
class SessionServer {
  Socket listener{socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)};
  std::thread worker;std::atomic_bool closing{false};std::exception_ptr failure;
  unsigned short port=0;std::function<std::string(const std::string&)> respond;
  void serve(){try{while(!closing){
    fd_set fds;FD_ZERO(&fds);FD_SET(listener.s,&fds);timeval wait{0,200000};
    auto ready=select(0,&fds,nullptr,nullptr,&wait);if(closing)break;
    require(ready>=0,"Session fixture select failed");if(!ready)continue;
    Socket peer(accept(listener.s,nullptr,nullptr));require(peer.s!=INVALID_SOCKET,"Session fixture accept failed");
    DWORD timeout=2000;setsockopt(peer.s,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
    std::string request;char buffer[4096];std::size_t length=0;
    for(;;){int n=recv(peer.s,buffer,sizeof(buffer),0);require(n>0,"Session request incomplete");request.append(buffer,n);
      auto at=request.find("\r\n\r\n");if(at==std::string::npos)continue;
      if(!length){const auto header=lower(request.substr(0,at));auto size=header.find("content-length:");length=at+4+(size==std::string::npos?0:std::stoul(header.substr(size+15)));}
      if(request.size()>=length)break;require(request.size()<65536,"Session fixture request exceeded bound");
    }
    requests.push_back(request);auto status=respond(request);const auto reply="HTTP/1.1 "+status+"\r\nContent-Type: application/json\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}";
    std::size_t sent=0;while(sent<reply.size()){int n=send(peer.s,reply.data()+sent,static_cast<int>(reply.size()-sent),0);require(n>0,"Session fixture reply failed");sent+=n;}
  }}catch(...){failure=std::current_exception();}}
public:
  std::vector<std::string> requests;
  explicit SessionServer(std::function<std::string(const std::string&)> callback):respond(std::move(callback)){
    require(listener.s!=INVALID_SOCKET,"Session socket failed");sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    require(bind(listener.s,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0&&listen(listener.s,4)==0,"Session bind failed");
    int size=sizeof(address);require(getsockname(listener.s,reinterpret_cast<sockaddr*>(&address),&size)==0,"Session port failed");port=ntohs(address.sin_port);worker=std::thread([this]{serve();});
  }
  ~SessionServer(){closing=true;if(worker.joinable())worker.join();}
  void finish(){closing=true;if(worker.joinable())worker.join();if(failure)std::rethrow_exception(failure);}
  std::string url()const{return "http://127.0.0.1:"+std::to_string(port);}
};
void publish(const fs::path& p,const std::string& value="fixture"){require(writePrivateFileAtomically(p,value),"Session fixture publication failed");}
client::ClientHeartbeat capabilities(){client::ClientHeartbeat h;h.clientId="fixture-client";h.platform="windows";h.version="1.0.46";h.cpu={"fixture CPU",1,1};h.gpus={{"cuda","fixture GPU",1}};h.engines={{"BitCrack","cuda",true,true}};return h;}
client::ClientExecutionState state(const fs::path& root,const std::string& id){client::ClientExecutionState s;s.active=true;s.assignmentId=id;s.clientId="fixture-client";s.puzzle=20;s.rangeId=1;s.target="fixture-address";s.start="0";s.end="F";s.engine="BitCrack";s.backend="cuda";s.workspace=pathUtf8(root/pathFromUtf8(id));return s;}
bool endpoint(const std::string& request,const char* path){return request.starts_with(std::string("POST ")+path+' ');}
int wmain(int argc,wchar_t** argv){try{
  require(argc==2,"Engine fixture path required");Winsock sockets;const fs::path fixture=fs::absolute(argv[1]);
  const auto root=fs::temp_directory_path()/pathFromUtf8("OpenPuzzle-session-ç-中文-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));fs::create_directories(root);
  require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str())&&SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT","cuda-6"),"Session environment failed");
  {bool called=false;WindowsRuntimeSession session("http://127.0.0.1:1",capabilities(),[]{return false;});require(session.runContinuous([&](auto&){called=true;return ClientIterationResult{};})==1&&!called&&!Control::running("cuda-6"),"Preflight did not block session");}
  {bool caught=false;try{WindowsRuntimeSession bad("file:///invalid",capabilities(),[]{return true;});}catch(const std::invalid_argument&){caught=true;}require(caught,"Invalid session URL accepted");}
  {auto path=Store::path("cuda-6");WorkspaceSecurity::prepare(path.parent_path());publish(path,"invalid-state");int called=0;WindowsRuntimeSession session("http://127.0.0.1:1",capabilities(),[]{return true;});require(session.runContinuous([&](auto&){++called;return ClientIterationResult{};})==1&&called==0&&fs::exists(path)&&!Control::running("cuda-6"),"Unreadable assignment was overwritten");fs::remove(path);}
  {SessionServer server([](const auto& request){require(endpoint(request,"/api/client/heartbeat"),"Unexpected capability fixture endpoint");return "200 OK";});
    WindowsRuntimeSession session(server.url(),capabilities(),[]{return true;});auto denied=state(root,"denied");denied.backend="opencl";
    require(session.runContinuous([&](auto& owner){return owner.execute(fixture,{"touch",pathUtf8(root/"must-not-run")},denied);})==1&&!fs::exists(root/"must-not-run")&&!Store::load("cuda-6"),"Unverified backend launched");server.finish();require(server.requests.size()==1,"Capability refusal sent extra requests");}
  {int prepared=0;SessionServer server([](const auto& request){require(endpoint(request,"/api/client/heartbeat"),"Unexpected idle acknowledgement endpoint");require(Control::requestStop("cuda-6"),"Idle stop fixture failed");return "503 Service Unavailable";});
    WindowsRuntimeSession session(server.url(),capabilities(),[]{return true;});
    require(session.runContinuous([&](auto&){++prepared;return ClientIterationResult{};})==0&&prepared==0&&!Store::load("cuda-6"),"Unacknowledged idle heartbeat requested work");server.finish();require(server.requests.size()==1,"Idle refusal repeated after stop");}
  {int completed=0,prepared=0;SessionServer server([&](const auto& request){
      if(endpoint(request,"/api/range/complete")){++completed;require(request.find("\"keys_checked\":\"16\"")!=std::string::npos&&request.find("\"status\":\"completed\"")!=std::string::npos,"Completion lost exact coverage");require(Control::requestSafeStop("cuda-6"),"Safe stop fixture failed");}
      else require(endpoint(request,"/api/client/heartbeat")||endpoint(request,"/api/range/progress"),"Unexpected completion fixture endpoint");return "200 OK";
    });publish(root/"complete-gate");WindowsRuntimeSession session(server.url(),capabilities(),[]{return true;});
    require(session.runContinuous([&](auto& owner){++prepared;return owner.execute(fixture,{"execution",pathUtf8(root/"complete-gate"),pathUtf8(root/"complete-ready"),"proof"},state(root,"complete"));})==0,"Shared native completion monitor failed");
    server.finish();require(completed==1&&prepared==1&&!Store::load("cuda-6")&&!Control::running("cuda-6"),"Safe stop requested new work or failed cleanup");}
  {auto foreign=WindowsEngineProcess::start(fixture,{"sleep"},root/"foreign-fixture");auto identity=foreign->identity();int cancelled=0;
    SessionServer server([&](const auto& request){
      if(endpoint(request,"/api/range/progress"))require(Control::requestStop("cuda-6"),"Cooperative stop fixture failed");
      else if(endpoint(request,"/api/range/complete")){++cancelled;require(request.find("\"status\":\"cancelled\"")!=std::string::npos&&request.find("\"exit_code\":-2")!=std::string::npos,"Stop became completed work");return "503 Service Unavailable";}
      else require(endpoint(request,"/api/client/heartbeat"),"Unexpected stop fixture endpoint");return "200 OK";
    });WindowsRuntimeSession session(server.url(),capabilities(),[]{return true;});
    require(session.runContinuous([&](auto& owner){return owner.execute(fixture,{"execution",pathUtf8(root/"stop-gate"),pathUtf8(root/"stop-ready"),"none"},state(root,"stopped"));})==1,"Unacknowledged stop was reported successful");
    server.finish();const auto saved=Store::load("cuda-6");require(cancelled==1&&saved&&WindowsEngineProcess::recover(pathFromUtf8(saved->workspace)).exitCode==125,"Failed cancellation lost persisted state/completion");
    require(WindowsProcessIdentity::status(identity.process.pid,identity.process.creationTime)==WindowsProcessIdentity::Status::Running,"Session stopped an unrelated process");foreign->stop();Store::remove("cuda-6");}
  {int finalized=0;bool corrupted=false;SessionServer server([&](const auto& request){
      if(endpoint(request,"/api/range/progress") && !corrupted){auto saved=Store::load("cuda-6");require(saved.has_value(),"Uncertain-identity fixture missing");saved->processGeneration="foreign-generation";require(Store::save(*saved,"cuda-6"),"Uncertain-identity fixture publication failed");corrupted=true;require(Control::requestStop("cuda-6"),"Uncertain stop fixture failed");}
      else if(endpoint(request,"/api/range/complete"))++finalized;
      else require(endpoint(request,"/api/client/heartbeat"),"Unexpected uncertain-identity endpoint");return "200 OK";
    });WindowsRuntimeSession session(server.url(),capabilities(),[]{return true;});
    require(session.runContinuous([&](auto& owner){return owner.execute(fixture,{"execution",pathUtf8(root/"uncertain-gate"),pathUtf8(root/"uncertain-ready"),"none"},state(root,"uncertain"));})==1,"Unknown identity became successful cancellation");
    server.finish();require(corrupted&&finalized==0&&Store::load("cuda-6"),"Unknown identity was finalized or discarded");Store::remove("cuda-6");}
  {const auto synthetic=std::string("K")+std::string(51,'1');auto assigned=state(root,"local-solution");const auto workspace=pathFromUtf8(assigned.workspace);int reports=0;
    SessionServer server([&](const auto& request){require(request.find(synthetic)==std::string::npos&&request.find("private_key")==std::string::npos,"Session uploaded local fixture material");
      if(endpoint(request,"/api/range/progress"))publish(workspace/"found.txt","OPENPUZZLE_SOLUTION_V1\naddress=fixture-address\nprivate_key_wif="+synthetic+"\ncompression=compressed\n");
      else if(endpoint(request,"/api/puzzle/report-solution"))++reports;
      else require(endpoint(request,"/api/client/heartbeat"),"Unexpected solution fixture endpoint");return "200 OK";
    });WindowsRuntimeSession session(server.url(),capabilities(),[]{return true;});
    require(session.runContinuous([&](auto& owner){return owner.execute(fixture,{"execution",pathUtf8(root/"solution-gate"),pathUtf8(root/"solution-ready"),"none"},assigned);})==0,"Shared solution monitor failed");
    server.finish();require(reports==1&&Store::load("cuda-6")&&fs::exists(localPaths::root()/"solutions"/"Puzzle-20"/"local-solution"/"wallet-import.txt"),"Solution was not privately exported and preserved");Store::remove("cuda-6");}
  fs::remove_all(root);std::cout<<"WindowsRuntimeSessionTests passed: actual monitor/continuous loop, verified launch, safe stop, owned cancellation and private solution report\n";return 0;
}catch(const std::exception& e){Control::release();std::cerr<<e.what()<<'\n';return 1;}}
