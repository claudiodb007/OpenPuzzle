#include "GpuControlInternal.hpp"
#include "openpuzzle/hardware/NvmlControlApi.hpp"
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace openpuzzle {
namespace {
// Official NVML v3 PCI ABI; keep separate from the existing read-only telemetry.
using Pci=NvmlControlPci;
static_assert(sizeof(Pci)==68);
class Nvml final:public GpuControlBackend {
 void* library_=nullptr;bool initialized_=false;
 template<class T>T symbol(const char* name){
#ifdef _WIN32
  return library_?reinterpret_cast<T>(GetProcAddress(static_cast<HMODULE>(library_),name)):nullptr;
#else
  return library_?reinterpret_cast<T>(dlsym(library_,name)):nullptr;
#endif
 }
 using Init=int(*)();using One=int(*)(void*,unsigned*);using Fans=int(*)(void*,unsigned,unsigned*);
 Init shutdown_=nullptr;int(*count_)(unsigned*)=nullptr;int(*handle_)(unsigned,void**)=nullptr;
 int(*name_)(void*,char*,unsigned)=nullptr;int(*uuid_)(void*,char*,unsigned)=nullptr;int(*pci_)(void*,Pci*)=nullptr;
 One limit_=nullptr,defaults_=nullptr,numFans_=nullptr;
 int(*constraints_)(void*,unsigned*,unsigned*)=nullptr;int(*fanRange_)(void*,unsigned*,unsigned*)=nullptr;
 Fans policy_=nullptr,target_=nullptr,temp_=nullptr;
 int(*setPower_)(void*,unsigned)=nullptr;int(*setFan_)(void*,unsigned,unsigned)=nullptr;int(*autoFan_)(void*,unsigned)=nullptr;
 struct Board {GpuControlDevice d;void* handle=nullptr;unsigned fans=0;};
 std::vector<Board> boards(){
  std::vector<Board> result;unsigned count=0;if(!initialized_||count_(&count)||count>64)return result;
  for(unsigned index=0;index<count;++index){
   Board b;Pci pci{};char name[128]{},uuid[128]{};if(handle_(index,&b.handle)||!b.handle||pci_(b.handle,&pci)||name_(b.handle,name,sizeof name)||uuid_(b.handle,uuid,sizeof uuid)||!std::memchr(name,0,sizeof name)||!std::memchr(uuid,0,sizeof uuid))continue;
   unsigned domain=0,bus=0,device=0,function=0;int end=0;if(!std::memchr(pci.busId,0,sizeof pci.busId)||std::sscanf(pci.busId,"%x:%x:%x.%x%n",&domain,&bus,&device,&function,&end)!=4||pci.busId[end])continue;
   auto& d=b.d;d.vendor="nvidia";d.pci=controlPci(domain,bus,device,function);d.uuid=uuid;d.name=name;d.detail="NVML; permission and write support checked on Apply";
   if(!gpuControlPci(d.pci)||d.uuid.empty())continue;
   unsigned now=0,low=0,high=0,original=0;
   if(limit_&&defaults_&&constraints_&&setPower_&&!limit_(b.handle,&now)&&!defaults_(b.handle,&original)&&!constraints_(b.handle,&low,&high)&&low>0&&low<=original&&original<=high&&now>0){
    d.power=true;d.powerNow=now/1000.0;d.powerMin=low/1000.0;d.powerMax=(std::min)(high,original)/1000.0;d.powerDefault=original/1000.0;d.powerStep=0.001;
   }
   unsigned temperature=0;if(temp_&&!temp_(b.handle,0,&temperature)&&temperature<=120)d.temperature=temperature;
   unsigned lo=0,hi=0;
   if(numFans_&&policy_&&target_&&fanRange_&&setFan_&&autoFan_&&!numFans_(b.handle,&b.fans)&&b.fans>0&&b.fans<=16&&!fanRange_(b.handle,&lo,&hi)&&lo<=hi&&hi<=100){
    bool valid=true;unsigned firstPolicy=0,firstSpeed=0;
    for(unsigned fan=0;fan<b.fans;++fan){unsigned p=0,t=0;if(policy_(b.handle,fan,&p)||target_(b.handle,fan,&t)||p>1||t>100){valid=false;break;}if(fan==0){firstPolicy=p;firstSpeed=t;}else if(p!=firstPolicy||(p==1&&t!=firstSpeed)){d.fanMode="mixed";}}
    if(valid){d.fan=d.automatic=true;d.fanMin=(std::max)(30u,lo);d.fanMax=hi;d.fanNow=firstSpeed;if(d.fanMode!="mixed")d.fanMode=firstPolicy?"manual":"auto";}
   }
   result.push_back(b);
  }
  unsigned after=0;if(count_(&after)||after!=count)throw std::runtime_error("NVML inventory changed during inspection");return result;
 }
 static void check(int code){if(!code)return;throw GpuControlFailure(code==4?77:code==3?69:1,code==4?"NVIDIA driver denied permission; run this explicit control with administrator/root permission":code==3?"NVIDIA driver does not support this control":"NVIDIA GPU control failed (NVML "+std::to_string(code)+")");}
public:
 explicit Nvml(const NvmlControlApi& a):initialized_(a.count&&a.handle&&a.name&&a.uuid&&a.pci),
 count_(a.count),handle_(a.handle),name_(a.name),uuid_(a.uuid),pci_(a.pci),limit_(a.limit),defaults_(a.defaults),numFans_(a.numFans),
 constraints_(a.constraints),fanRange_(a.fanRange),policy_(a.policy),target_(a.target),temp_(a.temperature),setPower_(a.setPower),setFan_(a.setFan),autoFan_(a.autoFan){}
 Nvml(){
#ifdef _WIN32
  wchar_t path[32768]{};auto n=GetSystemDirectoryW(path,32768);if(!n||n>=32750)return;std::wstring dll(path,n);dll+=L"\\nvml.dll";library_=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
  library_=dlopen("libnvidia-ml.so.1",RTLD_NOW|RTLD_LOCAL);
#endif
  auto init=symbol<Init>("nvmlInit_v2");shutdown_=symbol<Init>("nvmlShutdown");count_=symbol<decltype(count_)>("nvmlDeviceGetCount_v2");handle_=symbol<decltype(handle_)>("nvmlDeviceGetHandleByIndex_v2");
  name_=symbol<decltype(name_)>("nvmlDeviceGetName");uuid_=symbol<decltype(uuid_)>("nvmlDeviceGetUUID");pci_=symbol<decltype(pci_)>("nvmlDeviceGetPciInfo_v3");
  limit_=symbol<One>("nvmlDeviceGetPowerManagementLimit");defaults_=symbol<One>("nvmlDeviceGetPowerManagementDefaultLimit");constraints_=symbol<decltype(constraints_)>("nvmlDeviceGetPowerManagementLimitConstraints");setPower_=symbol<decltype(setPower_)>("nvmlDeviceSetPowerManagementLimit");
  numFans_=symbol<One>("nvmlDeviceGetNumFans");fanRange_=symbol<decltype(fanRange_)>("nvmlDeviceGetMinMaxFanSpeed");policy_=symbol<Fans>("nvmlDeviceGetFanControlPolicy_v2");target_=symbol<Fans>("nvmlDeviceGetTargetFanSpeed");temp_=symbol<Fans>("nvmlDeviceGetTemperature");setFan_=symbol<decltype(setFan_)>("nvmlDeviceSetFanSpeed_v2");autoFan_=symbol<decltype(autoFan_)>("nvmlDeviceSetDefaultFanSpeed_v2");
  initialized_=init&&shutdown_&&count_&&handle_&&name_&&uuid_&&pci_&&!init();
 }
 ~Nvml(){if(initialized_&&shutdown_)shutdown_();if(library_){
#ifdef _WIN32
  FreeLibrary(static_cast<HMODULE>(library_));
#else
  dlclose(library_);
#endif
 }}
 std::vector<GpuControlDevice> inspect()override{std::vector<GpuControlDevice> result;for(auto& b:boards())result.push_back(b.d);return result;}
 void apply(const GpuControlDevice& selected,const GpuControlRequest& request)override{
  auto all=boards();Board* b=nullptr;for(auto& item:all)if(item.d.pci==selected.pci){if(b)throw std::runtime_error("NVML GPU identity ambiguous");b=&item;}
  if(!b)throw std::runtime_error("NVML GPU disappeared");validateGpuControl(b->d,request);
  if(request.action==GpuControlAction::Power||request.action==GpuControlAction::PowerDefault){
   const auto value=static_cast<unsigned>(std::llround((request.action==GpuControlAction::PowerDefault?b->d.powerDefault:request.value)*1000));check(setPower_(b->handle,value));
   unsigned actual=0;if(limit_(b->handle,&actual)||actual!=value){const auto old=static_cast<unsigned>(std::llround(b->d.powerNow*1000));unsigned read=0;const bool restored=!setPower_(b->handle,old)&&!limit_(b->handle,&read)&&read==old;throw GpuControlFailure(restored?1:70,restored?"Power readback failed; previous limit restored":"Power readback failed; rollback failed; inspect NVIDIA settings");}return;
  }
  std::vector<std::pair<unsigned,unsigned>> previous;
  for(unsigned i=0;i<b->fans;++i){unsigned p=0,t=0;check(policy_(b->handle,i,&p));check(target_(b->handle,i,&t));if(p>1||t>100)throw std::runtime_error("Invalid NVIDIA fan state");previous.emplace_back(p,t);}
  int failure=0;
  for(unsigned i=0;i<b->fans;++i){failure=request.action==GpuControlAction::FanAuto?autoFan_(b->handle,i):setFan_(b->handle,i,static_cast<unsigned>(request.value));if(failure)break;}
  if(!failure)for(unsigned i=0;i<b->fans;++i){unsigned p=0,t=0;if(policy_(b->handle,i,&p)||target_(b->handle,i,&t)||(request.action==GpuControlAction::FanAuto?p!=0:p!=1||t!=request.value)){failure=999;break;}}
  if(failure){bool restored=true;for(unsigned i=0;i<b->fans;++i)if(previous[i].first?setFan_(b->handle,i,previous[i].second):autoFan_(b->handle,i))restored=false;
   for(unsigned i=0;i<b->fans;++i){unsigned p=0,t=0;if(policy_(b->handle,i,&p)||target_(b->handle,i,&t)||p!=previous[i].first||(p==1&&t!=previous[i].second))restored=false;}
   if(!restored)throw GpuControlFailure(70,"NVIDIA fan operation and rollback failed; inspect driver settings immediately");check(failure);}
 }
};
}
std::unique_ptr<GpuControlBackend> makeNvmlControl(){return std::make_unique<Nvml>();}
std::unique_ptr<GpuControlBackend> makeNvmlControlFixture(const NvmlControlApi& api){return std::make_unique<Nvml>(api);}
}
