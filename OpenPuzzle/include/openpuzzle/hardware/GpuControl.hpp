#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace openpuzzle {
class GpuControlFailure:public std::runtime_error {
public:
 int exitCode;
 GpuControlFailure(int code,const std::string& message):std::runtime_error(message),exitCode(code){}
};
// A control is attached to a physical board, never to a CUDA/OpenCL ordinal.
struct GpuControlDevice {
 std::string vendor,pci,uuid,name,detail;
 std::string powerUnit="W",fanMode="unknown";
 bool power=false,fan=false,automatic=false;
 double powerNow=0,powerMin=0,powerMax=0,powerDefault=0,powerStep=1;
 double fanNow=0,fanMin=30,fanMax=100,temperature=-1;
};
enum class GpuControlAction {Power,PowerDefault,FanManual,FanAuto};
struct GpuControlRequest {
 std::string vendor,pci,uuid;GpuControlAction action=GpuControlAction::Power;double value=0;
};
inline bool gpuControlPci(const std::string& s){
 if(s.size()!=16||s[8]!=':'||s[11]!=':'||s[14]!='.'||s[15]<'0'||s[15]>'7')return false;
 for(std::size_t i=0;i<s.size();++i)if(i!=8&&i!=11&&i!=14&&!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
 return true;
}
inline bool gpuControlRange(double value,double low,double high){return std::isfinite(value)&&std::isfinite(low)&&std::isfinite(high)&&low<=high&&value>=low&&value<=high;}
inline void validateGpuControl(const GpuControlDevice& d,const GpuControlRequest& r){
 if(!gpuControlPci(r.pci)||(r.vendor!="nvidia"&&r.vendor!="amd")||d.vendor!=r.vendor||d.pci!=r.pci||(!r.uuid.empty()&&d.uuid!=r.uuid))throw std::invalid_argument("GPU physical identity changed or is invalid");
 if(r.action==GpuControlAction::Power||r.action==GpuControlAction::PowerDefault){
  const double value=r.action==GpuControlAction::PowerDefault?d.powerDefault:r.value;
  if(!d.power)throw std::runtime_error("Power control unavailable for this driver/GPU");
  if(!gpuControlRange(value,d.powerMin,d.powerMax)||!std::isfinite(d.powerStep)||d.powerStep<=0)throw std::invalid_argument("Power limit outside the driver range");
  const auto steps=(value-d.powerMin)/d.powerStep;
  if(std::abs(steps-std::round(steps))>0.00001)throw std::invalid_argument("Power limit does not match the driver step");
 }else if(r.action==GpuControlAction::FanManual){
  if(!d.fan||!d.automatic)throw std::runtime_error("Manual fan control or driver restoration unavailable");
  if(!gpuControlRange(r.value,(std::max)(30.0,d.fanMin),d.fanMax)||std::floor(r.value)!=r.value)throw std::invalid_argument("Fan speed outside the permitted range (minimum 30%)");
  if(!gpuControlRange(d.temperature,0,79.999))throw std::runtime_error("Manual fan control requires a valid GPU temperature below 80 C");
 }else if(!d.automatic)throw std::runtime_error("Automatic fan control unavailable for this driver/GPU");
}
class GpuControlBackend {
public:
 virtual ~GpuControlBackend()=default;
 virtual std::vector<GpuControlDevice> inspect()=0; // Must never call setters.
 virtual void apply(const GpuControlDevice&,const GpuControlRequest&)=0;
};
std::vector<GpuControlDevice> inspectGpuControls();
void applyGpuControl(const GpuControlRequest&);
// Narrow injected boundary permits zero-write/identity/permission tests.
inline GpuControlDevice applyGpuControlChecked(GpuControlBackend& backend,const GpuControlRequest& request){
 auto all=backend.inspect();std::vector<GpuControlDevice> matches;
 for(const auto& d:all)if(d.pci==request.pci)matches.push_back(d);
 if(matches.size()!=1)throw std::runtime_error("GPU physical identity is missing or ambiguous; no change applied");
 validateGpuControl(matches.front(),request);backend.apply(matches.front(),request);
 auto after=backend.inspect();std::optional<GpuControlDevice> result;
 for(const auto& d:after)if(d.pci==request.pci){if(result)throw std::runtime_error("GPU readback ambiguous; inspect the driver settings");result=d;}
 if(!result||result->vendor!=request.vendor||result->uuid!=matches.front().uuid)throw std::runtime_error("GPU readback identity unavailable; inspect the driver settings");
 const auto& d=*result;
 if(request.action==GpuControlAction::Power||request.action==GpuControlAction::PowerDefault){
  const double value=request.action==GpuControlAction::PowerDefault?matches.front().powerDefault:request.value;
  if(!d.power||std::abs(d.powerNow-value)>0.001)throw std::runtime_error("Power readback differs from the request; inspect the driver settings");
 }else if(request.action==GpuControlAction::FanManual){
  if(d.fanMode!="manual"||std::abs(d.fanNow-request.value)>0.5)throw std::runtime_error("Fan target readback differs from the request; inspect the driver settings");
 }else if(d.fanMode!="auto")throw std::runtime_error("Automatic fan readback failed; inspect the driver settings");
 return d;
}
// Versioned, bounded machine protocol used by both desktop panels. Strings are
// quoted; each record is one line, no translated text is used for command routing.
inline std::string formatGpuControls(const std::vector<GpuControlDevice>& devices){
 std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(12)<<"OpenPuzzle-GPU-controls 1\n";
 for(const auto& d:devices)o<<std::quoted(d.vendor)<<' '<<std::quoted(d.pci)<<' '<<std::quoted(d.uuid)<<' '<<std::quoted(d.name)<<' '<<std::quoted(d.detail)<<' '<<std::quoted(d.powerUnit)<<' '<<std::quoted(d.fanMode)<<' '<<d.power<<' '<<d.fan<<' '<<d.automatic<<' '<<d.powerNow<<' '<<d.powerMin<<' '<<d.powerMax<<' '<<d.powerDefault<<' '<<d.powerStep<<' '<<d.fanNow<<' '<<d.fanMin<<' '<<d.fanMax<<' '<<d.temperature<<'\n';
 return o.str();
}
inline std::vector<GpuControlDevice> parseGpuControls(const std::string& text){
 if(text.size()>65536)throw std::runtime_error("GPU control response too large");
 std::istringstream in(text);in.imbue(std::locale::classic());std::string line;
 if(!std::getline(in,line))throw std::runtime_error("GPU control response version unavailable");
 if(!line.empty()&&line.back()=='\r')line.pop_back();
 if(line!="OpenPuzzle-GPU-controls 1")throw std::runtime_error("GPU control response version unavailable");
 std::vector<GpuControlDevice> result;
 while(std::getline(in,line)){
  if(line.empty())continue;if(line.size()>4096||result.size()>=64)throw std::runtime_error("GPU control response exceeds limits");
  std::istringstream s(line);s.imbue(std::locale::classic());GpuControlDevice d;
  if(!(s>>std::quoted(d.vendor)>>std::quoted(d.pci)>>std::quoted(d.uuid)>>std::quoted(d.name)>>std::quoted(d.detail)>>std::quoted(d.powerUnit)>>std::quoted(d.fanMode)>>d.power>>d.fan>>d.automatic>>d.powerNow>>d.powerMin>>d.powerMax>>d.powerDefault>>d.powerStep>>d.fanNow>>d.fanMin>>d.fanMax>>d.temperature))throw std::runtime_error("GPU control response incomplete");
  s>>std::ws;if(!s.eof()||!gpuControlPci(d.pci)||(d.vendor!="amd"&&d.vendor!="nvidia")||(d.powerUnit!="W"&&d.powerUnit!="%"))throw std::runtime_error("GPU control response invalid");
  for(double v:{d.powerNow,d.powerMin,d.powerMax,d.powerDefault,d.powerStep,d.fanNow,d.fanMin,d.fanMax,d.temperature})if(!std::isfinite(v))throw std::runtime_error("GPU control response contains invalid numbers");
  for(const auto& old:result)if(old.pci==d.pci)throw std::runtime_error("GPU control identity ambiguous");result.push_back(d);
 }return result;
}
inline double gpuControlNumber(const std::string& text){
 std::istringstream in(text);in.imbue(std::locale::classic());double n=0;
 if(!(in>>n)||!std::isfinite(n))throw std::invalid_argument("Invalid GPU control number");in>>std::ws;if(!in.eof())throw std::invalid_argument("Invalid GPU control number");return n;
}
inline int gpuControlCommand(const std::vector<std::string>& args,std::ostream& out,
 const std::function<std::vector<GpuControlDevice>()>& inspect=inspectGpuControls,
 const std::function<void(const GpuControlRequest&)>& apply=applyGpuControl){
 if(args.empty()||(args.size()==1&&args[0]=="list")){out<<formatGpuControls(inspect());return 0;}
 if(args.front()!="apply"||args.size()<6)throw std::invalid_argument("gpu-control list | apply --vendor nvidia|amd --pci DDDDDDDD:BB:DD.F [--uuid UUID] --power VALUE | --power-default | --fan auto|30..100");
 GpuControlRequest r;bool operation=false;std::vector<std::string> seen;
 for(std::size_t i=1;i<args.size();++i){const auto& option=args[i];if(std::find(seen.begin(),seen.end(),option)!=seen.end())throw std::invalid_argument("Duplicate GPU control option");seen.push_back(option);
  if(option=="--power-default"){if(operation)throw std::invalid_argument("Apply one GPU control at a time");operation=true;r.action=GpuControlAction::PowerDefault;continue;}
  if(++i==args.size())throw std::invalid_argument("Missing GPU control option value");const auto& value=args[i];
  if(option=="--vendor")r.vendor=value;else if(option=="--pci")r.pci=value;else if(option=="--uuid")r.uuid=value;
  else if(option=="--power"||option=="--fan"){
   if(operation)throw std::invalid_argument("Apply one GPU control at a time");operation=true;
   if(option=="--power"){r.action=GpuControlAction::Power;r.value=gpuControlNumber(value);}
   else if(value=="auto")r.action=GpuControlAction::FanAuto;else {r.action=GpuControlAction::FanManual;r.value=gpuControlNumber(value);}
  }else throw std::invalid_argument("Unknown GPU control option");
 }
 if(!operation||!gpuControlPci(r.pci)||(r.vendor!="nvidia"&&r.vendor!="amd"))throw std::invalid_argument("Explicit vendor, physical PCI and one operation required");
 apply(r);out<<formatGpuControls(inspect());return 0;
}
} // namespace openpuzzle
