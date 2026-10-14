#pragma once
#include "openpuzzle/engines/WindowsRunProfile.hpp"
#include "openpuzzle/runtime/ExecutionSlot.hpp"
#include "openpuzzle/client/HttpRequestPolicy.hpp"
#include <optional>
#include <map>
namespace openpuzzle::ui {
enum class UiAction { Preflight, Start, Stop, SafeStop, Recover, Status, Telemetry, ThermalRead, ThermalSave };
struct WindowsUiSelection {
 std::string profilePath,server,slot="primary";
 int puzzle=71,duration=60;
 bool once=false;
 std::optional<WindowsRunProfile> profile;
 GpuThermalPolicyConfiguration thermal;
};
struct WindowsUiStatus {
 bool known=false,running=false,preserved=false;
 static WindowsUiStatus parse(int code,const std::string& output,const std::string& slot) {
  std::map<std::string,std::string> values;std::istringstream input(output);std::string line;
  while(std::getline(input,line)){
   if(!line.empty()&&line.back()=='\r')line.pop_back();const auto colon=line.find(": ");
   if(colon==std::string::npos)continue;
   if(!values.emplace(line.substr(0,colon),line.substr(colon+2)).second)return {};
  }
  if((code!=0&&code!=2)||values["Slot"]!=slot||!values.count("Assignment")||!values.count("Pending claim"))return {};
  const auto runtime=values["Runtime"];if(runtime!="idle"&&runtime!="running")return {};
  if(values["Assignment"].empty()||(values["Pending claim"]!="none"&&values["Pending claim"]!="preserved"))return {};
  // Code 2 is preserved/uncertain state, even if the runtime itself is known.
  return {true,runtime=="running",code!=0||values["Assignment"]!="none"||values["Pending claim"]!="none"};
 }
 bool canStart()const{return known&&!running&&!preserved;}
};
inline std::string profileSlot(const WindowsRunProfile& p){
 return p.backend=="cpu"?"primary":p.backend=="cuda"?ExecutionSlot::cuda(p.device):ExecutionSlot::opencl(p.device);
}
inline double windowsUiThermalThreshold(std::string value){
 std::replace(value.begin(),value.end(),',','.');double result=0;
 const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
 if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||!std::isfinite(result))throw std::invalid_argument("Temperatura inválida");
 return result;
}
inline std::vector<std::string> windowsUiCommand(UiAction action,const WindowsUiSelection& s) {
 if(action==UiAction::Telemetry)return {"telemetry"};
 if(action==UiAction::ThermalRead)return {"thermal"};
 if(action==UiAction::ThermalSave){
  if(!GpuThermalPolicy::valid(s.thermal))throw std::invalid_argument("Limites térmicos inválidos");
  return {"thermal",s.thermal.enabled?"--enable":"--disable",s.thermal.stopOnCritical?"--stop-on-critical":"--no-stop-on-critical",
   "--warning",std::to_string(s.thermal.warningC),"--critical",std::to_string(s.thermal.criticalC)};
 }
 if(!ExecutionSlot::valid(s.slot))throw std::invalid_argument("Slot inválido");
 if(action==UiAction::Status)return {"status","--slot",s.slot};
 if(action==UiAction::Stop||action==UiAction::SafeStop)return {action==UiAction::Stop?"stop":"safestop","--slot",s.slot};
 if(action==UiAction::Recover){
  std::vector<std::string> args{"recover","--slot",s.slot};
  if(!s.server.empty()){if(!client::HttpRequestPolicy::endpoint(s.server))throw std::invalid_argument("URL inválido");args.insert(args.end(),{"--server",s.server});}return args;
 }
 if(!s.profile||s.profilePath.empty()||s.profilePath.find('\0')!=std::string::npos)throw std::invalid_argument("Seleciona um perfil medido válido");
 if(s.profile->backend!="cpu"&&s.profile->gpuUuid.empty()&&s.profile->pciBusId.empty())throw std::invalid_argument("O perfil GPU requer associação física");
 if(s.slot!=profileSlot(*s.profile))throw std::invalid_argument("O slot não corresponde ao perfil selecionado");
 if(action==UiAction::Preflight)return {"run","--profile",s.profilePath,"--server","http://127.0.0.1:1","--slot",s.slot,"--dry-run"};
 if(action==UiAction::Start&&!client::HttpRequestPolicy::endpoint(s.server))throw std::invalid_argument("Indica o URL do servidor");
 if(s.puzzle<1||s.duration<1||s.duration>360)throw std::invalid_argument("Puzzle ou duração inválidos");
 std::vector<std::string> args{"run"};
 // Local device/profile verification must not require puzzle metadata, contact
 // a server or accidentally turn into a production launch.
 if(action==UiAction::Start)args.push_back(std::to_string(s.puzzle));
 args.insert(args.end(),{"--profile",s.profilePath,"--server",action==UiAction::Preflight?"http://127.0.0.1:1":s.server,
  "--duration-minutes",std::to_string(s.duration),"--slot",s.slot});
 if(s.once)args.push_back("--once");if(action==UiAction::Preflight)args.push_back("--dry-run");
 return args;
}
}
