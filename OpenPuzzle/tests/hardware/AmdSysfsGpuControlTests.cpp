#include "openpuzzle/hardware/GpuControl.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <chrono>
namespace openpuzzle {std::unique_ptr<GpuControlBackend> makeAmdSysfsControl(const std::filesystem::path&);}
using namespace openpuzzle;namespace fs=std::filesystem;
void put(const fs::path& p,const std::string& value){std::ofstream(p)<<value;}
void need(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){const auto root=fs::temp_directory_path()/("openpuzzle-amd-control-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 try{auto pci=root/"0000:09:00.0",h=pci/"hwmon"/"hwmon9";fs::create_directories(h);
 put(pci/"vendor","0x1002");put(h/"name","amdgpu");put(h/"power1_cap","220000000");put(h/"power1_cap_min","100000000");put(h/"power1_cap_max","250000000");put(h/"pwm1","128");put(h/"pwm1_enable","2");put(h/"temp1_input","67000");
 auto b=makeAmdSysfsControl(root);auto d=b->inspect().at(0);need(d.pci=="00000000:09:00.0"&&d.powerNow==220&&d.powerDefault==-1&&d.temperature==67,"AMD units/identity failed");
 need(d.fanMode=="auto","Read-only inspection changed fan mode");GpuControlRequest r{d.vendor,d.pci,"",GpuControlAction::Power,180};
 applyGpuControlChecked(*b,r);need(b->inspect()[0].powerNow==180,"AMD microwatt application failed");
 r.action=GpuControlAction::FanManual;r.value=60;applyGpuControlChecked(*b,r);need(b->inspect()[0].fanMode=="manual","AMD manual fan failed");
 r.action=GpuControlAction::FanAuto;applyGpuControlChecked(*b,r);need(b->inspect()[0].fanMode=="auto","AMD automatic restore failed");
 fs::remove(h/"power1_cap");fs::create_symlink(h/"temp1_input",h/"power1_cap");bool refused=false;r.action=GpuControlAction::Power;r.value=150;
 try{b->apply(d,r);}catch(...){refused=true;}need(refused,"Symlink write accepted");std::ifstream temp(h/"temp1_input");int value=0;temp>>value;need(value==67000,"Unexpected symlink target write");
 fs::remove_all(root);std::cout<<"AmdSysfsGpuControlTests passed: isolated sysfs, units, PWM, default detection and symlink-write refusal\n";return 0;
 }catch(const std::exception& e){fs::remove_all(root);std::cerr<<e.what()<<'\n';return 1;}}
