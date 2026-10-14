#include "openpuzzle/hardware/WindowsGpuTelemetry.hpp"
#include <cstring>
#include <iostream>
#include <limits>
using namespace openpuzzle;
namespace {unsigned shutdowns=0,countCalls=0;bool failInit=false,unsupported=false,duplicate=false,changed=false;unsigned temperature=62;
int init(){return failInit?18:0;}int shutdown(){++shutdowns;return 0;}int count(unsigned* n){*n=duplicate||(changed&&countCalls++)?2:1;return 0;}
int handle(unsigned n,void** h){*h=reinterpret_cast<void*>(static_cast<std::uintptr_t>(n+1));return 0;}
int name(void*,char* s,unsigned){std::strcpy(s,"NVIDIA fixture GPU");return 0;}
int pci(void*,WindowsNvmlPciInfo* p){p->domain=0;p->bus=8;p->device=0;std::strcpy(p->busId,"00000000:08:00.0");return 0;}
int temp(void*,unsigned sensor,unsigned* v){*v=temperature;return sensor?2:unsupported?3:0;}int power(void*,unsigned* v){*v=123456;return unsupported?3:0;}int limit(void*,unsigned* v){*v=290000;return 0;}
void require(bool b){if(!b)throw std::runtime_error("NVML read-only contract failed");}}
int main(){try{
 WindowsNvmlApi a{init,shutdown,count,handle,name,pci,temp,power,limit};auto r=WindowsGpuTelemetry::query(a);
 require(r.complete&&r.readings.size()==1&&shutdowns==1&&r.readings[0].values.temperatureC==62&&r.readings[0].values.powerDrawW==123.456&&r.readings[0].values.powerLimitW==290);
 require(r.readings[0].values.pciBusId=="00000000:08:00.0"&&formatWindowsTelemetry(r).find("62")!=std::string::npos);
 a.uuid=[](void*,char* value,unsigned)->int{std::strcpy(value,"GPU-12345678-1234-5678-9abc-123456789abc");return 0;};
 r=WindowsGpuTelemetry::query(a);require(r.complete&&r.readings[0].uuid=="12345678-1234-5678-9abc-123456789abc");
 a.uuid=[](void*,char* value,unsigned)->int{std::strcpy(value,"invalid");return 0;};r=WindowsGpuTelemetry::query(a);require(!r.complete&&r.readings.empty());
 a.uuid=nullptr;
 unsupported=true;r=WindowsGpuTelemetry::query(a);require(r.complete&&!r.readings[0].values.temperatureC&&!r.readings[0].values.powerDrawW&&r.readings[0].values.powerLimitW==290);unsupported=false;
 temperature=(std::numeric_limits<unsigned>::max)();r=WindowsGpuTelemetry::query(a);require(r.complete&&!r.readings[0].values.temperatureC);temperature=62;
 const auto previous=shutdowns;failInit=true;r=WindowsGpuTelemetry::query(a);require(!r.complete&&r.readings.empty()&&shutdowns==previous);failInit=false;
 a.pci=nullptr;r=WindowsGpuTelemetry::query(a);require(!r.complete&&shutdowns==previous);a.pci=pci;
 changed=true;countCalls=0;r=WindowsGpuTelemetry::query(a);require(!r.complete&&r.readings.empty());changed=false;
 duplicate=true;r=WindowsGpuTelemetry::query(a);require(!r.complete&&r.readings.empty());duplicate=false;
 a.pci=[](void*,WindowsNvmlPciInfo* p){p->bus=8;std::strcpy(p->busId,"0000:09:00.0");return 0;};r=WindowsGpuTelemetry::query(a);require(!r.complete);
 require(WindowsGpuTelemetry::systemNvmlPath().ends_with(L"\\nvml.dll"));
 std::cout<<"WindowsGpuTelemetryTests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
