#pragma once
#include "openpuzzle/runtime/RunCommandOptions.hpp"
#include "openpuzzle/core/CliInteger.hpp"
#include "openpuzzle/hardware/WindowsTelemetryModel.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <charconv>
#include <cmath>
#include <set>
#include <sstream>
namespace openpuzzle {
struct WindowsRunProfile {
 std::string backend,sha256,deviceIdentity;
 int device=0,threads=0,blocks=0,points=0;
 double speed=0;
 bool autoTuned=false;
 std::string pciBusId,gpuUuid;
 static WindowsRunProfile parse(const std::string& json) {
  if(json.empty()||json.size()>16384)throw std::invalid_argument("Invalid benchmark profile size");
  boost::property_tree::ptree p;std::istringstream input(json);boost::property_tree::read_json(input,p);
  std::set<std::string> fields;
  for(const auto& item:p)if(item.first.empty()||!fields.insert(item.first).second||!item.second.empty())throw std::invalid_argument("Duplicate or structured benchmark field");
  auto integer=[&](const char* key,int lo,int hi){const auto n=cliIntegerArgument({"--value",p.get<std::string>(key)},{"--value"},0);if(n<lo||n>hi)throw std::invalid_argument("Invalid benchmark integer");return n;};
  (void)integer("version",1,1);(void)integer("samples",3,10000);const auto duration=integer("duration_seconds",10,60);
  if(p.get<std::string>("selftest_passed")!="true")throw std::invalid_argument("Profile lacks a passed native self-test");
  WindowsRunProfile result;result.backend=p.get<std::string>("backend");
  if(result.backend!="cpu"&&result.backend!="cuda"&&result.backend!="opencl")throw std::invalid_argument("Unknown profile backend");
  result.sha256=p.get<std::string>("engine_sha256");
  if(result.sha256.size()!=64||result.sha256.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::invalid_argument("Invalid engine fingerprint");
  result.deviceIdentity=p.get<std::string>("device_identity");
  if(result.deviceIdentity.empty()||result.deviceIdentity.size()>1024||result.deviceIdentity.find('\0')!=std::string::npos)throw std::invalid_argument("Missing verified device identity");
  if(const auto pci=p.get_optional<std::string>("pci_bus_id")){
   result.pciBusId=canonicalGpuPciBusId(*pci);
   if(result.pciBusId.empty()||result.backend=="cpu")throw std::invalid_argument("Invalid GPU profile PCI identity");
  }
  if(const auto uuid=p.get_optional<std::string>("gpu_uuid")){
   result.gpuUuid=canonicalGpuUuid(*uuid);
   if(result.gpuUuid.empty()||result.backend=="cpu")throw std::invalid_argument("Invalid GPU profile UUID");
  }
  if(p.get_optional<std::string>("autotuned")){
   if(p.get<std::string>("autotuned")!="true"||p.get<std::string>("finalist_validated","")!="true"||result.backend=="cpu"||duration!=60)throw std::invalid_argument("Invalid automatic benchmark validation");
   (void)integer("candidates_tested",1,64);result.autoTuned=true;
  }
  result.device=integer("device",0,255);result.threads=integer("threads",1,result.backend=="cpu"?256:1024);
  result.blocks=integer("blocks",1,65535);result.points=integer("points",1,65536);
  if(result.backend=="cpu"?result.device!=0:result.threads<32||result.threads%32!=0)throw std::invalid_argument("Invalid profile device/threads");
  const auto text=p.get<std::string>("speed_mkeys");const auto converted=std::from_chars(text.data(),text.data()+text.size(),result.speed);
  if(converted.ec!=std::errc{}||converted.ptr!=text.data()+text.size()||!std::isfinite(result.speed)||result.speed<=0||result.speed>1e9)throw std::invalid_argument("Invalid benchmark planning speed");
  return result;
 }
 RunCommandOptions apply(RunCommandOptions o)const {
  if(o.command!="run"||(!o.backend.empty()&&o.backend!=backend)||(!o.engine.empty()&&o.engine!=(backend=="cpu"?"keyhunt":"bitcrack"))||
     (o.deviceSpecified&&o.device!=device)||(o.threadsSpecified&&o.threads!=threads)||
     (o.blocksSpecified&&o.blocks!=blocks)||(o.pointsSpecified&&o.points!=points))throw std::invalid_argument("Run parameters differ from the measured engine profile; benchmark them first");
  o.engine=backend=="cpu"?"keyhunt":"bitcrack";o.backend=backend;o.device=device;o.threads=threads;o.blocks=blocks;o.points=points;
  o.threadsSpecified=true;return o;
 }
};
}
