#include "GpuControlInternal.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
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
void check(ADLX_RESULT r){if(ok(r))return;throw GpuControlFailure(r==ADLX_RESET_NEEDED?78:r==ADLX_NOT_SUPPORTED?69:1,r==ADLX_RESET_NEEDED?"AMD driver requires a tuning reset; OpenPuzzle will not reset other GPU settings":"AMD ADLX operation failed ("+std::to_string(r)+"); check driver support and administrator permission");}
class AmdWindows final:public GpuControlBackend {
 HMODULE dll_=nullptr;ADLXTerminate_Fn terminate_=nullptr;IADLXSystem* system_=nullptr;IADLMapping* mapping_=nullptr;
 Ref<IADLXGPUTuningServices> tuning_;Ref<IADLXPerformanceMonitoringServices> monitoring_;
 struct Board {GpuControlDevice d;unsigned index=0;};
 void power(IADLXGPU* gpu,Ref<IADLXManualPowerTuning1>& result){
  Ref<IADLXInterface> base;if(!tuning_||!ok(tuning_->GetManualPowerTuning(gpu,&base.p))||!base)return;
  base->QueryInterface(IADLXManualPowerTuning1::IID(),reinterpret_cast<void**>(&result.p));
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
  std::vector<Board> result;if(!system_||!mapping_||!tuning_)return result;Ref<IADLXGPUList> list;if(!ok(system_->GetGPUs(&list.p))||!list||list->Size()>64)return result;
  for(unsigned i=0;i<list->Size();++i){
   Ref<IADLXGPU> gpu;if(!ok(list->At(i,&gpu.p))||!gpu)continue;int bus=-1,device=-1,function=-1;
   if(!ok(mapping_->BdfFromADLXGPU(gpu.p,&bus,&device,&function))||bus<0||device<0||function<0)continue;
   Board b;b.index=i;auto& d=b.d;d.vendor="amd";d.pci=controlPci(0,bus,device,function);if(!gpuControlPci(d.pci))continue;
   const char* name=nullptr;if(ok(gpu->Name(&name))&&name)d.name=name;d.powerUnit="%";d.detail="ADLX; power is percent offset, not watts; driver defaults restore fans only";
   Ref<IADLXManualPowerTuning1> p;power(gpu.p,p);ADLX_IntRange range{};int current=0,original=0;
   if(p&&ok(p->GetPowerLimitRange(&range))&&ok(p->GetPowerLimit(&current))&&ok(p->GetPowerLimitDefault(&original))&&range.step>0&&range.minValue<=original&&original<=range.maxValue){d.power=true;d.powerNow=current;d.powerMin=range.minValue;d.powerMax=range.maxValue;d.powerDefault=original;d.powerStep=range.step;}
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
 AmdWindows(){
  wchar_t path[32768]{};auto n=GetSystemDirectoryW(path,32768);if(!n||n>=32740)return;const auto full=std::wstring(path,n)+L"\\amdadlx64.dll";
  dll_=LoadLibraryExW(full.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!dll_)return;
  auto init=reinterpret_cast<ADLXInitialize2_Fn>(GetProcAddress(dll_,"ADLXInitialize2"));terminate_=reinterpret_cast<ADLXTerminate_Fn>(GetProcAddress(dll_,"ADLXTerminate"));
  if(!init||!terminate_||!ok(init(ADLX_MAKE_FULL_VER(1,4,0,0),&system_,&mapping_))){system_=nullptr;return;}
  system_->GetGPUTuningServices(&tuning_.p);system_->GetPerformanceMonitoringServices(&monitoring_.p);
 }
 ~AmdWindows(){if(monitoring_.p){monitoring_.p->Release();monitoring_.p=nullptr;}if(tuning_.p){tuning_.p->Release();tuning_.p=nullptr;}if(system_&&terminate_)terminate_();if(dll_)FreeLibrary(dll_);}
 std::vector<GpuControlDevice> inspect()override{std::vector<GpuControlDevice> out;for(auto& b:boards())out.push_back(b.d);return out;}
 void apply(const GpuControlDevice& selected,const GpuControlRequest& request)override{
  auto all=boards();const Board* b=nullptr;for(const auto& item:all)if(item.d.pci==selected.pci){if(b)throw std::runtime_error("AMD physical identity ambiguous");b=&item;}
  if(!b)throw std::runtime_error("AMD physical GPU unavailable");validateGpuControl(b->d,request);
  Ref<IADLXGPUList> list;check(system_->GetGPUs(&list.p));Ref<IADLXGPU> gpu;check(list->At(b->index,&gpu.p));
  int bus=-1,device=-1,function=-1;check(mapping_->BdfFromADLXGPU(gpu.p,&bus,&device,&function));if(controlPci(0,bus,device,function)!=request.pci)throw std::runtime_error("AMD GPU mapping changed");
  if(request.action==GpuControlAction::Power||request.action==GpuControlAction::PowerDefault){
   Ref<IADLXManualPowerTuning1> p;power(gpu.p,p);if(!p)throw std::runtime_error("AMD power interface disappeared");int old=0;check(p->GetPowerLimit(&old));const auto value=static_cast<int>(request.action==GpuControlAction::PowerDefault?b->d.powerDefault:request.value);
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
std::unique_ptr<GpuControlBackend> makeAmdControl(){return std::make_unique<AmdWindows>();}
}
#endif
