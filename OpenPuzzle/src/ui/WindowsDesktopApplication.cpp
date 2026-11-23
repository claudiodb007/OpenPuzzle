#include "openpuzzle/ui/WindowsDesktopApplication.hpp"
#include "openpuzzle/ui/WindowsDesktopProtocol.hpp"
#include "openpuzzle/ui/WindowsUiProfileStore.hpp"
#include "openpuzzle/ui/WindowsUiModel.hpp"
#include "openpuzzle/engines/WindowsEngineDiagnostics.hpp"
#include "openpuzzle/engines/WindowsEngineDeviceList.hpp"
#include "openpuzzle/core/WindowsCliApplication.hpp"
#include "openpuzzle/runtime/WindowsRunSession.hpp"
#include "openpuzzle/runtime/WindowsExecutionState.hpp"
#include "openpuzzle/runtime/WindowsVanityRun.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/hardware/WindowsGpuTelemetry.hpp"
#include <iomanip>
#include <locale>
#include <atomic>
#include <set>
#include <future>
namespace openpuzzle::ui {
namespace fs=std::filesystem;
namespace {
std::string slot(const std::string& backend,int device){return backend=="cpu"?"primary":backend+"-"+std::to_string(device);}
std::string option(const std::vector<std::string>& a,const std::string& key,const std::string& fallback){
 for(std::size_t i=0;i<a.size();++i)if(a[i]==key){if(i+1==a.size())throw std::invalid_argument("Missing option value");return a[i+1];}return fallback;
}
int integer(const std::vector<std::string>& a,const std::string& key,int fallback){return cliIntegerArgument(a,{key},fallback);}
void backend(const std::string& b){if(b!="cpu"&&b!="cuda"&&b!="opencl")throw std::invalid_argument("This Windows engine is not available");}
struct Tee:std::streambuf{std::ostream& out;std::string text;explicit Tee(std::ostream& o):out(o){}
 std::streamsize xsputn(const char* s,std::streamsize n)override{text.append(s,static_cast<std::size_t>(n));if(text.size()>524288)text.erase(0,text.size()-262144);out.write(s,n);out.flush();return n;}
 int overflow(int c)override{if(c!=traits_type::eof()){char ch=static_cast<char>(c);xsputn(&ch,1);}return traits_type::not_eof(c);}};
}
WindowsDesktopApplication::WindowsDesktopApplication(Command command,fs::path root,fs::path legacy,WindowsRuntimeThermalSettings::Reader sensors)
 :command_(std::move(command)),sensors_(std::move(sensors)),root_(root.empty()?localPaths::dataDirectory()/"desktop-profiles":std::move(root)),
 legacy_(legacy.empty()?WindowsEngineDiagnostics::defaultFolder().parent_path():std::move(legacy)){
 if(!sensors_)sensors_=WindowsGpuTelemetry::inspect;
 if(!command_)command_=[](const auto& a,std::ostream& o){return WindowsCliApplication().run(a,o);};
}
fs::path WindowsDesktopApplication::profile(const std::string& b,int device,int threads)const{
 backend(b);if(device<0||device>255)throw std::invalid_argument("Invalid desktop device");
 const auto dir=root_/slot(b,device);bool missing=false;const auto selected=windowsFile::readPrivate(dir/"selected.txt",missing,256);
 auto check=[&](const fs::path& path)->fs::path{bool absent=false;const auto bytes=windowsFile::readPrivate(path,absent,16384);if(!bytes)throw std::runtime_error("Selected profile unavailable; preserved");
  const auto p=WindowsRunProfile::parse(*bytes);if(p.backend!=b||p.device!=device||(b=="cpu"?p.threads!=threads:!p.hasPhysicalBinding()||!p.autoTuned))return {};return path;};
 if(selected){if(selected->empty()||selected->size()>128||!selected->starts_with("measured-")||!selected->ends_with(".json")||selected->find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-.")!=std::string::npos)throw std::runtime_error("Invalid selected profile; preserved");return check(dir/pathFromUtf8(*selected));}
 if(!missing)throw std::runtime_error("Unreadable profile selection; preserved");
 const auto old=b=="cpu"?std::optional<fs::path>(legacy_/"profiles"/"cpu"/"result.json"):loadWindowsUiAutomaticProfile(legacy_,b);
 if(old){bool absent=false;const auto raw=windowsFile::readPrivate(*old,absent,16384);if(raw)return check(*old);if(!absent)throw std::runtime_error("Previous profile is unreadable; preserved");}
 return {};
}
fs::path WindowsDesktopApplication::publish(const fs::path& source){
 bool missing=false;const auto raw=windowsFile::readPrivate(source,missing,16384);if(!raw)throw std::runtime_error("Measured result unavailable; preserved");
 const auto p=WindowsRunProfile::parse(*raw);if(p.backend!="cpu"&&(!p.autoTuned||!p.hasPhysicalBinding()))throw std::runtime_error("GPU measurement lacks validated physical binding");
 const auto dir=root_/slot(p.backend,p.device);WorkspaceSecurity::prepare(dir);windowsFile::Lock lock((dir/"selected.lock").wstring());if(!lock.locked())throw std::runtime_error("Profile selection occupied");
 // Validate an earlier pointer, including its contents, before publishing a new one.
 const auto old=windowsFile::readPrivate(dir/"selected.txt",missing,256);if(old||!missing)(void)profile(p.backend,p.device,p.threads);
 static std::atomic<unsigned long long> sequence{0};const auto name="measured-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64())+'-'+std::to_string(sequence++)+".json";
 const auto target=dir/name;
 if(fs::exists(target)||!writePrivateFileAtomically(target,*raw))throw std::runtime_error("Measured profile could not be saved");
 const auto verify=windowsFile::readPrivate(target,missing,16384);if(!verify||*verify!=*raw||!writePrivateFileAtomically(dir/"selected.txt",name))throw std::runtime_error("Profile selection publication failed; prior data retained");return target;
}
int WindowsDesktopApplication::benchmark(const std::vector<std::string>& args,std::ostream& out){
 std::vector<std::string> native{"benchmark"};std::set<std::string> seen;
 for(std::size_t i=1;i<args.size();++i){auto a=args[i];if(a=="--gpu")a="--device";if(!seen.insert(a).second)throw std::invalid_argument("Duplicate benchmark option");
  if(a=="--require-idle")continue;
  if(a=="--real"||a=="--auto"){native.push_back(a);continue;}
  if(a!="--backend"&&a!="--device"&&a!="--threads"&&a!="--seconds")throw std::invalid_argument("Unsupported Windows benchmark option");
  if(++i==args.size())throw std::invalid_argument("Missing benchmark option");native.insert(native.end(),{a,args[i]});
 }
 const auto parsed=WindowsEngineCheckOptions::parse(native);
 for(const auto& s:ClientRuntimeControl::executionSlots())if(ClientRuntimeControl::runtimeIdentityStatus(s)!=ClientRuntimeControl::RuntimeIdentityStatus::Inactive)throw std::runtime_error("Stop active or uncertain runtimes before measuring");
 if(parsed.backend!="cpu"&&!parsed.automatic)throw std::invalid_argument("Use automatic measured GPU preparation");
 Tee capture(out);std::ostream stream(&capture);const auto code=command_(native,stream);if(code||!parsed.real)return code;
 auto measured=pathFromUtf8(windowsUiBenchmarkReport(capture.text))/"result.json";
 bool missing=false;const auto raw=windowsFile::readPrivate(measured,missing,16384);if(!raw)throw std::runtime_error("Missing measured benchmark result");
 auto p=WindowsRunProfile::parse(*raw);if(p.backend!=parsed.backend||p.device!=parsed.device||(p.backend=="cpu"?p.threads!=parsed.threads:!p.autoTuned))throw std::runtime_error("Measurement differs from requested hardware");
 if(p.backend!="cpu"){const auto bound=measured.parent_path()/"desktop-bound.json";
  const auto result=command_({"bind-profile","--profile",pathUtf8(measured),"--output",pathUtf8(bound)},out);if(result)return result;measured=bound;}
 out<<"Desktop profile saved: "<<pathUtf8(publish(measured))<<'\n';return 0;
}
int WindowsDesktopApplication::run(const std::vector<std::string>& args,std::ostream& out){try{
 if(args.empty())throw std::invalid_argument("Desktop command required");const auto& command=args.front();
 if(command=="desktop-capabilities"){
  if(args.size()!=1)throw std::invalid_argument("Invalid capability request");
  struct Probe {std::string devices="[]",detail;int code=1;};
  const auto bounded=[](std::string value){
   if(value.size()>4096)value.resize(4096);
   if(value.empty())return std::string("No diagnostic output was returned");
   if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0)>0)return value;
   // Engine/driver console output can use the Windows ANSI code page.
   const auto count=MultiByteToWideChar(CP_ACP,0,value.data(),static_cast<int>(value.size()),nullptr,0);
   if(!count)return std::string("Diagnostic output could not be decoded");
   std::wstring wide(count,0);MultiByteToWideChar(CP_ACP,0,value.data(),static_cast<int>(value.size()),wide.data(),count);
   const auto size=WideCharToMultiByte(CP_UTF8,0,wide.data(),count,nullptr,0,nullptr,nullptr);
   std::string result(size,0);WideCharToMultiByte(CP_UTF8,0,wide.data(),count,result.data(),size,nullptr,nullptr);return result;
  };
  const auto probe=[&](const char* b){Probe result;std::ostringstream list;
   try{result.code=command_({"engine-devices","--backend",b},list);
    if(result.code==0)result.devices=formatWindowsEngineDevices(parseWindowsEngineDevices(list.str()));
    else result.detail=bounded(list.str());
   }catch(const std::exception& e){result.code=1;result.detail=bounded(e.what());}
   return result;
  };
  // Only independent read-only diagnostic dispatches run concurrently. The
  // default callback constructs a separate native CLI instance for each call;
  // searches, slot selection and commands that mutate the environment do not.
  auto cudaTask=std::async(std::launch::async,probe,"cuda");
  auto openclTask=std::async(std::launch::async,probe,"opencl");
  const auto cuda=cudaTask.get(),opencl=openclTask.get();
  out<<"{\"cuda\":"<<cuda.devices<<",\"opencl\":"<<opencl.devices<<",\"diagnostics\":{";
  bool first=true;for(const auto& item:{std::pair{"cuda",cuda},std::pair{"opencl",opencl}}){
   if(!first)out<<',';first=false;
   out<<'"'<<item.first<<"\":{\"code\":"<<item.second.code<<",\"message\":\""<<escapeJsonString(item.second.detail)<<"\"}";
  }
  out<<'}';
  if(cuda.devices=="[]"&&opencl.devices=="[]"){
   std::ostringstream hardware;try{(void)command_({"devices"},hardware);}catch(const std::exception& e){hardware<<e.what();}
   if(!hardware.str().empty())out<<",\"hardware_report\":\""<<escapeJsonString(bounded(hardware.str()))<<'"';
  }
  out<<"}\n";return 0;
 }
 if(command=="benchmark")return benchmark(args,out);
 if(command=="status"){
  const auto b=option(args,"--backend","cuda");backend(b);const auto device=integer(args,"--device",0),threads=integer(args,"--threads",1);
  std::set<std::string> seen;for(std::size_t i=1;i<args.size();i+=2)if(i+1==args.size()||(args[i]!="--backend"&&args[i]!="--device"&&args[i]!="--threads")||!seen.insert(args[i]).second)throw std::invalid_argument("Invalid desktop status options");
  std::ostringstream raw;const auto code=command_({"status","--all"},raw);auto slots=windowsDesktopSlots(raw.str(),code);
  for(auto& s:slots){const auto state=client::ClientStateStore::load(s.slot);if(state&&s.fields["Assignment"]==state->assignmentId){
   s.fields["Engine"]=state->engine;s.fields["Backend"]=state->backend;s.fields["Puzzle"]=std::to_string(state->puzzle);s.fields["Range"]=std::to_string(state->rangeId);s.fields["Workspace"]=state->workspace;
   const auto inspected=client::ExecutionSyncService().inspect(s.slot);
   if(inspected.identityUnavailable){s.status="identity unavailable";}
   else if(inspected.solutionFound&&inspected.hasState&&inspected.state.assignmentId==state->assignmentId){s.status="solution found";}
   else if(s.status=="stopped"){
    s.fields["Recovery"]="yes";
    if (state->engine == "Vanity") {
     s.fields["Device"] = std::to_string(state->device);
     const auto record = windowsExecutionRecord(*state);
     if (record) {
      const auto generation = WindowsEngineProcess::generationPath(pathFromUtf8(state->workspace), *record);
      const auto proof = readWindowsVanityEvidence(*state, generation);
      if (proof && !proof->solution)
       s.fields["Checkpoint"] = "verified locally (" + std::to_string(state->vanityResumeOffset + proof->checked) + " keys checked)";
     }
    }
   }
  }const auto speed=s.fields.find("Speed MKey/s");if(speed!=s.fields.end()&&speed->second!="unavailable")s.fields["Speed"]=speed->second+" MKey/s";}
  std::optional<WindowsTelemetryReport> sensors;
  const auto number=[](double value,const char* unit){std::ostringstream text;text.imbue(std::locale::classic());text<<std::fixed<<std::setprecision(1)<<value<<unit;return text.str();};
  for(auto& s:slots){
   const auto dash=s.slot.find('-');if(dash==std::string::npos||s.status=="identity unavailable")continue;
   const auto gpuBackend=s.slot.substr(0,dash);if(gpuBackend!="cuda"&&gpuBackend!="opencl")continue;
   s.fields["Temperature"]="—";s.fields["Power"]="—";
   try{
    const auto gpuDevice=cliIntegerArgument({"--device",s.slot.substr(dash+1)},{"--device"},0);
    const auto path=profile(gpuBackend,gpuDevice,1);if(path.empty())continue;
    bool absent=false;const auto raw=windowsFile::readPrivate(path,absent,16384);if(!raw)continue;
    const auto bound=WindowsRunProfile::parse(*raw);if(!bound.hasPhysicalBinding())continue;
    // Read all sensors once per refresh. Display is bound to the measured
    // physical profile, never ADLX/NVML order or the selected name alone.
    if(!sensors){sensors.emplace();*sensors=sensors_();}
    const auto* reading=sensors->complete?sensors->find(bound.gpuUuid,bound.pciBusId,bound.windowsDeviceInstanceId):nullptr;
    if(!reading||!reading->values.valid())continue;
    const auto& value=reading->values;
    if(value.temperatureC)s.fields["Temperature"]=number(*value.temperatureC," °C (GPU)");
    if(value.powerDrawW||value.powerLimitW)s.fields["Power"]=(value.powerDrawW?number(*value.powerDrawW," W"):"—")+" / "+(value.powerLimitW?number(*value.powerLimitW," W"):"—");
   }catch(...){/* Sensor/profile failures must clear readings, not hide status. */}
  }
  bool ready=false;std::string issue;try{ready=!profile(b,device,threads).empty();}catch(const std::exception& e){issue=e.what();}
  out<<windowsDesktopStatus(slots,ready);if(!issue.empty())out<<"Profile issue...... "<<issue<<'\n';return 0;
 }
 if(command=="run"){
  auto options=RunCommandOptions::parse(args);if(options.backend.empty())options.backend="cuda";backend(options.backend);
  if(!options.profile.empty()||(!options.server.empty()&&options.server!=windowsUiServer))throw std::invalid_argument("The desktop uses its measured profile and official coordinator");
  const auto path=profile(options.backend,options.device,options.threads);if(path.empty())throw std::runtime_error("Run Benchmark first for this GPU/CPU selection; existing profiles retained");
  auto native=args;native.insert(native.end(),{"--profile",pathUtf8(path)});if(options.server.empty())native.insert(native.end(),{"--server",windowsUiServer});
  native.insert(native.end(),{"--slot",slot(options.backend,options.device)});return command_(native,out);
 }
 if(command=="stop"||command=="safestop"){
  if(args.size()!=1)throw std::invalid_argument("Desktop stop accepts no options");bool any=false,ok=true;
  for(const auto& s:ClientRuntimeControl::executionSlots())if(ClientRuntimeControl::runtimeIdentityStatus(s)!=ClientRuntimeControl::RuntimeIdentityStatus::Inactive){any=true;ok=(command_({command,"--slot",s},out)==0)&&ok;}
  if(!any)out<<"No active runtime; existing data retained\n";return ok?0:1;
 }
 if(command=="doctor"){if(args.size()!=1)throw std::invalid_argument("Invalid doctor options");return command_({"doctor","--offline"},out);}
 if(command=="selftest"||command=="gpu-control"||command=="recover"||command=="engine-devices"||command=="telemetry"||command=="version")return command_(args,out);
 throw std::invalid_argument("This command is not available in the Windows desktop");
 }catch(const std::exception& e){out<<"Windows desktop: "<<e.what()<<"\nExisting local data preserved\n";return 1;}}
}
