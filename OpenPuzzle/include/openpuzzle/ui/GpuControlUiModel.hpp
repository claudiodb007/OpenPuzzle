#pragma once
#include "openpuzzle/hardware/GpuControl.hpp"
namespace openpuzzle::ui {
inline std::vector<std::string> gpuControlArguments(const GpuControlDevice& d,GpuControlAction action,double value=0){
 GpuControlRequest r{d.vendor,d.pci,d.uuid,action,value};validateGpuControl(d,r);
 std::vector<std::string> args{"gpu-control","apply","--vendor",d.vendor,"--pci",d.pci};
 if(!d.uuid.empty()){args.push_back("--uuid");args.push_back(d.uuid);}
 if(action==GpuControlAction::PowerDefault)args.push_back("--power-default");
 else {args.push_back(action==GpuControlAction::Power?"--power":"--fan");std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(12)<<value;args.push_back(action==GpuControlAction::FanAuto?"auto":s.str());}
 return args;
}
}
