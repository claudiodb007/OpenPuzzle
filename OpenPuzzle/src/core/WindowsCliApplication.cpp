#include "openpuzzle/core/WindowsCliApplication.hpp"
#include "openpuzzle/hardware/GpuControl.hpp"
#include "openpuzzle/ui/WindowsUiProgress.hpp"
#include "openpuzzle/engines/WindowsEngineDiagnostics.hpp"
#include "openpuzzle/runtime/WindowsRunProvider.hpp"
#include "openpuzzle/runtime/WindowsAssignmentRecovery.hpp"
#include "openpuzzle/runtime/WindowsProfileGpuBinding.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/client/HttpRequestPolicy.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/ExecutionSlot.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include <charconv>
#include <cmath>
#include <algorithm>
#include <set>
#include <stdexcept>
namespace openpuzzle {
namespace fs=std::filesystem;
namespace {
using Control=ClientRuntimeControl;using Store=client::ClientStateStore;
int thermalCommand(const std::vector<std::string>& args,std::ostream& out){
 std::optional<bool> enabled,stop;std::optional<double> warning,critical;std::set<std::string> seen;
 for(std::size_t i=1;i<args.size();++i){const auto& a=args[i];if(!seen.insert(a).second)throw std::invalid_argument("Duplicate thermal option");
  if(a=="--enable"||a=="--disable"){if(enabled)throw std::invalid_argument("Conflicting thermal modes");enabled=a=="--enable";}
  else if(a=="--stop-on-critical"||a=="--no-stop-on-critical"){if(stop)throw std::invalid_argument("Conflicting thermal stop modes");stop=a=="--stop-on-critical";}
  else if(a=="--warning"||a=="--critical"){
   if(++i==args.size())throw std::invalid_argument("Thermal threshold requires a number");const auto& text=args[i];double value=0;const auto r=std::from_chars(text.data(),text.data()+text.size(),value);
   if(r.ec!=std::errc{}||r.ptr!=text.data()+text.size()||!std::isfinite(value))throw std::invalid_argument("Invalid thermal threshold");
   (a=="--warning"?warning:critical)=value;
  }else throw std::invalid_argument("Unknown thermal option");
 }
 auto config=ConfigurationManager::loadChecked();if(!config)throw std::runtime_error("Configuration is unreadable; preserved");
 const auto apply=[&](Configuration& c){auto& p=c.gpu.thermal;if(enabled)p.enabled=*enabled;if(stop)p.stopOnCritical=*stop;if(warning)p.warningC=*warning;if(critical)p.criticalC=*critical;if(!GpuThermalPolicy::valid(p))throw std::invalid_argument("Thermal thresholds must satisfy 30 <= warning <= 110 and warning < critical <= 120");};
 apply(*config);
 if(args.size()>1){
  const auto path=pathFromUtf8(ConfigurationManager::configPath());WorkspaceSecurity::prepare(path.parent_path());
  windowsFile::Lock lock(path.wstring()+L".thermal.lock");if(!lock.locked())throw std::runtime_error("Thermal configuration lock unavailable");
  config=ConfigurationManager::loadChecked();if(!config)throw std::runtime_error("Configuration changed or became unreadable; preserved");apply(*config);
  if(!ConfigurationManager::save(*config))throw std::runtime_error("Thermal configuration write failed; previous configuration preserved");
 }
 const auto& p=config->gpu.thermal;out<<"Thermal mode: "<<(p.enabled?"enabled":"disabled")<<"\nWarning C: "<<p.warningC<<"\nCritical C: "<<p.criticalC<<"\nStop on critical: "<<(p.stopOnCritical?"yes":"no")<<"\nScope: uniquely identified selected GPU; CPU is not thermally gated\n";
 if(args.size()>1)out<<"Saved for subsequent runs; no current runtime changed\n";return 0;
}
bool present(const fs::path& path){std::error_code e;auto s=fs::symlink_status(path,e);return (e&&e!=std::errc::no_such_file_or_directory)||fs::exists(s);}
std::string environmentSlot(){
 char value[64]{};SetLastError(ERROR_SUCCESS);
 const auto count=GetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT",value,sizeof(value));
 if(!count&&GetLastError()==ERROR_ENVVAR_NOT_FOUND)return "primary";
 if(!count||count>=sizeof(value))throw std::invalid_argument("Execution slot environment is empty or too long");
 std::string slot(value,count);
 if(!ExecutionSlot::valid(slot))throw std::invalid_argument("Invalid OPENPUZZLE_EXECUTION_SLOT; no fallback to another slot");return slot;
}
// RunSession reads the established process environment. Restore its value on
// every return or exception; console dispatch is serialized, not thread-safe.
class SlotGuard {
 std::wstring previous_;bool existed_=false;
public:
 explicit SlotGuard(const std::string& slot){
  const auto size=GetEnvironmentVariableW(L"OPENPUZZLE_EXECUTION_SLOT",nullptr,0);
  if(size){previous_.resize(size);auto n=GetEnvironmentVariableW(L"OPENPUZZLE_EXECUTION_SLOT",previous_.data(),size);
   if(!n||n>=size)throw std::runtime_error("Execution slot environment changed");previous_.resize(n);existed_=true;}
  const std::wstring wide(slot.begin(),slot.end());if(!SetEnvironmentVariableW(L"OPENPUZZLE_EXECUTION_SLOT",wide.c_str()))throw std::runtime_error("Unable to select execution slot");
 }
 ~SlotGuard(){SetEnvironmentVariableW(L"OPENPUZZLE_EXECUTION_SLOT",existed_?previous_.c_str():nullptr);}
};
bool status(const std::string& slot,std::ostream& out){
 const auto runtime=Control::runtimeIdentityStatus(slot);
 const auto state=client::ExecutionSyncService().inspect(slot);
 const bool pending=present(WindowsRunSession::pendingClaimPath(slot));
 out<<"Slot: "<<slot<<"\nRuntime: "<<(runtime==Control::RuntimeIdentityStatus::Running?"running":runtime==Control::RuntimeIdentityStatus::Unavailable?"unknown":"idle")
  <<"\nAssignment: "<<(state.identityUnavailable?"identity unavailable":state.hasState?state.state.assignmentId:present(Store::path(slot))?"unreadable state preserved":"none")
  <<"\nPending claim: "<<(pending?"preserved":"none")<<'\n';
 // The inspected progress is already bound to the verified Windows process
 // generation. Never use the benchmark profile or a legacy workspace log.
 const auto progress=runtime==Control::RuntimeIdentityStatus::Running&&state.running&&!state.identityUnavailable&&!state.solutionFound&&state.hasProgress
  ?ui::WindowsUiProgress::measured(state.progress.speedMKeys,state.progress.keysChecked):ui::WindowsUiProgress{};
 out<<progress.protocol();
 return runtime==Control::RuntimeIdentityStatus::Unavailable||state.identityUnavailable||pending||(!state.hasState&&present(Store::path(slot)));
}
std::set<std::string> slots(const std::string& selected){
 std::set<std::string> all{selected};for(const auto& slot:Control::executionSlots())if(ExecutionSlot::valid(slot))all.insert(slot);
 const auto folder=localPaths::dataDirectory()/"pending-claims";
 if(!windowsFile::safePath(folder))throw std::runtime_error("Pending claim directory is unavailable");
 std::error_code e;fs::directory_iterator it(folder,e),end;
 if(e){if(e==std::errc::no_such_file_or_directory)return all;throw std::runtime_error("Pending claim directory cannot be inspected");}
 for(;it!=end;it.increment(e)){
  if(e)throw std::runtime_error("Pending claim directory changed during inspection");
  const auto p=it->path();const auto slot=pathUtf8(p.stem());if(p.extension()==".json"&&ExecutionSlot::valid(slot))all.insert(slot);
 }if(e)throw std::runtime_error("Pending claim directory inspection failed");return all;
}
}
WindowsCliApplication::WindowsCliApplication(WindowsRunDependencies dependencies,std::function<WindowsGpuInventory()> discovery,std::filesystem::path engineFolder,WindowsRuntimeThermalSettings::Reader telemetry)
 :dependencies_(std::move(dependencies)),engineFolder_(std::move(engineFolder)),discovery_(std::move(discovery)),telemetry_(std::move(telemetry)){}
int WindowsCliApplication::run(const std::vector<std::string>& arguments,std::ostream& out){
 try{
  std::vector<std::string> args;std::string selected;bool explicitSlot=false;
  for(std::size_t i=0;i<arguments.size();++i){const auto& a=arguments[i];if(a.empty()||a.find('\0')!=std::string::npos)throw std::invalid_argument("Empty or NUL console argument");
   if(a=="--slot"){if(explicitSlot||i+1==arguments.size()||!ExecutionSlot::valid(arguments[i+1]))throw std::invalid_argument("--slot requires one recognized execution slot");selected=arguments[++i];explicitSlot=true;}
   else args.push_back(a);
  }
  const auto command=args.empty()?std::string("help"):args.front();
  if(command=="help"||command=="--help"||command=="-h"||command=="version"||command=="--version"||command=="-V"){
   if(args.size()>1||explicitSlot)throw std::invalid_argument("Information command takes no additional arguments");
   if(command=="version"||command=="--version"||command=="-V")out<<"OpenPuzzle 1.0.46 Windows CLI preview\n";
   else out<<"OpenPuzzle Windows CLI preview\nhelp | version | devices | paths | doctor --offline | status [--all] [--slot NAME]\nstop | safestop [--slot NAME]\ngpu-control list | apply --vendor VENDOR --pci PCI [--uuid UUID] --power VALUE|--power-default|--fan auto|PERCENT\nbind-profile --profile FILE --output NEW_FILE\ntelemetry | thermal [--enable|--disable] [--warning C] [--critical C] [--stop-on-critical|--no-stop-on-critical]\nrun [puzzle] [options] [--slot NAME]\nselftest --backend cpu|cuda|opencl [--device N]\nbenchmark --backend cpu|cuda|opencl [--real] [--seconds 10..60] [--threads N]\nGPU automatic tuning: benchmark --backend cuda|opencl --auto [--real] [--seconds 10..30]\nCPU: KeyHunt | GPU: BitCrack CUDA/OpenCL\nRun requires --profile result.json and --server URL. recover [--server URL] [--slot NAME] reconciles preserved work without claiming again.\nNo automatic start or production request is made by local commands.\n";
   return 0;
  }
  if(command=="selftest"||command=="benchmark"){
   if(explicitSlot)throw std::invalid_argument("Engine diagnostics do not use execution slots");
   return WindowsEngineDiagnostics(engineFolder_,discovery_).run(args,out);
  }
  if(command=="bind-profile"){
   if(explicitSlot)throw std::invalid_argument("Profile binding does not use execution slots");
   return bindWindowsGpuProfile(args,engineFolder_,discovery_,telemetry_,out);
  }
  if(command=="gpu-control"){
   if(explicitSlot)throw std::invalid_argument("GPU controls use physical GPUs, not execution slots");
   return gpuControlCommand({args.begin()+1,args.end()},out);
  }
  if(command=="thermal"){
   if(explicitSlot)throw std::invalid_argument("Thermal configuration applies to subsequent GPU runs, not an execution slot");return thermalCommand(args,out);
  }
  if(command=="telemetry"){
   if(args.size()!=1||explicitSlot)throw std::invalid_argument("Telemetry takes no additional options");
   const auto config=ConfigurationManager::loadChecked();if(!config)throw std::runtime_error("Configuration is unreadable; preserved");
   const auto report=telemetry_?telemetry_():WindowsTelemetryReport{};out<<formatWindowsTelemetry(report,config->gpu.thermal);
   return report.complete&&std::any_of(report.readings.begin(),report.readings.end(),[](const auto& r){return r.values.valid();})?0:2;
  }
  if(command=="devices"){
   if(args.size()!=1||explicitSlot)throw std::invalid_argument("Devices takes no options or execution slot");
   if(!discovery_)throw std::runtime_error("Hardware discovery provider is missing");
   const auto inventory=discovery_();out<<formatWindowsGpuInventory(inventory);
   return windowsGpuDiscoveryHasError(inventory)?2:0;
  }
  if(command=="recover"){
   std::string server;
   if(args.size()!=1){if(args.size()!=3||args[1]!="--server"||!client::HttpRequestPolicy::endpoint(args[2]))throw std::invalid_argument("Recover accepts only an optional --server URL");server=args[2];}
   if(!explicitSlot)selected=environmentSlot();SlotGuard guard(selected);
   return recoverWindowsAssignment(selected,server,out);
  }
  // Validate the entire command before reading paths or changing a slot.
  if(command=="paths"||command=="stop"||command=="safestop"){if(args.size()!=1)throw std::invalid_argument("Local command takes no extra options");}
  else if(command=="doctor"){if(args.size()>2||(args.size()==2&&args[1]!="--offline"))throw std::invalid_argument("Doctor supports only --offline");}
  else if(command=="status"){if(args.size()>2||(args.size()==2&&args[1]!="--all"))throw std::invalid_argument("Status supports only --all");}
  else (void)RunCommandOptions::parse(args);
  if(!explicitSlot)selected=environmentSlot();
  if(command=="paths"){
   out<<"Configuration: "<<pathUtf8(localPaths::configDirectory())<<"\nLocal data: "<<pathUtf8(localPaths::dataDirectory())
    <<"\nExecution state: "<<pathUtf8(Store::path(selected))<<"\nPending claim: "<<pathUtf8(WindowsRunSession::pendingClaimPath(selected))<<'\n';return 0;
  }
  if(command=="status"){
   bool uncertain=false;const auto found=args.size()==2?slots(selected):std::set<std::string>{selected};
   for(const auto& slot:found)uncertain=status(slot,out)||uncertain;
   return uncertain?2:0;
  }
  if(command=="stop"||command=="safestop"){
   const bool ok=command=="stop"?Control::requestStop(selected):Control::requestSafeStop(selected);
   out<<"Slot: "<<selected<<'\n'<<(ok?"Cooperative request recorded; completion is not yet confirmed":"No verified runtime accepted the request; state preserved")<<'\n';return ok?0:1;
  }
  if(command=="doctor"){
   out<<"OpenPuzzle Windows Doctor (offline)\nPlatform: Windows x64\n";
   SYSTEM_INFO system{};GetNativeSystemInfo(&system);MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);
   const auto threads=GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
   if(system.wProcessorArchitecture!=PROCESSOR_ARCHITECTURE_AMD64||!threads||!GlobalMemoryStatusEx(&memory))throw std::runtime_error("Native x64 CPU/memory information unavailable");
   out<<"Logical processors: "<<threads<<"\nMemory MiB: "<<memory.ullTotalPhys/1048576<<"\nConfiguration path: "<<ConfigurationManager::configPath()<<'\n';
   const auto configPath=pathFromUtf8(ConfigurationManager::configPath());
   if(!windowsFile::safePath(configPath))throw std::runtime_error("Configuration path is unavailable; existing data preserved");
   if(present(configPath)){
    if(!ConfigurationManager::loadChecked())throw std::runtime_error("Existing configuration is invalid or unreadable; preserved without replacement");out<<"Configuration: readable\n";
   }else out<<"Configuration: missing; defaults not written\n";
   const auto config=ConfigurationManager::loadChecked();if(!config)throw std::runtime_error("Configuration became unreadable; preserved");
   out<<"Local data: "<<pathUtf8(localPaths::dataDirectory())<<"\nServer connection: SKIPPED\n"<<formatWindowsTelemetry(telemetry_?telemetry_():WindowsTelemetryReport{},config->gpu.thermal);
   if(!discovery_)throw std::runtime_error("Hardware discovery provider is missing");
   const auto inventory=discovery_();out<<formatWindowsGpuInventory(inventory);
   WindowsEngineDiagnostics(engineFolder_).describe(out);
   const bool uncertain=status(selected,out),hardwareError=windowsGpuDiscoveryHasError(inventory);
   out<<"Status: "<<(uncertain?"BLOCKED (local state preserved)":hardwareError?"HARDWARE DIAGNOSTIC UNAVAILABLE":"LOCAL DIAGNOSTICS PASSED; launch requires a measured profile")<<'\n';return uncertain||hardwareError?2:0;
  }
  SlotGuard guard(selected);
  if(!dependencies_.preflight||!dependencies_.capabilities){
   auto prepared=prepareWindowsRun(RunCommandOptions::parse(args),engineFolder_);
   return WindowsRunSession(std::move(prepared.dependencies)).runPrepared(std::move(prepared.options),out);
  }
  return WindowsRunSession(dependencies_).run(args,out);
 }catch(const std::exception& error){out<<"Windows CLI refused: "<<error.what()<<'\n';return 1;}
}
}
