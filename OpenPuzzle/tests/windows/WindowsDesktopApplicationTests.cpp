#include "openpuzzle/ui/WindowsDesktopApplication.hpp"
#include "openpuzzle/ui/WindowsUiProfileStore.hpp"
#include "openpuzzle/ui/WindowsUiModel.hpp"
#include <iostream>
#include <atomic>
using namespace openpuzzle;using namespace openpuzzle::ui;namespace fs=std::filesystem;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
std::string value(const std::vector<std::string>& a,const std::string& k,std::string fallback="0"){for(std::size_t i=0;i+1<a.size();++i)if(a[i]==k)return a[i+1];return fallback;}
std::string record(const std::string& backend,int device,int threads,bool bound){
 return "{\"version\":1,\"backend\":\""+backend+"\",\"engine_sha256\":\""+std::string(64,'a')+"\",\"device\":"+std::to_string(device)+",\"device_identity\":\"synthetic\",\"selftest_passed\":true,\"threads\":"+std::to_string(threads)+",\"blocks\":1,\"points\":1,\"samples\":3,\"duration_seconds\":60,\"speed_mkeys\":123"+(backend=="cpu"?"":",\"autotuned\":true,\"finalist_validated\":true,\"candidates_tested\":2")+(bound?",\"windows_device_instance_id\":\"PCI\\\\VEN_1002&DEV_731F\\\\TEST\"":"")+"}";
}
std::string read(const fs::path& p){bool m=false;auto s=windowsFile::readPrivate(p,m,16384);return s?*s:"";}
}
int main(){try{
 const auto root=fs::temp_directory_path()/("op-desktop-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));
 fs::create_directories(root);check(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()),"Environment");
 int runs=0,measurements=0;bool fail=false;std::string gpuStatus;std::vector<std::string> runArgs;
 auto provider=[&](const std::vector<std::string>& a,std::ostream& out){
  if(a.front()=="status"){if(!gpuStatus.empty()){out<<gpuStatus;return 0;}out<<"Slot: primary\nRuntime: idle\nAssignment: none\nPending claim: none\n";return 0;}
  if(a.front()=="benchmark"){
   if(fail)return 1;
   const auto dir=root/("report-"+std::to_string(++measurements));WorkspaceSecurity::prepare(dir);
   const auto b=value(a,"--backend");const auto d=std::stoi(value(a,"--device"));const auto t=b=="cpu"?std::stoi(value(a,"--threads")):32;
   check(writePrivateFileAtomically(dir/"result.json",record(b,d,t,false)),"Write measurement");out<<"Report: "<<pathUtf8(dir)<<'\n';return 0;
  }
  if(a.front()=="bind-profile"){
   auto content=read(pathFromUtf8(value(a,"--profile")));content.insert(content.size()-1,",\"windows_device_instance_id\":\"PCI\\\\VEN_1002&DEV_731F\\\\TEST\"");
   check(writePrivateFileAtomically(pathFromUtf8(value(a,"--output")),content),"Write bound fixture");return 0;
  }
  if(a.front()=="run"){++runs;runArgs=a;return 0;}
  if(a.front()=="engine-devices"){out<<"[]";return 0;}
  return 1;
 };
 WindowsAmdTelemetryApi metrics;metrics.devices=[]{return std::vector<WindowsAmdTelemetryDevice>{{4,"AMD Radeon RX 5700 XT","PCI\\VEN_1002&DEV_731F\\TEST"}};};
 metrics.temperature=[](const auto&){return std::optional<double>{64};};metrics.power=[](const auto&){return std::optional<double>{142.75};};
 auto sensors=queryWindowsAmdTelemetry(metrics);bool failSensors=false;int samples=0;
 WindowsDesktopApplication app(provider,root/"catalog",root/"legacy",[&]{++samples;if(failSensors)throw std::runtime_error("Sensor lost");return sensors;});
 auto run=[&](std::vector<std::string> a){std::ostringstream out;const auto code=app.run(a,out);return std::pair(code,out.str());};
 check(run({"run","--backend","opencl","--device","7"}).first==1&&runs==0,"Missing profile contacted coordinator");
 check(run({"benchmark","--backend","opencl","--gpu","7","--auto","--real","--require-idle"}).first==0,"OpenCL benchmark bridge");
 const auto pointer=root/"catalog"/"opencl-7"/"selected.txt";const auto before=read(pointer);
 check(!before.empty(),"Profile selection absent");const auto saved=read(pointer.parent_path()/before);check(!saved.empty(),"Measured copy absent");
 check(run({"status","--backend","opencl","--device","7","--threads","1"}).second.find("Desktop profile.... ready")!=std::string::npos,"Status omitted valid profile");
 check(run({"status","--backend","opencl","--device","6"}).second.find("Desktop profile.... missing")!=std::string::npos,"Wrong GPU accepted");
 gpuStatus="Slot: opencl-7\nRuntime: idle\nAssignment: none\nPending claim: none\n";
 auto sensedStatus=run({"status","--backend","opencl","--device","7"});
 check(sensedStatus.first==0&&samples==1&&sensedStatus.second.find("64.0 °C (GPU)")!=std::string::npos&&sensedStatus.second.find("142.8 W / —")!=std::string::npos,"Physical AMD metrics not displayed in shared status protocol");
 const auto originalSensors=sensors;sensors.readings.push_back(sensors.readings[0]);
 check(run({"status","--backend","opencl","--device","7"}).second.find("64.0")==std::string::npos,"Duplicate sensor displayed as current");
 sensors=originalSensors;sensors.readings[0].windowsDeviceInstanceId="PCI\\VEN_1002&DEV_731F\\FOREIGN";
 check(run({"status","--backend","opencl","--device","7"}).second.find("142.8")==std::string::npos,"Foreign physical GPU shown by ordinal");
 sensors=originalSensors;sensors.complete=false;
 check(run({"status","--backend","opencl","--device","7"}).second.find("64.0")==std::string::npos,"Incomplete sensor report displayed");
 failSensors=true;check(run({"status","--backend","opencl","--device","7"}).first==0,"Sensor exception hid execution status");failSensors=false;
 sensors=originalSensors;sensors.readings[0].values.powerDrawW.reset();
 check(run({"status","--backend","opencl","--device","7"}).second.find("Power.......... —")!=std::string::npos,"Missing AMD watts reused previous reading");
 sensors=originalSensors;gpuStatus.clear();
 check(run({"run","--backend","opencl","--device","7"}).first==0&&runs==1&&value(runArgs,"--slot")=="opencl-7"&&value(runArgs,"--server")==windowsUiServer,"Run handoff");
 check(run({"run","--backend","opencl","--device","7","--server","http://127.0.0.1"}).first==1&&runs==1,"Custom coordinator allowed");
 fail=true;check(run({"benchmark","--backend","opencl","--gpu","7","--auto","--real"}).first==1&&read(pointer)==before&&read(pointer.parent_path()/before)==saved,"Failure replaced profile");fail=false;
 check(run({"benchmark","--backend","cpu","--threads","4","--real","--require-idle"}).first==0,"CPU benchmark bridge");
 check(run({"run","--backend","cpu","--threads","3"}).first==1&&run({"run","--backend","cpu","--threads","4"}).first==0,"CPU thread profile mismatch");
 check(writePrivateFileAtomically(pointer,"../../outside.json"),"Corrupt fixture write");
 check(run({"benchmark","--backend","opencl","--gpu","7","--auto","--real"}).first==1&&read(pointer)=="../../outside.json"&&read(pointer.parent_path()/before)==saved,"Damaged pointer replaced");
 check(!fs::exists(localPaths::configDirectory()/"client.id"),"Desktop test registered a client");
 // A failed CUDA probe must not erase a usable AMD/OpenCL result. Both
 // read-only probes can progress without waiting for the other backend.
 std::atomic<int> entered{0};
 WindowsDesktopApplication caps([&](const std::vector<std::string>& a,std::ostream& out){
  if(a.front()!="engine-devices")return 1;
  ++entered;const auto until=GetTickCount64()+1000;while(entered.load()<2&&GetTickCount64()<until)Sleep(1);
  if(entered.load()<2)throw std::runtime_error("GPU probes were serialized");
  if(value(a,"--backend")=="cuda")throw std::runtime_error("CUDA driver reports no device");
  out<<"[{\"engine_index\":0,\"platform_index\":0,\"native_index\":0,\"gpu\":true,\"name\":\"gfx1010:xnack-\",\"display_name\":\"AMD Radeon RX 5700 XT\",\"memory_bytes\":8573157376}]";return 0;
 },root/"caps",root/"legacy");
 std::ostringstream found;check(caps.run({"desktop-capabilities"},found)==0,"Capability command failed");
 boost::property_tree::ptree parsed;std::istringstream input(found.str());boost::property_tree::read_json(input,parsed);
 check(parsed.get_child("opencl").size()==1&&parsed.get_child("cuda").empty()&&parsed.get<int>("diagnostics.cuda.code")==1&&parsed.get<std::string>("diagnostics.cuda.message").find("no device")!=std::string::npos,"CUDA error hid AMD or lost its diagnostic");
 WindowsDesktopApplication badCaps([](const std::vector<std::string>& a,std::ostream& out){
  if(a.front()=="devices"){out<<"Windows adapter: AMD Radeon RX 5700 XT";return 0;}
  if(value(a,"--backend")=="cuda"){out<<"bad-json";return 0;}
  out<<"OpenCL engine failed: fixture error";return 1;
 },root/"caps",root/"legacy");
 std::ostringstream unavailable;check(badCaps.run({"desktop-capabilities"},unavailable)==0,"GPU failures broke the diagnostic protocol");
 input.clear();input.str(unavailable.str());parsed.clear();boost::property_tree::read_json(input,parsed);
 check(parsed.get<int>("diagnostics.cuda.code")==1&&parsed.get<int>("diagnostics.opencl.code")==1&&parsed.get<std::string>("diagnostics.opencl.message").find("fixture error")!=std::string::npos&&parsed.get<std::string>("hardware_report").find("5700 XT")!=std::string::npos,"GPU failure returned a silent empty inventory");
 fs::remove_all(root);std::cout<<"Desktop adapter passed: measured GPU/CPU profiles, immutable history, status and coordinator handoff\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
