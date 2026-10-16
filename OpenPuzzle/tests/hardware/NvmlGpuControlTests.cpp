#include "openpuzzle/hardware/NvmlControlApi.hpp"
#include <cstring>
#include <iostream>
using namespace openpuzzle;
namespace {
unsigned power=333000;unsigned speeds[2]{35,40},policies[2]{0,0};int writes=0,failFan=-1;bool deny=false,unsupported=false;
int count(unsigned* n){*n=1;return 0;}int handle(unsigned,void** p){*p=reinterpret_cast<void*>(1);return 0;}
int name(void*,char* s,unsigned){std::strcpy(s,"NVIDIA fixture");return 0;}int uuid(void*,char* s,unsigned){std::strcpy(s,"GPU-fixture");return 0;}
int pci(void*,NvmlControlPci* p){std::strcpy(p->busId,"00000000:08:00.0");return 0;}
int limit(void*,unsigned* p){*p=power;return 0;}int defaults(void*,unsigned* p){*p=333000;return 0;}
int range(void*,unsigned* a,unsigned* b){*a=100000;*b=351500;return 0;}int fans(void*,unsigned* n){*n=2;return 0;}
int fanRange(void*,unsigned* a,unsigned* b){*a=20;*b=100;return unsupported?3:0;}
int policy(void*,unsigned f,unsigned* p){*p=policies[f];return 0;}int target(void*,unsigned f,unsigned* p){*p=speeds[f];return 0;}
int temperature(void*,unsigned,unsigned* p){*p=67;return 0;}
int setPower(void*,unsigned p){if(deny)return 4;++writes;power=p;return 0;}
int setFan(void*,unsigned f,unsigned p){if(deny)return 4;if(static_cast<int>(f)==failFan){failFan=-1;return 4;}++writes;policies[f]=1;speeds[f]=p;return 0;}
int automatic(void*,unsigned f){if(deny)return 4;++writes;policies[f]=0;speeds[f]=35+5*f;return 0;}
void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
template<class F>void refuses(F f){bool rejected=false;try{f();}catch(...){rejected=true;}require(rejected,"Driver failure accepted");}
}
int main(){try{
 NvmlControlApi api{count,handle,name,uuid,pci,limit,defaults,fans,range,fanRange,policy,target,temperature,setPower,setFan,automatic};auto b=makeNvmlControlFixture(api);auto d=b->inspect().at(0);
 require(writes==0&&d.powerNow==333&&d.powerMax==333&&d.fanMode=="auto"&&d.fanMin==30,"NVML read-only capabilities, units or mixed auto fans failed");
 GpuControlRequest r{d.vendor,d.pci,d.uuid,GpuControlAction::Power,250};applyGpuControlChecked(*b,r);require(power==250000,"NVML milliwatt conversion failed");
 deny=true;r.value=200;refuses([&]{applyGpuControlChecked(*b,r);});require(power==250000,"Permission failure mutated power");deny=false;
 r.action=GpuControlAction::FanManual;r.value=70;applyGpuControlChecked(*b,r);require(speeds[0]==70&&speeds[1]==70&&policies[0]==1&&policies[1]==1,"NVML did not apply all fans");
 failFan=1;r.value=80;refuses([&]{applyGpuControlChecked(*b,r);});require(speeds[0]==70&&speeds[1]==70&&policies[0]==1&&policies[1]==1,"NVML partial failure did not restore both fans");
 r.action=GpuControlAction::FanAuto;applyGpuControlChecked(*b,r);require(policies[0]==0&&policies[1]==0,"NVML did not restore driver control");
 unsupported=true;require(!b->inspect()[0].fan,"Unsupported fan controls were exposed");r.action=GpuControlAction::FanManual;r.value=60;const auto before=writes;refuses([&]{applyGpuControlChecked(*b,r);});require(writes==before,"Unsupported fan operation called setters");
 std::cout<<"NvmlGpuControlTests passed: real NVML adapter with injected driver, no-write inspection, units, permissions, all fans, rollback and automatic restoration\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
