#include "GpuControlInternal.hpp"
#include "openpuzzle/hardware/WindowsGpuTelemetry.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#include "openpuzzle/hardware/WindowsAdapterIdentity.hpp"
#ifdef OPENPUZZLE_WITH_ADLX
#include <ADLX.h>
#include <IGPUTuning.h>
#include <IGPUManualPowerTuning.h>
#include <IGPUManualFanTuning.h>
#include <IPerformanceMonitoring.h>
#endif
namespace openpuzzle {
namespace {
#ifdef OPENPUZZLE_WITH_ADLX
using namespace adlx;
template<class T>struct Ref {
 T* p=nullptr;~Ref(){if(p)p->Release();}T* operator->()const{return p;}operator bool()const{return p!=nullptr;}
 Ref()=default;Ref(const Ref&)=delete;Ref& operator=(const Ref&)=delete;
};
bool ok(ADLX_RESULT r){return ADLX_SUCCEEDED(r);}
// Resolve the driver's persistent PNP identity through Windows. Older ADLX
// runtimes expose ADLXInitialize, but not the newer ADLXInitialize2 mapping API.
std::optional<std::string> physicalPci(const std::string& instance){
 const auto id=canonicalWindowsGpuInstanceId(instance);if(id.empty())return {};
 struct Devices{HDEVINFO h=SetupDiCreateDeviceInfoList(nullptr,nullptr);~Devices(){if(h!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(h);}} devices;
 if(devices.h==INVALID_HANDLE_VALUE)return {};
 SP_DEVINFO_DATA data{};data.cbSize=sizeof data;const std::wstring wide(id.begin(),id.end());
 if(!SetupDiOpenDeviceInfoW(devices.h,wide.c_str(),nullptr,0,&data))return {};
 const auto number=[&](DWORD property)->std::optional<DWORD>{DWORD type=0,size=0,value=0;
  if(!SetupDiGetDeviceRegistryPropertyW(devices.h,&data,property,&type,reinterpret_cast<BYTE*>(&value),sizeof value,&size)||type!=REG_DWORD||size!=sizeof value)return {};return value;};
 const auto bus=number(SPDRP_BUSNUMBER),address=number(SPDRP_ADDRESS);
 if(!bus||!address||*bus>255||(*address>>16)>31||(*address&0xffff)>7)return {};
 // The control key is BDF-scoped; the full PNP instance remains mandatory in
 // the descriptor/request so it is never confused with a different board.
 return controlPci(0,*bus,*address>>16,*address&0xffff);
}
void check(ADLX_RESULT r){if(ok(r))return;throw GpuControlFailure(r==ADLX_RESET_NEEDED?78:r==ADLX_NOT_SUPPORTED?69:1,r==ADLX_RESET_NEEDED?"AMD driver requires a tuning reset; OpenPuzzle will not reset other GPU settings":"AMD ADLX operation failed ("+std::to_string(r)+"); check driver support and administrator permission");}
class AmdWindows final:public GpuControlBackend {
 HMODULE dll_=nullptr;ADLXTerminate_Fn terminate_=nullptr;IADLXSystem* system_=nullptr;
 Ref<IADLXGPUTuningServices> tuning_;Ref<IADLXPerformanceMonitoringServices> monitoring_;
 struct Board {GpuControlDevice d;unsigned index=0;};
 void power(IADLXGPU* gpu,Ref<IADLXManualPowerTuning>& result){
  Ref<IADLXInterface> base;if(!tuning_||!ok(tuning_->GetManualPowerTuning(gpu,&base.p))||!base)return;
  base->QueryInterface(IADLXManualPowerTuning::IID(),reinterpret_cast<void**>(&result.p));
 }
 void fan(IADLXGPU* gpu,Ref<IADLXManualFanTuning1>& result){
  Ref<IADLXInterface> base;if(!tuning_||!ok(tuning_->GetManualFanTuning(gpu,&base.p))||!base)return;
  base->QueryInterface(IADLXManualFanTuning1::IID(),reinterpret_cast<void**>(&result.p));
 }
 std::vector<std::pair<int,int>> fanValues(IADLXManualFanTuningStateList* states){
  std::vector<std::pair<int,int>> result;if(!states||states->Size()==0||states->Size()>32)throw std::runtime_error("AMD fan curve unavailable");
  for(unsigned i=0;i<states->Size();++i){Ref<IADLXManualFanTuningState> state;check(states->At(i,&state.p));int temp=0,speed=0;check(state->GetTemperature(&temp));check(state->GetFanSpeed(&speed));result.emplace_back(temp,speed);}return result;
 }
 std::vector<Board> boards(){
  std::vector<Board> result;if(!system_||!tuning_)return result;Ref<IADLXGPUList> list;if(!ok(system_->GetGPUs(&list.p))||!list||list->Size()>64)return result;
  for(unsigned i=0;i<list->Size();++i){
   Ref<IADLXGPU> gpu;if(!ok(list->At(i,&gpu.p))||!gpu)continue;
   const char* pnp=nullptr;if(!ok(gpu->PNPString(&pnp))||!pnp)continue;
   const auto identity=canonicalWindowsGpuInstanceId(pnp),pci=physicalPci(identity).value_or("");
   if(identity.empty()||!gpuControlPci(pci))continue;
   Board b;b.index=i;auto& d=b.d;d.vendor="amd";d.pci=pci;d.uuid=identity;
   const char* name=nullptr;if(ok(gpu->Name(&name))&&name)d.name=name;d.powerUnit="%";d.detail="ADLX; power is percent offset, not watts; driver defaults restore fans only";
   Ref<IADLXManualPowerTuning> p;power(gpu.p,p);ADLX_IntRange range{};int current=0,original=0;
   if(p&&ok(p->GetPowerLimitRange(&range))&&ok(p->GetPowerLimit(&current))&&range.step>0&&range.minValue<=range.maxValue&&current>=range.minValue&&current<=range.maxValue){
    d.power=true;d.powerNow=current;d.powerMin=range.minValue;d.powerMax=range.maxValue;d.powerStep=range.step;
    // Legacy ADLX can expose manual power without the newer factory-default
    // getter. Keep manual power, and explicitly disable Default in that case.
    d.powerDefault=static_cast<double>(range.maxValue)+range.step;
    Ref<IADLXManualPowerTuning1> defaults;p->QueryInterface(IADLXManualPowerTuning1::IID(),reinterpret_cast<void**>(&defaults.p));
    if(defaults&&ok(defaults->GetPowerLimitDefault(&original))&&original>=range.minValue&&original<=range.maxValue)d.powerDefault=original;
   }
   Ref<IADLXGPUMetrics> metrics;double temperature=-1;
   if(monitoring_&&ok(monitoring_->GetCurrentGPUMetrics(gpu.p,&metrics.p))&&metrics&&ok(metrics->GPUTemperature(&temperature))&&gpuControlRange(temperature,0,120))d.temperature=temperature;
   Ref<IADLXManualFanTuning1> f;fan(gpu.p,f);
   if(f){
    ADLX_IntRange speeds{},temps{};Ref<IADLXManualFanTuningStateList> states,defaults;
    if(ok(f->GetFanTuningRanges(&speeds,&temps))&&speeds.minValue>=0&&speeds.maxValue<=100&&speeds.step==1&&ok(f->GetFanTuningStates(&states.p))&&ok(f->GetDefaultFanTuningStates(&defaults.p))){
     try{auto values=fanValues(states.p),originals=fanValues(defaults.p);d.fan=d.automatic=true;d.fanMin=(std::max)(30,speeds.minValue);d.fanMax=speeds.maxValue;d.fanNow=values.front().second;
      bool isDefault=values==originals,zeroSupported=false,zero=false,defaultZero=false;
      if(!ok(f->IsSupportedZeroRPM(&zeroSupported)))d.fan=d.automatic=false;
      if(zeroSupported){if(!ok(f->GetZeroRPMState(&zero))||!ok(f->GetDefaultZeroRPMState(&defaultZero)))d.fan=d.automatic=false;else isDefault=isDefault&&zero==defaultZero;}
      const bool flat=std::all_of(values.begin(),values.end(),[&](auto v){return v.second==values.front().second;});d.fanMode=isDefault?"auto":flat&&!zero?"manual":"mixed";
     }catch(...){d.fan=d.automatic=false;}
    }
   }result.push_back(b);
  }return result;
 }
public:
 explicit AmdWindows(bool sensorsOnly=false){
  wchar_t path[32768]{};auto n=GetSystemDirectoryW(path,32768);if(!n||n>=32740)return;const auto full=std::wstring(path,n)+L"\\amdadlx64.dll";
  dll_=LoadLibraryExW(full.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!dll_)return;
  auto init=reinterpret_cast<ADLXInitialize_Fn>(GetProcAddress(dll_,"ADLXInitialize"));terminate_=reinterpret_cast<ADLXTerminate_Fn>(GetProcAddress(dll_,"ADLXTerminate"));
  if(!init||!terminate_)return;
  adlx_uint64 requested=ADLX_MAKE_FULL_VER(1,0,0,0),runtime=0;
  const auto query=reinterpret_cast<ADLXQueryFullVersion_Fn>(GetProcAddress(dll_,"ADLXQueryFullVersion"));
  if(query&&ok(query(&runtime))&&runtime>=requested)requested=(std::min)(runtime,static_cast<adlx_uint64>(ADLX_FULL_VERSION));
  if(!ok(init(requested,&system_))){system_=nullptr;return;}
  if(!sensorsOnly)system_->GetGPUTuningServices(&tuning_.p);
  system_->GetPerformanceMonitoringServices(&monitoring_.p);
 }
 ~AmdWindows(){if(monitoring_.p){monitoring_.p->Release();monitoring_.p=nullptr;}if(tuning_.p){tuning_.p->Release();tuning_.p=nullptr;}if(system_&&terminate_)terminate_();if(dll_)FreeLibrary(dll_);}
 WindowsTelemetryReport telemetry(){
  if(!system_||!monitoring_)return {false,"AMD ADLX performance monitoring unavailable",{}};
  WindowsAmdTelemetryApi api;
  api.devices=[this]{
   Ref<IADLXGPUList> list;check(system_->GetGPUs(&list.p));
   if(!list||list->Size()>256)throw std::runtime_error("AMD GPU inventory unavailable");
   std::vector<WindowsAmdTelemetryDevice> result;
   for(unsigned i=0;i<list->Size();++i){Ref<IADLXGPU> gpu;check(list->At(i,&gpu.p));
    if(!gpu)throw std::runtime_error("AMD GPU disappeared");
    const char *pnp=nullptr,*name=nullptr;check(gpu->PNPString(&pnp));check(gpu->Name(&name));
    if(!pnp||!name)throw std::runtime_error("AMD GPU identity unavailable");
    result.push_back({static_cast<int>(i),name,pnp});
   }return result;
  };
  const auto sample=[this](const std::string& identity,bool power)->std::optional<double>{
   Ref<IADLXGPUList> list;check(system_->GetGPUs(&list.p));
   if(!list||list->Size()>256)throw std::runtime_error("AMD GPU inventory unavailable");
   Ref<IADLXGPU> selected;
   for(unsigned i=0;i<list->Size();++i){Ref<IADLXGPU> gpu;check(list->At(i,&gpu.p));
    if(!gpu)throw std::runtime_error("AMD GPU disappeared");
    const char* pnp=nullptr;check(gpu->PNPString(&pnp));
    if(pnp&&canonicalWindowsGpuInstanceId(pnp)==identity){
     if(selected)throw std::runtime_error("AMD sensor identity duplicated");
     selected.p=gpu.p;gpu.p=nullptr;
    }
   }
   if(!selected)throw std::runtime_error("AMD sensor disappeared");
   Ref<IADLXGPUMetricsSupport> support;
   if(!ok(monitoring_->GetSupportedGPUMetrics(selected.p,&support.p))||!support)return {};
   Ref<IADLXGPUMetrics> metrics;
   if(!ok(monitoring_->GetCurrentGPUMetrics(selected.p,&metrics.p))||!metrics)return {};
   bool supported=false;double value=0;
   if(!power){
    if(ok(support->IsSupportedGPUTemperature(&supported))&&supported&&ok(metrics->GPUTemperature(&value)))return value;
    return {};
   }
   // Prefer total board consumption. Older GPUs may expose only GPU power;
   // both are real watt measurements, unrelated to the tuning percentage.
   if(ok(support->IsSupportedGPUTotalBoardPower(&supported))&&supported&&ok(metrics->GPUTotalBoardPower(&value))&&gpuControlRange(value,0,10000))return value;
   supported=false;
   if(ok(support->IsSupportedGPUPower(&supported))&&supported&&ok(metrics->GPUPower(&value))&&gpuControlRange(value,0,10000))return value;
   return {};
  };
  api.temperature=[sample](const auto& id){return sample(id,false);};
  api.power=[sample](const auto& id){return sample(id,true);};
  return queryWindowsAmdTelemetry(api);
 }
 std::vector<GpuControlDevice> inspect()override{std::vector<GpuControlDevice> out;for(auto& b:boards())out.push_back(b.d);return out;}
 void apply(const GpuControlDevice& selected,const GpuControlRequest& request)override{
  auto all=boards();const Board* b=nullptr;for(const auto& item:all)if(item.d.pci==selected.pci){if(b)throw std::runtime_error("AMD physical identity ambiguous");b=&item;}
  if(!b)throw std::runtime_error("AMD physical GPU unavailable");validateGpuControl(b->d,request);
  Ref<IADLXGPUList> list;check(system_->GetGPUs(&list.p));Ref<IADLXGPU> gpu;check(list->At(b->index,&gpu.p));
  const char* pnp=nullptr;check(gpu->PNPString(&pnp));
  const auto identity=pnp?canonicalWindowsGpuInstanceId(pnp):std::string();
  if(identity!=b->d.uuid||physicalPci(identity).value_or("")!=request.pci)throw std::runtime_error("AMD GPU mapping changed");
  if(request.action==GpuControlAction::Power||request.action==GpuControlAction::PowerDefault){
   Ref<IADLXManualPowerTuning> p;power(gpu.p,p);if(!p)throw std::runtime_error("AMD power interface disappeared");int old=0;check(p->GetPowerLimit(&old));const auto value=static_cast<int>(request.action==GpuControlAction::PowerDefault?b->d.powerDefault:request.value);
   check(p->SetPowerLimit(value));int read=0;if(!ok(p->GetPowerLimit(&read))||read!=value){int restored=0;if(!ok(p->SetPowerLimit(old))||!ok(p->GetPowerLimit(&restored))||restored!=old)throw GpuControlFailure(70,"AMD power readback and rollback failed");throw std::runtime_error("AMD power readback failed; previous limit restored");}return;
  }
  Ref<IADLXManualFanTuning1> f;fan(gpu.p,f);if(!f)throw std::runtime_error("AMD fan interface disappeared");
  Ref<IADLXManualFanTuningStateList> old,next;check(f->GetFanTuningStates(&old.p));const auto oldValues=fanValues(old.p);
  bool supportsZero=false,oldZero=false,newZero=false;check(f->IsSupportedZeroRPM(&supportsZero));if(supportsZero)check(f->GetZeroRPMState(&oldZero));
  if(request.action==GpuControlAction::FanAuto){check(f->GetDefaultFanTuningStates(&next.p));if(supportsZero)check(f->GetDefaultZeroRPMState(&newZero));}
  else {
   check(f->GetEmptyFanTuningStates(&next.p));if(!next||next->Size()!=oldValues.size())throw std::runtime_error("AMD fan curve size changed");
   for(unsigned i=0;i<next->Size();++i){Ref<IADLXManualFanTuningState> state;check(next->At(i,&state.p));check(state->SetTemperature(oldValues[i].first));check(state->SetFanSpeed(static_cast<int>(request.value)));}
  }
  int invalid=-1;check(f->IsValidFanTuningStates(next.p,&invalid));if(invalid!=-1)throw std::invalid_argument("AMD driver rejected the fan curve; no change applied");const auto expected=fanValues(next.p);
  try{
   if(supportsZero)check(f->SetZeroRPMState(newZero));check(f->SetFanTuningStates(next.p));
   Ref<IADLXManualFanTuningStateList> read;check(f->GetFanTuningStates(&read.p));bool actualZero=false;if(supportsZero)check(f->GetZeroRPMState(&actualZero));
   if(fanValues(read.p)!=expected||(supportsZero&&actualZero!=newZero))throw std::runtime_error("AMD fan readback failed");
  }catch(...){const bool curve=ok(f->SetFanTuningStates(old.p));const bool zero=!supportsZero||ok(f->SetZeroRPMState(oldZero));bool confirmed=curve&&zero;Ref<IADLXManualFanTuningStateList> restored;bool restoredZero=false;if(!ok(f->GetFanTuningStates(&restored.p))||!restored)confirmed=false;else if(fanValues(restored.p)!=oldValues)confirmed=false;if(supportsZero&&(!ok(f->GetZeroRPMState(&restoredZero))||restoredZero!=oldZero))confirmed=false;
   if(!confirmed)throw GpuControlFailure(70,"AMD fan change and rollback failed; inspect driver settings immediately");throw;}
 }
};
#else
class AmdWindows final:public GpuControlBackend {
public:
 std::vector<GpuControlDevice> inspect()override{return {};}
 void apply(const GpuControlDevice&,const GpuControlRequest&)override{throw std::runtime_error("This build has no ADLX support; configure OPENPUZZLE_ADLX_INCLUDE_DIR with an installed AMD SDK");}
};
#endif
}
WindowsTelemetryReport WindowsGpuTelemetry::inspectAmd(){
#ifdef OPENPUZZLE_WITH_ADLX
 try{return AmdWindows(true).telemetry();}catch(const std::exception& e){return {false,e.what(),{}};}
#else
 return {false,"This build has no AMD ADLX telemetry support",{}};
#endif
}
std::unique_ptr<GpuControlBackend> makeAmdControl(){return std::make_unique<AmdWindows>();}
}
#endif
