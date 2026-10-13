#include "WindowsRunRequestFixture.hpp"
#include "openpuzzle/runtime/WindowsRuntimeSession.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WindowsExecutionState.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include <iostream>
using namespace openpuzzle;namespace fs=std::filesystem;
WindowsTelemetryReport readings(double t){client::ClientGpuTelemetry v;v.vendor="NVIDIA";v.deviceId="cuda-0";v.device=0;v.pciBusId="00000000:08:00.0";v.temperatureC=t;v.powerDrawW=123;v.powerLimitW=290;return {true,"fixture",{{"fixture GPU",v}}};}
client::ClientHeartbeat capabilities(){client::ClientHeartbeat h;h.clientId="fixture-client";h.platform="windows";h.version="1.0.46";h.cpu={"fixture CPU",1,1};h.gpus={{"cuda","fixture GPU",1}};h.engines={{"BitCrack","cuda",true,true}};return h;}
bool endpoint(const std::string& r,const char* p){return r.starts_with(std::string("POST ")+p+' ');}
int wmain(int argc,wchar_t** argv){try{
 require(argc==2,"Engine fixture required");Winsock sockets;const auto engine=fs::absolute(argv[1]);
 const auto root=fs::temp_directory_path()/pathFromUtf8("op-thermal-runtime-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));fs::create_directory(root);
 require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str())&&SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT","cuda-7"),"Thermal environment failed");
 WindowsRuntimeThermalSettings settings;settings.policy={true,true,75,85};settings.deviceName="fixture GPU";settings.interval=std::chrono::milliseconds(1);settings.reader=[]{return readings(88);};
 {int requested=0;RunServer server([](const auto&){throw std::runtime_error("Hot startup must not contact server");return std::string{};});
  WindowsRuntimeSession hot(server.url(),capabilities(),[]{return true;},settings);
  require(hot.runContinuous([&](auto&){++requested;return ClientIterationResult{};},true)==2&&requested==0,"Hot startup claimed work");server.finish();require(server.requests.empty(),"Hot startup sent heartbeat");}
 {settings.reader=[]{auto r=readings(60);r.readings.push_back(r.readings.front());return r;};int requested=0;
  WindowsRuntimeSession ambiguous("http://127.0.0.1:1",capabilities(),[]{return true;},settings);
  require(ambiguous.runContinuous([&](auto&){++requested;return ClientIterationResult{};},true)==2&&requested==0,"Ambiguous device allowed protected startup");}
 std::atomic_bool hot=false;int cancelled=0,prepared=0,heartbeats=0;
 settings.reader=[&]{return readings(hot?90:65);};
 auto foreign=WindowsEngineProcess::start(engine,{"sleep"},root/"foreign");const auto foreignId=foreign->identity();
 RunServer server([&](const auto& r){
  if(endpoint(r,"/api/client/heartbeat")){++heartbeats;require(r.find("\"temperature_c\":")!=std::string::npos&&r.find("\"power_limit_w\":290")!=std::string::npos,"Heartbeat did not publish thermal readings");if(heartbeats==1)require(r.find("\"temperature_c\":65")!=std::string::npos,"Initial heartbeat did not include current temperature");}
  else if(endpoint(r,"/api/range/progress"))hot=true;
  else if(endpoint(r,"/api/range/complete")){++cancelled;require(r.find("\"status\":\"cancelled\"")!=std::string::npos&&r.find("\"exit_code\":-2")!=std::string::npos,"Thermal stop falsely completed range");}
  else throw std::runtime_error("Thermal monitor requested other work");return std::string("{}");
 });
 WindowsRuntimeSession session(server.url(),capabilities(),[]{return true;},settings);
 const auto code=session.runContinuous([&](auto& owner){++prepared;client::ClientExecutionState s;s.active=true;s.assignmentId="thermal-assignment";s.clientId="fixture-client";s.puzzle=20;s.rangeId=1;s.target="fixture-address";s.start="0";s.end="F";s.engine="BitCrack";s.backend="cuda";s.workspace=pathUtf8(root/"owned");return owner.execute(engine,{"execution",pathUtf8(root/"gate-not-created"),pathUtf8(root/"ready"),"none"},s);},true);
 server.finish();require(code==2&&prepared==1&&heartbeats>=1&&cancelled==1,"Thermal stop did not end the owned assignment once");
 require(!client::ClientStateStore::load("cuda-7")&&!ClientRuntimeControl::running("cuda-7"),"Acknowledged thermal stop left stale state");
 require(WindowsProcessIdentity::status(foreignId.process.pid,foreignId.process.creationTime)==WindowsProcessIdentity::Status::Running,"Thermal control stopped a foreign process");foreign->stop();
 std::cout<<"WindowsThermalRuntimeTests passed: startup gate, exact heartbeat and owned orderly cancellation\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
