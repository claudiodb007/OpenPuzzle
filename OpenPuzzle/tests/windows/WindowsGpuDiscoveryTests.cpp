#include "openpuzzle/hardware/WindowsGpuDiscovery.hpp"
#include "openpuzzle/core/WindowsCliApplication.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace openpuzzle;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct CudaFixture {
 int initialized=0,count=2,countCalls=0,finalCount=2,version=12050;
 bool duplicate=false,badName=false,zeroMemory=false,failInfo=false;
} cuda;
int WINAPI cudaInit(unsigned int){return cuda.initialized;}
int WINAPI cudaVersion(int* p){*p=cuda.version;return 0;}
int WINAPI cudaCount(int* p){*p=cuda.countCalls++?cuda.finalCount:cuda.count;return 0;}
int WINAPI cudaGet(int* p,int index){*p=cuda.duplicate?0:index;return 0;}
int WINAPI cudaName(char* p,int n,int index){if(cuda.failInfo)return 999;if(cuda.badName){std::memset(p,'x',n);return 0;}
 const auto name=std::string(index?"NVIDIA future GPU":"NVIDIA test GPU");std::memcpy(p,name.c_str(),name.size()+1);return 0;}
int WINAPI cudaMemory(std::size_t* p,int){*p=cuda.zeroMemory?0:12ULL*1024*1024*1024;return 0;}
int WINAPI cudaAttribute(int* p,int field,int){require(field==16,"Incorrect CUDA attribute");*p=56;return 0;}
WindowsCudaDiscoveryApi cudaApi(){return {cudaInit,cudaVersion,cudaCount,cudaGet,cudaName,cudaMemory,cudaAttribute};}
struct ClFixture {
 int platformStatus=0,infoStatus=0,devicesStatus=0;
 unsigned int platformCount=2,deviceCount=2;
 bool changedPlatforms=false,changedDevices=false,duplicatePlatforms=false,nullDevice=false,duplicateDevice=false;
 bool badScalar=false,badString=false,invalidUtf8=false,missingTerminator=false,unavailable=false,cpuOnly=false;
} cl;
void* handle(std::uintptr_t n){return reinterpret_cast<void*>(n);}
int WINAPI clPlatforms(unsigned int n,void** values,unsigned int* count){
 *count=cl.platformCount;if(cl.platformStatus)return cl.platformStatus;
 if(n){for(unsigned int i=0;i<n;++i)values[i]=handle(cl.duplicatePlatforms?1:i+1);if(cl.changedPlatforms)++*count;}return 0;
}
int WINAPI clDevices(void* platform,std::uint64_t type,unsigned int n,void** values,unsigned int* count){
 require(type==0xFFFFFFFFULL,"OpenCL enumeration did not use ALL; indexes could shift");*count=cl.deviceCount;
 if(cl.devicesStatus)return cl.devicesStatus;
 if(n){for(unsigned int i=0;i<n;++i)values[i]=cl.nullDevice?nullptr:handle(cl.duplicateDevice?10:10*reinterpret_cast<std::uintptr_t>(platform)+i);
 if(cl.changedDevices)++*count;}return 0;
}
template<class T>int put(T value,std::size_t size,void* out,std::size_t* actual){*actual=cl.badScalar?sizeof(T)-1:sizeof(T);if(size<sizeof(T))return -30;std::memcpy(out,&value,sizeof(T));return 0;}
int WINAPI clInfo(void* device,unsigned int field,std::size_t size,void* out,std::size_t* actual){
 if(cl.infoStatus)return cl.infoStatus;
 const auto id=reinterpret_cast<std::uintptr_t>(device);
 if(field==0x1000)return put<std::uint64_t>(cl.cpuOnly||id==10?2:4,size,out,actual);
 if(field==0x1002)return put<unsigned int>(40,size,out,actual);
 if(field==0x101F)return put<std::uint64_t>(8ULL*1024*1024*1024,size,out,actual);
 if(field==0x1027||field==0x1028)return put<unsigned int>(cl.unavailable?0:1,size,out,actual);
 std::string s=field==0x102B?(id==11?"AMD Radeon RX 5700 XT":"NVIDIA future GPU"):field==0x102C?(id==11?"AMD":"NVIDIA"):"driver fixture";
 if(cl.invalidUtf8)s=std::string(1,static_cast<char>(0xFF));
 *actual=cl.badString?4097:s.size()+1;
 if(out){if(size<s.size()+1)return -30;std::memcpy(out,s.c_str(),s.size()+1);if(cl.missingTerminator)static_cast<char*>(out)[s.size()]='x';}return 0;
}
WindowsOpenClDiscoveryApi clApi(){return {clPlatforms,clDevices,clInfo};}
void badCuda(const char* description){const auto r=WindowsGpuDiscovery::queryCuda(cudaApi());require(r.state==GpuDiscoveryState::Error&&r.devices.empty(),description);}
void badCl(const char* description){const auto r=WindowsGpuDiscovery::queryOpenCl(clApi());require(r.state==GpuDiscoveryState::Error&&r.devices.empty(),description);}
class Environment {
 std::wstring name_,previous_;bool existed_=false;
public:
 Environment(const wchar_t* name,const wchar_t* value):name_(name){const auto n=GetEnvironmentVariableW(name,nullptr,0);if(n){previous_.resize(n);auto used=GetEnvironmentVariableW(name,previous_.data(),n);require(used&&used<n,"Environment changed");previous_.resize(used);existed_=true;}
 require(SetEnvironmentVariableW(name,value)!=0,"Cannot set test environment");}
 ~Environment(){SetEnvironmentVariableW(name_.c_str(),existed_?previous_.c_str():nullptr);}
};
}
int main(){try{
 cuda={};auto c=WindowsGpuDiscovery::queryCuda(cudaApi());
 require(c.state==GpuDiscoveryState::Available&&c.devices.size()==2&&c.devices[1].nativeDeviceIndex==1&&c.devices[0].memoryMiB==12288,"CUDA x64 memory or device ordering wrong");
 require(c.devices[0].driverVersion=="CUDA API 12.5","CUDA driver version wrong");
 cuda={};cuda.initialized=100;require(WindowsGpuDiscovery::queryCuda(cudaApi()).state==GpuDiscoveryState::Empty,"CUDA no-device response treated as failure");
 cuda={};cuda.count=0;require(WindowsGpuDiscovery::queryCuda(cudaApi()).state==GpuDiscoveryState::Empty,"Empty CUDA inventory not accepted");
 auto missing=cudaApi();missing.deviceMemory=nullptr;require(WindowsGpuDiscovery::queryCuda(missing).state==GpuDiscoveryState::Error,"Incomplete CUDA exports accepted");
 cuda={};cuda.initialized=999;badCuda("CUDA initialization failure hidden");
 cuda={};cuda.count=257;badCuda("Unbounded CUDA count accepted");
 cuda={};cuda.count=-1;badCuda("Negative CUDA count accepted");
 cuda={};cuda.duplicate=true;badCuda("Duplicate CUDA handle accepted");
 cuda={};cuda.badName=true;badCuda("Unterminated CUDA name accepted");
 cuda={};cuda.zeroMemory=true;badCuda("Invalid CUDA memory accepted");
 cuda={};cuda.failInfo=true;badCuda("Partial CUDA inventory survived an error");
 cuda={};cuda.version=0;badCuda("Invalid CUDA version accepted");
 cuda={};cuda.finalCount=1;badCuda("Changed CUDA inventory accepted");
 cl={};auto o=WindowsGpuDiscovery::queryOpenCl(clApi());
 require(o.state==GpuDiscoveryState::Available&&o.devices.size()==3,"CPU filtering or multi-platform OpenCL enumeration wrong");
 require(o.devices[0].name=="AMD Radeon RX 5700 XT"&&o.devices[0].nativeDeviceIndex==1&&o.devices[0].platformIndex==0,"OpenCL native indexes shifted by CPU filtering");
 require(o.devices[1].platformIndex==1&&o.devices[1].nativeDeviceIndex==0&&o.devices[2].inventoryIndex==2&&o.devices[0].memoryMiB==8192,"Multi-vendor indexes or x64 memory wrong");
 cl={};cl.unavailable=true;o=WindowsGpuDiscovery::queryOpenCl(clApi());require(o.devices.size()==3&&!o.devices[0].available&&!o.devices[0].compilerAvailable,"Unavailable OpenCL GPU was reported as usable");
 cl={};cl.cpuOnly=true;require(WindowsGpuDiscovery::queryOpenCl(clApi()).state==GpuDiscoveryState::Empty,"OpenCL CPU mistaken for GPU or KeyHunt readiness");
 cl={};cl.platformStatus=-1001;require(WindowsGpuDiscovery::queryOpenCl(clApi()).state==GpuDiscoveryState::Empty,"No OpenCL platforms treated as failure");
 cl={};cl.devicesStatus=-1;require(WindowsGpuDiscovery::queryOpenCl(clApi()).state==GpuDiscoveryState::Empty,"No OpenCL devices treated as failure");
 auto missingCl=clApi();missingCl.info=nullptr;require(WindowsGpuDiscovery::queryOpenCl(missingCl).state==GpuDiscoveryState::Error,"Incomplete OpenCL exports accepted");
 cl={};cl.platformCount=65;badCl("Unbounded OpenCL platform count accepted");
 cl={};cl.deviceCount=257;badCl("Unbounded OpenCL device count accepted");
 cl={};cl.platformCount=2;cl.deviceCount=200;badCl("Unbounded aggregate OpenCL device count accepted");
 cl={};cl.changedPlatforms=true;badCl("Changed OpenCL platforms accepted");
 cl={};cl.changedDevices=true;badCl("Changed OpenCL devices accepted");
 cl={};cl.duplicatePlatforms=true;badCl("Duplicate OpenCL platform accepted");
 cl={};cl.nullDevice=true;badCl("Null OpenCL device accepted");
 cl={};cl.duplicateDevice=true;badCl("Duplicate OpenCL device accepted");
 cl={};cl.badScalar=true;badCl("Wrong OpenCL scalar ABI accepted");
 cl={};cl.badString=true;badCl("Unbounded OpenCL string accepted");
 cl={};cl.invalidUtf8=true;badCl("Invalid OpenCL UTF-8 accepted");
 cl={};cl.missingTerminator=true;badCl("Unterminated OpenCL string accepted");
 cl={};cl.infoStatus=-30;badCl("Partial OpenCL inventory survived error");
 wchar_t system[MAX_PATH]{};require(GetSystemDirectoryW(system,MAX_PATH)>0,"System path unavailable");
 require(WindowsGpuDiscovery::systemLibraryPath(L"OpenCL.dll")==std::wstring(system)+L"\\OpenCL.dll","OpenCL loader uses a non-system path");
 require(WindowsGpuDiscovery::systemLibraryPath(L"nvcuda.dll")==std::wstring(system)+L"\\nvcuda.dll","CUDA loader uses a non-system path");
 bool rejected=false;try{WindowsGpuDiscovery::systemLibraryPath(L"..\\OpenCL.dll");}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Relative driver DLL accepted");
 WindowsGpuInventory inventory;inventory.nativeCpuAvailable=true;inventory.logicalProcessors=16;inventory.systemMemoryMiB=32768;
 cl={};inventory.opencl=WindowsGpuDiscovery::queryOpenCl(clApi());inventory.cuda={GpuDiscoveryState::Missing,"not installed",{}};
 require(!windowsGpuDiscoveryHasError(inventory),"AMD-only PC incorrectly requires NVIDIA");
 auto formatted=formatWindowsGpuInventory(inventory);require(formatted.find("CPU engine: KeyHunt")!=std::string::npos&&formatted.find("CUDA: NOT INSTALLED")!=std::string::npos&&formatted.find("OpenCL: DETECTED")!=std::string::npos&&formatted.find("PENDING")!=std::string::npos&&formatted.find("READY")==std::string::npos,"Hardware detection advertised unvalidated search readiness");
 // Hardware commands do not need a profile or a valid execution-slot environment.
 Environment profile(L"LOCALAPPDATA",nullptr),slot(L"OPENPUZZLE_EXECUTION_SLOT",L"bad-slot");int calls=0;
 WindowsCliApplication app({},[&]{++calls;return inventory;});std::ostringstream output;
 require(app.run({"devices"},output)==0&&calls==1,"Devices needs local storage or execution slot");
 for(const auto& args:std::vector<std::vector<std::string>>{{"devices","extra"},{"devices","--slot","cpu"},{"devices","--offline"}})
  require(app.run(args,output)==1&&calls==1,"Invalid device command queried hardware");
 inventory.cuda={GpuDiscoveryState::Error,"CUDA query failed",{}};
 require(app.run({"devices"},output)==2&&output.str().find("AMD Radeon RX 5700 XT")!=std::string::npos,"CUDA failure hid independent AMD inventory");
 cuda={};inventory.cuda=WindowsGpuDiscovery::queryCuda(cudaApi());inventory.opencl={GpuDiscoveryState::Error,"OpenCL query failed",{}};
 std::ostringstream nvidia;require(app.run({"devices"},nvidia)==2&&nvidia.str().find("NVIDIA test GPU")!=std::string::npos,"OpenCL failure hid CUDA inventory");
 inventory.opencl={};inventory.nativeCpuAvailable=false;require(app.run({"devices"},output)==2,"Unavailable native CPU accepted");
 std::cout<<"WindowsGpuDiscoveryTests passed: native CPU/KeyHunt pending, AMD/NVIDIA enumeration, x64 ABI, independent failures, bounds, driver paths and profile-free CLI\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
