#include "WindowsRunRequestFixture.hpp"
#include "openpuzzle/runtime/WindowsRunSession.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/ClientIdentity.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include <sstream>
#include <fstream>
#include <iostream>
using namespace openpuzzle;namespace fs=std::filesystem;using Control=ClientRuntimeControl;using Store=client::ClientStateStore;
const std::string target="1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";
bool endpoint(const std::string& r,const char* p){return r.starts_with(std::string("POST ")+p+' ');}
std::string assigned(const std::string& id="fixture-claim",const std::string& begin="0",const std::string& finish="F"){
 return "{\"assignment_id\":\""+id+"\",\"puzzle\":71,\"range_id\":1,\"target\":\""+target+"\",\"start\":\""+begin+"\",\"end\":\""+finish+"\"}";
}
WindowsRunDependencies deps(const fs::path& fixture,const std::string& url){
 WindowsRunDependencies d;d.serverUrl=url;d.engines={{"bitcrack","cuda",fixture,{0,2},12.5,true},{"keyhunt","cpu",fixture,{},1,true}};
 d.preflight=[](const auto&,const auto&){return true;};
 d.capabilities=[](const std::string& id){client::ClientHeartbeat h;h.clientId=id;h.platform="windows";h.version="1.0.46";h.cpu={"fixture CPU",4,4};h.gpus={{"cuda","fixture GPU",1}};h.engines={{"BitCrack","cuda",true,true},{"KeyHunt","cpu",true,true}};return h;};
 d.metadata=[](int number)->std::optional<PuzzleExecutionMetadata>{if(number!=71)return {};PuzzleExecutionMetadata m;m.puzzle=71;m.address=target;m.keyspace="0:FF";return m;};return d;
}
std::string read(const fs::path& p){std::ifstream input(p);std::ostringstream text;text<<input.rdbuf();return text.str();}
void publish(const fs::path& p,const std::string& s){require(writePrivateFileAtomically(p,s),"Private fixture publication failed");}
int wmain(int argc,wchar_t** argv){try{
 require(argc==2,"Engine fixture path required");Winsock sockets;const auto fixture=fs::absolute(argv[1]);
 const auto root=fs::temp_directory_path()/pathFromUtf8("OpenPuzzle-run ç 中文-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));fs::create_directories(root);
 require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str())&&SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT","cuda-7"),"Run environment setup failed");
 const auto pending=WindowsRunSession::pendingClaimPath("cuda-7");std::ostringstream output;
 {WindowsRunSession controls({});for(const auto& command:{"help","version","status"})require(controls.run({command},output)==0,"Local information requires remote configuration");
  require(Control::acquire(),"Local control fixture acquisition failed");
  require(controls.run({"stop"},output)==0&&Control::stopRequested("cuda-7"),"Stop command did not publish cooperative marker");
  require(Control::clearStop("cuda-7"),"Stop fixture cleanup failed");
  require(controls.run({"safestop"},output)==0&&Control::safeStopRequested("cuda-7"),"Safe stop command did not publish cooperative marker");
  require(Control::release(),"Local control fixture release failed");}
 {auto d=deps(fixture,"http://127.0.0.1:1");int called=0;d.preflight=[&](const auto&,const auto&){++called;return true;};WindowsRunSession session(d);
  require(session.run({"run","--device","2x"},output)==1&&called==0&&!fs::exists(localPaths::configDirectory()/"client.id"),"Argument failure changed identity or reached preflight");
  require(session.run({"run","71","--dry-run","--device","2"},output)==0&&called==1&&!fs::exists(localPaths::configDirectory()/"client.id")&&!Control::running("cuda-7"),"Dry-run registered/acquired a runtime or created identity");
  require(session.run({"run","71","--once","--device","3"},output)==1&&called==1,"Missing device reached hardware preflight");
  require(session.run({"run","72","--once"},output)==1&&called==1,"Unavailable puzzle reached preflight");}
 {auto d=deps(fixture,"http://127.0.0.1:1");d.engines.erase(d.engines.begin());int called=0;d.preflight=[&](const auto&,const auto&){++called;return true;};
  require(WindowsRunSession(d).run({"run","--once","--device","0"},output)==1&&called==0&&!fs::exists(pending),"Automatic CPU selection ignored explicit GPU options");}
 {auto d=deps(fixture,"http://127.0.0.1:1");d.preflight=[](const auto&,const auto&){return false;};require(WindowsRunSession(d).run({"run","--once"},output)==1&&!fs::exists(pending),"Preflight refusal published a claim");}
 {auto d=deps(fixture,"http://127.0.0.1:1");d.capabilities=[](const std::string& id){auto h=deps(fs::path{},"").capabilities(id);h.engines.clear();return h;};require(WindowsRunSession(d).run({"run","--once"},output)==1&&!fs::exists(pending),"Unverified heartbeat capabilities reached claim");}
 for(const bool failRegistration:{true,false}){
  RunServer server([](const auto& request){require(endpoint(request,"/api/client/register")||endpoint(request,"/api/client/heartbeat"),"Failed initialization reached assignment endpoint");return std::string("{}");},
    [&](const auto& request){return failRegistration||endpoint(request,"/api/client/heartbeat")?std::string("503 Service Unavailable"):std::string("200 OK");});
  require(WindowsRunSession(deps(fixture,server.url())).run({"run","--once"},output)==1,"Failed initialization was reported as successful");server.finish();
  require(server.requests.size()==(failRegistration?1u:2u)&&!fs::exists(pending)&&!Control::running("cuda-7"),"Unacknowledged initialization claimed work or retained runtime ownership");
 }
 for(const bool cpu:{false,true}){
  int claims=0,complete=0;RunServer server([&](const auto& request){
    if(endpoint(request,"/api/range/claim")){++claims;require(request.find("\"puzzle\":0")!=std::string::npos&&request.find("\"target_duration_minutes\":5")!=std::string::npos,"Automatic puzzle/duration claim changed");return assigned(cpu?"cpu-once":"gpu-once",cpu?"1":"0",cpu?"10":"F");}
    if(endpoint(request,"/api/range/complete")){++complete;require(request.find("\"status\":\"completed\"")!=std::string::npos&&request.find("\"keys_checked\":\"16\"")!=std::string::npos,"Completion did not preserve full inclusive coverage");}
    else require(endpoint(request,"/api/client/register")||endpoint(request,"/api/client/heartbeat")||endpoint(request,"/api/range/progress"),"Unexpected native command endpoint");return std::string("{}");
  });
  const std::vector<std::string> args=cpu?std::vector<std::string>{"run","--once","--engine","keyhunt","--threads","2","--duration-minutes","5"}:std::vector<std::string>{"run","--once","--device","2","--duration-minutes","5"};
  require(WindowsRunSession(deps(fixture,server.url())).run(args,output)==0,"Single native assignment failed");server.finish();
  require(claims==1&&complete==1&&endpoint(server.requests.front(),"/api/client/register")&&endpoint(server.requests[1],"/api/client/heartbeat"),"Registration/heartbeat/claim order changed or once requested extra work");
  require(!fs::exists(pending)&&!Store::load("cuda-7")&&!Control::running("cuda-7"),"Acknowledged single run left pending claim/runtime state");
  auto workspace=localPaths::dataDirectory()/"assignments"/(cpu?"cpu-once":"gpu-once");const auto received=read(workspace/"fixture-arguments.txt");
  require(received.find(cpu?"-m\naddress\n":"--keyspace\n0:F\n")!=std::string::npos,"Native engine did not receive the planned argument vector");
 }
 {int claims=0;RunServer server([&](const auto& request){if(endpoint(request,"/api/range/claim")){++claims;return std::string("{\"available\":false,\"message\":\"fixture empty\"}");}return std::string("{}");});
  require(WindowsRunSession(deps(fixture,server.url())).run({"run","--once"},output)==0,"One-shot unavailable result did not finish");server.finish();require(claims==1&&!fs::exists(pending),"Unavailable work retained a lease or repeated a claim");}
 {RunServer server([](const auto& request){return endpoint(request,"/api/range/claim")?assigned("invalid-range","F","0"):std::string("{}");});
  require(WindowsRunSession(deps(fixture,server.url())).run({"run","71","--once"},output)==1,"Descending claim launched");server.finish();
  require(fs::exists(pending)&&read(pending).find("invalid-range")!=std::string::npos&&!fs::exists(localPaths::dataDirectory()/"assignments"/"invalid-range"),"Invalid claim lost receipt or created a workspace");
  const auto preserved=read(pending);require(WindowsRunSession(deps(fixture,server.url())).run({"run","--once"},output)==1&&read(pending)==preserved,"Pending claim was overwritten or ignored");fs::remove(pending);}
 {RunServer server([](const auto& request){return endpoint(request,"/api/range/claim")?std::string("not-json"):std::string("{}");});
  require(WindowsRunSession(deps(fixture,server.url())).run({"run","--once"},output)==1,"Ambiguous claim response accepted");server.finish();require(fs::exists(pending)&&read(pending).find("requesting")!=std::string::npos,"Ambiguous claim lost pre-request intent");fs::remove(pending);}
 {const auto work=localPaths::dataDirectory()/"assignments"/"existing-work";fs::create_directories(work);publish(work/"preserved.txt","byte-for-byte");
  RunServer server([](const auto& request){return endpoint(request,"/api/range/claim")?assigned("existing-work"):std::string("{}");});
  require(WindowsRunSession(deps(fixture,server.url())).run({"run","--once"},output)==1,"Existing assignment workspace reused");server.finish();require(read(work/"preserved.txt")=="byte-for-byte"&&fs::exists(pending),"Workspace refusal discarded existing data or claim");fs::remove(pending);}
 {int claimed=0;RunServer server([&](const auto& request){if(endpoint(request,"/api/range/claim")){++claimed;return assigned("continuous-safe");}if(endpoint(request,"/api/range/complete"))require(Control::requestSafeStop("cuda-7"),"Native safe stop request failed");return std::string("{}");});
  require(WindowsRunSession(deps(fixture,server.url())).run({"run","71"},output)==0,"Continuous safe-stop run failed");server.finish();require(claimed==1&&!fs::exists(pending),"Safe stop allocated another range or lost final cleanup");}
 fs::remove_all(root);std::cout<<"WindowsRunSessionTests passed: registration order, linear CUDA/CPU planning, one-shot/continuous execution, local controls and preserved uncertain claims\n";return 0;
}catch(const std::exception& error){Control::release();std::cerr<<error.what()<<'\n';return 1;}}
