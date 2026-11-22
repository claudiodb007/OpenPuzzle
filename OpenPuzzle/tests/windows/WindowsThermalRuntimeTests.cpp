#include "WindowsRunRequestFixture.hpp"
#include "openpuzzle/runtime/WindowsRuntimeSession.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WindowsExecutionState.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include <iostream>
using namespace openpuzzle;namespace fs=std::filesystem;
WindowsTelemetryReport readings(double t){client::ClientGpuTelemetry v;v.vendor="NVIDIA";v.deviceId="cuda-0";v.device=0;v.pciBusId="00000000:08:00.0";v.temperatureC=t;v.powerDrawW=123;v.powerLimitW=290;return {true,"fixture",{{"fixture GPU",v,"12345678-1234-5678-9abc-123456789abc"}}};}
client::ClientHeartbeat capabilities(){client::ClientHeartbeat h;h.clientId="fixture-client";h.platform="windows";h.version="1.0.46";h.cpu={"fixture CPU",1,1};h.gpus={{"cuda","fixture GPU",1}};h.engines={{"BitCrack","cuda",true,true}};return h;}
bool endpoint(const std::string& r,const char* p){return r.starts_with(std::string("POST ")+p+' ');}
int wmain(int argc,wchar_t** argv){try{
 require(argc==2,"Engine fixture required");Winsock sockets;const auto engine=fs::absolute(argv[1]);
 const auto root=fs::temp_directory_path()/pathFromUtf8("op-thermal-runtime-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));fs::create_directory(root);
 require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str())&&SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT","cuda-7"),"Thermal environment failed");

 // AMD-only host: no NVIDIA library or watt-limit reading is required. The
 // normal runtime must publish temperature even with critical stopping off.
 for(bool withPower:{false,true}){
  const std::string pnp="PCI\\VEN_1002&DEV_731F\\RIG-A";
  WindowsAmdTelemetryApi api;api.devices=[&]{return std::vector<WindowsAmdTelemetryDevice>{{4,"AMD Radeon RX 5700 XT",pnp}};};
  api.temperature=[](const auto&){return std::optional<double>{63.5};};
  if(withPower)api.power=[](const auto&){return std::optional<double>{142.75};};
  WindowsRuntimeThermalSettings amd;amd.policy.enabled=false;amd.deviceName="gfx1010:xnack-";amd.deviceWindowsInstanceId=pnp;
  amd.reader=[&]{auto r=mergeWindowsTelemetryReports({false,"NVML missing",{}},queryWindowsAmdTelemetry(api),false,true);
   require(r.bindDevice("opencl",0,"","",pnp),"AMD sensor did not bind to OpenCL index");return r;};
  auto h=capabilities();h.gpus={{"opencl","AMD Radeon RX 5700 XT",8176}};h.engines={{"BitCrack","opencl",true,true}};
  int heartbeats=0,prepared=0;
  RunServer server([&](const auto& r){
   require(endpoint(r,"/api/client/heartbeat"),"AMD read-only fixture sent unrelated request");++heartbeats;
   require(r.find("\"temperature_c\":63.50")!=std::string::npos&&r.find("\"vendor\":\"AMD\"")!=std::string::npos&&
    r.find("\"device_id\":\"opencl-0\"")!=std::string::npos,"AMD temperature missing from heartbeat");
   require(r.find("\"power_limit_w\":null")!=std::string::npos&&r.find(withPower?"\"power_draw_w\":142.75":"\"power_draw_w\":null")!=std::string::npos,"AMD fixture invented watts");
   require(r.find("RIG-A")==std::string::npos,"Internal PNP identity leaked into heartbeat");return std::string("{}");
  });
  WindowsRuntimeSession session(server.url(),h,[]{return true;},amd);
  require(session.runContinuous([&](auto&){++prepared;return ClientIterationResult{};},true)==0,"AMD telemetry blocked unprotected execution");
  server.finish();require(heartbeats>=1&&prepared==1,"AMD runtime did not publish temperature");
 }
 WindowsRuntimeThermalSettings settings;settings.policy={true,true,75,85};settings.deviceName="fixture GPU";settings.interval=std::chrono::milliseconds(1);settings.reader=[]{return readings(88);};
 {int requested=0;RunServer server([](const auto&){throw std::runtime_error("Hot startup must not contact server");return std::string{};});
  WindowsRuntimeSession hot(server.url(),capabilities(),[]{return true;},settings);
  require(hot.runContinuous([&](auto&){++requested;return ClientIterationResult{};},true)==2&&requested==0,"Hot startup claimed work");server.finish();require(server.requests.empty(),"Hot startup sent heartbeat");}
 {settings.reader=[]{auto r=readings(60);r.readings.push_back(r.readings.front());return r;};int requested=0;
  WindowsRuntimeSession ambiguous("http://127.0.0.1:1",capabilities(),[]{return true;},settings);
  require(ambiguous.runContinuous([&](auto&){++requested;return ClientIterationResult{};},true)==2&&requested==0,"Ambiguous device allowed protected startup");}
 std::atomic_bool hot=false;int cancelled=0,prepared=0,heartbeats=0;
 settings.deviceUuid="12345678-1234-5678-9abc-123456789abc";settings.devicePciBusId="00000000:08:00.0";
 settings.reader=[&]{auto r=readings(hot?90:65);auto other=r.readings.front();other.uuid="22345678-1234-5678-9abc-123456789abc";other.values.device=1;other.values.deviceId="cuda-1";other.values.pciBusId="00000000:09:00.0";other.values.temperatureC=95;r.readings.insert(r.readings.begin(),other);return r;};
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
