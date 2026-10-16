#include "GpuControlInternal.hpp"
#ifndef _WIN32
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
namespace openpuzzle {
namespace {
namespace fs=std::filesystem;
std::optional<long long> number(const fs::path& path){
 std::ifstream in(path);std::string value,extra;if(!(in>>value)||in>>extra||value.size()>24)return {};try{std::size_t end=0;auto n=std::stoll(value,&end,0);if(end==value.size()&&n>=0)return n;}catch(...){}return {};
}
void writeNumber(const fs::path& path,long long value){
 const int fd=open(path.c_str(),O_WRONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)throw std::runtime_error(errno==EACCES||errno==EPERM?"AMD sysfs permission denied; root permission required":"AMD sysfs control could not be opened");
 const auto text=std::to_string(value)+"\n";const auto n=write(fd,text.data(),text.size());const auto err=errno;const auto closed=close(fd);
 if(n!=static_cast<ssize_t>(text.size())||closed)throw std::runtime_error("AMD sysfs write failed: "+std::string(std::strerror(err)));
}
class AmdSysfs final:public GpuControlBackend {
 fs::path root_;
 struct Board {GpuControlDevice d;fs::path hwmon;};
 std::vector<Board> boards(){
  std::vector<Board> result;std::error_code ec;
  fs::directory_iterator it(root_,ec),end;if(ec)return result;
  for(;it!=end;it.increment(ec)){
   if(ec)throw std::runtime_error("PCI inventory changed");auto vendor=number(it->path()/"vendor");if(!vendor||*vendor!=0x1002)continue;
   // Only a driver explicitly identifying its hwmon as amdgpu is writable.
   const auto hwroot=it->path()/"hwmon";std::vector<fs::path> monitors;
   for(fs::directory_iterator h(hwroot,ec);!ec&&h!=end;h.increment(ec)){std::ifstream file(h->path()/"name");std::string name;if(file>>name&&name=="amdgpu")monitors.push_back(h->path());}ec.clear();
   if(monitors.size()!=1)continue;const auto h=monitors.front();
   unsigned domain=0,bus=0,device=0,function=0;int length=0;const auto id=it->path().filename().string();
   if(std::sscanf(id.c_str(),"%x:%x:%x.%x%n",&domain,&bus,&device,&function,&length)!=4||length!=static_cast<int>(id.size()))continue;
   Board b;auto& d=b.d;d.vendor="amd";d.pci=controlPci(domain,bus,device,function);if(!gpuControlPci(d.pci))continue;
   d.name="AMD amdgpu "+d.pci;d.detail="amdgpu hwmon; root permission required on Apply";b.hwmon=h;
   auto now=number(h/"power1_cap"),low=number(h/"power1_cap_min"),high=number(h/"power1_cap_max"),original=number(h/"power1_cap_default");
   if(now&&low&&high&&*low>0&&*low<=*high&&*now>0){d.power=true;d.powerNow=*now/1e6;d.powerMin=*low/1e6;d.powerMax=*high/1e6;d.powerDefault=original?*original/1e6:-1;d.powerStep=0.000001;}
   auto pwm=number(h/"pwm1"),mode=number(h/"pwm1_enable"),minimum=number(h/"pwm1_min"),maximum=number(h/"pwm1_max");
   if(pwm&&mode&&*pwm<=255&&*mode<=2){d.fan=d.automatic=true;d.fanNow=*pwm*100.0/255;d.fanMin=(std::max)(30.0,minimum?std::ceil(*minimum*100.0/255):30.0);d.fanMax=maximum?std::floor(*maximum*100.0/255):100;d.fanMode=*mode==2?"auto":*mode==1?"manual":"unknown";}
   const auto temp=number(h/"temp1_input");if(temp&&*temp<=120000)d.temperature=*temp/1000.0;
   result.push_back(b);
  }return result;
 }
public:
 explicit AmdSysfs(fs::path root):root_(std::move(root)){}
 std::vector<GpuControlDevice> inspect()override{std::vector<GpuControlDevice> out;for(auto& b:boards())out.push_back(b.d);return out;}
 void apply(const GpuControlDevice& device,const GpuControlRequest& request)override{
  auto all=boards();Board* board=nullptr;for(auto& b:all)if(b.d.pci==device.pci){if(board)throw std::runtime_error("AMD physical identity ambiguous");board=&b;}
  if(!board)throw std::runtime_error("AMD physical GPU unavailable");validateGpuControl(board->d,request);const auto h=board->hwmon;
  if(request.action==GpuControlAction::Power||request.action==GpuControlAction::PowerDefault){
   const auto previous=number(h/"power1_cap");if(!previous)throw std::runtime_error("AMD power snapshot unavailable");
   const auto requested=std::llround((request.action==GpuControlAction::PowerDefault?board->d.powerDefault:request.value)*1e6);
   try{writeNumber(h/"power1_cap",requested);if(number(h/"power1_cap")!=requested)throw std::runtime_error("AMD power readback failed");}
   catch(...){try{writeNumber(h/"power1_cap",*previous);if(number(h/"power1_cap")!=previous)throw std::runtime_error("AMD power rollback readback failed");}catch(...){throw GpuControlFailure(70,"AMD power change and rollback failed; inspect driver settings");}throw;}return;
  }
  const auto previous=number(h/"pwm1"),mode=number(h/"pwm1_enable");if(!previous||!mode)throw std::runtime_error("AMD fan snapshot unavailable");
  try{
   if(request.action==GpuControlAction::FanAuto){writeNumber(h/"pwm1_enable",2);if(number(h/"pwm1_enable")!=2)throw std::runtime_error("AMD automatic fan readback failed");}
   else {
    // Keep the existing duty while entering manual, then set the target.
    const auto target=std::llround(request.value*255/100);writeNumber(h/"pwm1_enable",1);writeNumber(h/"pwm1",target);
    if(number(h/"pwm1")!=target||number(h/"pwm1_enable")!=1)throw std::runtime_error("AMD manual fan readback failed");
   }
  }catch(...){try{writeNumber(h/"pwm1",*previous);writeNumber(h/"pwm1_enable",*mode);if(number(h/"pwm1_enable")!=mode||(*mode==1&&number(h/"pwm1")!=previous))throw std::runtime_error("AMD fan rollback readback failed");}catch(...){throw GpuControlFailure(70,"AMD fan change and rollback failed; inspect driver settings immediately");}throw;}
 }
};
}
// Root injection is only a C++ fixture seam; there is no CLI/environment override.
std::unique_ptr<GpuControlBackend> makeAmdSysfsControl(const std::filesystem::path& root){return std::make_unique<AmdSysfs>(root);}
std::unique_ptr<GpuControlBackend> makeAmdControl(){return makeAmdSysfsControl("/sys/bus/pci/devices");}
}
#endif
