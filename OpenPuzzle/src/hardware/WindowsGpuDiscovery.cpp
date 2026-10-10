#include "openpuzzle/hardware/WindowsGpuDiscovery.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace openpuzzle {
namespace {
static_assert(sizeof(int)==4 && sizeof(unsigned int)==4 && sizeof(std::size_t)==8,
              "Windows GPU discovery requires the documented x64 ABI");
constexpr unsigned int maxDevices=256, maxPlatforms=64, maxString=4096;
constexpr std::uint64_t clAll=0xFFFFFFFFULL, clGpu=4;
constexpr unsigned int clType=0x1000, clUnits=0x1002, clMemory=0x101F,
  clAvailable=0x1027, clCompiler=0x1028, clName=0x102B,
  clVendor=0x102C, clDriver=0x102D;
WindowsGpuBackend failure(const std::string& message) {
  return {GpuDiscoveryState::Error,message,{}};
}
void check(int result,const char* operation) {
  if(result!=0)throw std::runtime_error(std::string(operation)+" failed (driver code "+std::to_string(result)+")");
}
bool validText(const std::string& s) {
  return !s.empty() && std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32||c==127;}) &&
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0)>0;
}
template<class T> T scalar(const WindowsOpenClDiscoveryApi& api,void* device,unsigned int field) {
  T value{};std::size_t bytes=0;
  check(api.info(device,field,sizeof(value),&value,&bytes),"OpenCL device information");
  if(bytes!=sizeof(value))throw std::runtime_error("OpenCL scalar size is invalid");
  return value;
}
std::string text(const WindowsOpenClDiscoveryApi& api,void* device,unsigned int field) {
  std::size_t size=0;check(api.info(device,field,0,nullptr,&size),"OpenCL string size");
  if(size<2||size>maxString)throw std::runtime_error("OpenCL string size is invalid");
  std::vector<char> bytes(size,1);std::size_t actual=0;
  check(api.info(device,field,size,bytes.data(),&actual),"OpenCL string information");
  if(actual!=size||bytes.back()!=0||std::find(bytes.begin(),bytes.end()-1,0)!=bytes.end()-1)
    throw std::runtime_error("OpenCL string changed or lacks a unique terminator");
  std::string value(bytes.data(),size-1);
  if(!validText(value))throw std::runtime_error("OpenCL string is invalid UTF-8 or contains controls");
  return value;
}
class Module {
  HMODULE handle_=nullptr;
public:
  explicit Module(const std::wstring& path):handle_(LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)){}
  ~Module(){if(handle_)FreeLibrary(handle_);}
  bool valid()const{return handle_!=nullptr;}
  template<class T>T function(const char* name)const{return reinterpret_cast<T>(GetProcAddress(handle_,name));}
};
template<class Query> WindowsGpuBackend load(const std::wstring& name,Query query) {
  try {
    const auto path=WindowsGpuDiscovery::systemLibraryPath(name);
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES){
      const auto error=GetLastError();
      if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND)
        return {GpuDiscoveryState::Missing,"Driver library is not installed",{}};
      return failure("Driver library cannot be inspected (Windows code "+std::to_string(error)+")");
    }
    if(attributes&FILE_ATTRIBUTE_DIRECTORY)return failure("Driver library path is a directory");
    Module module(path);
    if(!module.valid())return failure("Installed driver library cannot be loaded (Windows code "+std::to_string(GetLastError())+")");
    return query(module);
  }catch(const std::exception& e){return failure(e.what());}
}
const char* stateName(GpuDiscoveryState state){
  switch(state){case GpuDiscoveryState::Missing:return "NOT INSTALLED";case GpuDiscoveryState::Empty:return "NO GPU";
    case GpuDiscoveryState::Available:return "DETECTED";case GpuDiscoveryState::Error:return "UNAVAILABLE";}
  return "UNAVAILABLE";
}
}

std::wstring WindowsGpuDiscovery::systemLibraryPath(const std::wstring& name) {
  if(name!=L"OpenCL.dll"&&name!=L"nvcuda.dll")throw std::invalid_argument("Unrecognized GPU driver library");
  std::wstring system(32768,L'\0');const auto length=GetSystemDirectoryW(system.data(),static_cast<unsigned int>(system.size()));
  if(!length||length>=system.size())throw std::runtime_error("Windows system directory is unavailable");
  system.resize(length);return system+L"\\"+name;
}
WindowsGpuBackend WindowsGpuDiscovery::queryCuda(const WindowsCudaDiscoveryApi& api) {
  try {
    if(!api.init||!api.driverVersion||!api.deviceCount||!api.deviceGet||!api.deviceName||!api.deviceMemory||!api.deviceAttribute)
      throw std::runtime_error("CUDA driver exports are incomplete");
    const auto initialized=api.init(0);
    if(initialized==100)return {GpuDiscoveryState::Empty,"CUDA driver reports no device",{}};
    check(initialized,"CUDA initialization");
    int count=-1;check(api.deviceCount(&count),"CUDA device count");
    if(count<0||count>static_cast<int>(maxDevices))throw std::runtime_error("CUDA device count is invalid");
    if(count==0)return {GpuDiscoveryState::Empty,"CUDA driver reports no device",{}};
    int version=0;check(api.driverVersion(&version),"CUDA driver version");
    if(version<=0)throw std::runtime_error("CUDA driver version is invalid");
    WindowsGpuBackend result{GpuDiscoveryState::Available,"Enumeration only; engine and benchmark validation pending",{}};
    std::set<int> handles;
    for(int ordinal=0;ordinal<count;++ordinal){
      int device=-1;check(api.deviceGet(&device,ordinal),"CUDA device handle");
      if(device<0||!handles.insert(device).second)throw std::runtime_error("CUDA device handle is invalid or duplicated");
      std::array<char,256> name;name.fill(1);check(api.deviceName(name.data(),static_cast<int>(name.size()),device),"CUDA device name");
      const auto end=std::find(name.begin(),name.end(),0);
      if(end==name.end())throw std::runtime_error("CUDA name lacks a terminator");
      WindowsGpuDevice info;info.inventoryIndex=ordinal;info.nativeDeviceIndex=ordinal;info.name.assign(name.begin(),end);
      if(!validText(info.name))throw std::runtime_error("CUDA device name is invalid");
      std::size_t memory=0;int units=0;
      check(api.deviceMemory(&memory,device),"CUDA device memory");
      check(api.deviceAttribute(&units,16,device),"CUDA multiprocessor count");
      if(memory<1048576||units<=0)throw std::runtime_error("CUDA device memory or multiprocessor count is invalid");
      info.vendor="NVIDIA";info.driverVersion="CUDA API "+std::to_string(version/1000)+'.'+std::to_string((version%1000)/10);
      info.memoryMiB=memory/1048576;info.computeUnits=static_cast<unsigned int>(units);info.available=true;
      result.devices.push_back(std::move(info));
    }
    int finalCount=-1;check(api.deviceCount(&finalCount),"CUDA final device count");
    if(finalCount!=count)throw std::runtime_error("CUDA inventory changed during enumeration; retry required");
    return result;
  }catch(const std::exception& e){return failure(e.what());}
}
WindowsGpuBackend WindowsGpuDiscovery::queryOpenCl(const WindowsOpenClDiscoveryApi& api) {
  try {
    if(!api.platforms||!api.devices||!api.info)throw std::runtime_error("OpenCL driver exports are incomplete");
    unsigned int count=0;const auto code=api.platforms(0,nullptr,&count);
    if(code==-1001)return {GpuDiscoveryState::Empty,"OpenCL driver reports no platform",{}};
    check(code,"OpenCL platform count");
    if(count==0)return {GpuDiscoveryState::Empty,"OpenCL driver reports no platform",{}};
    if(count>maxPlatforms)throw std::runtime_error("OpenCL platform count exceeds the inventory limit");
    std::vector<void*> platforms(count,nullptr);unsigned int actual=0;
    check(api.platforms(count,platforms.data(),&actual),"OpenCL platforms");
    if(actual!=count)throw std::runtime_error("OpenCL platform count changed; retry required");
    WindowsGpuBackend result{GpuDiscoveryState::Empty,"No OpenCL GPU was reported",{}};
    std::set<void*> seenPlatforms,seenDevices;unsigned int total=0;
    for(unsigned int p=0;p<count;++p){
      auto platform=platforms[p];if(!platform||!seenPlatforms.insert(platform).second)throw std::runtime_error("OpenCL platform is null or duplicated");
      unsigned int n=0;const auto status=api.devices(platform,clAll,0,nullptr,&n);
      if(status==-1)continue;
      check(status,"OpenCL device count");
      if(n==0)continue;
      if(n>maxDevices||total>maxDevices-n)throw std::runtime_error("OpenCL device count exceeds the inventory limit");total+=n;
      std::vector<void*> devices(n,nullptr);unsigned int returned=0;
      check(api.devices(platform,clAll,n,devices.data(),&returned),"OpenCL devices");
      if(returned!=n)throw std::runtime_error("OpenCL device count changed; retry required");
      for(unsigned int d=0;d<n;++d){
        auto device=devices[d];if(!device||!seenDevices.insert(device).second)throw std::runtime_error("OpenCL device is null or duplicated");
        if(!(scalar<std::uint64_t>(api,device,clType)&clGpu))continue;
        WindowsGpuDevice info;info.inventoryIndex=static_cast<int>(result.devices.size());info.platformIndex=static_cast<int>(p);info.nativeDeviceIndex=static_cast<int>(d);
        info.name=text(api,device,clName);info.vendor=text(api,device,clVendor);info.driverVersion=text(api,device,clDriver);
        const auto memory=scalar<std::uint64_t>(api,device,clMemory);info.computeUnits=scalar<unsigned int>(api,device,clUnits);
        const auto available=scalar<unsigned int>(api,device,clAvailable),compiler=scalar<unsigned int>(api,device,clCompiler);
        if(memory<1048576||!info.computeUnits||available>1||compiler>1)throw std::runtime_error("OpenCL device capability is invalid");
        info.memoryMiB=memory/1048576;info.available=available!=0;info.compilerAvailable=compiler!=0;
        result.devices.push_back(std::move(info));
      }
    }
    if(!result.devices.empty()){result.state=GpuDiscoveryState::Available;result.detail="Enumeration only; engine index mapping, kernel and benchmark validation pending";}
    return result;
  }catch(const std::exception& e){return failure(e.what());}
}
WindowsGpuInventory WindowsGpuDiscovery::inspect() {
  WindowsGpuInventory result;
  SYSTEM_INFO system{};GetNativeSystemInfo(&system);
  MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);
  result.logicalProcessors=GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  result.nativeCpuAvailable=system.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 &&
    result.logicalProcessors!=0 && GlobalMemoryStatusEx(&memory)!=0;
  if(result.nativeCpuAvailable)result.systemMemoryMiB=memory.ullTotalPhys/1048576;
  result.cuda=load(L"nvcuda.dll",[](const Module& m){
    WindowsCudaDiscoveryApi a;
    a.init=m.function<decltype(a.init)>("cuInit");a.driverVersion=m.function<decltype(a.driverVersion)>("cuDriverGetVersion");
    a.deviceCount=m.function<decltype(a.deviceCount)>("cuDeviceGetCount");a.deviceGet=m.function<decltype(a.deviceGet)>("cuDeviceGet");
    a.deviceName=m.function<decltype(a.deviceName)>("cuDeviceGetName");a.deviceMemory=m.function<decltype(a.deviceMemory)>("cuDeviceTotalMem_v2");
    a.deviceAttribute=m.function<decltype(a.deviceAttribute)>("cuDeviceGetAttribute");return queryCuda(a);
  });
  result.opencl=load(L"OpenCL.dll",[](const Module& m){
    WindowsOpenClDiscoveryApi a;a.platforms=m.function<decltype(a.platforms)>("clGetPlatformIDs");
    a.devices=m.function<decltype(a.devices)>("clGetDeviceIDs");a.info=m.function<decltype(a.info)>("clGetDeviceInfo");return queryOpenCl(a);
  });return result;
}
bool windowsGpuDiscoveryHasError(const WindowsGpuInventory& inventory){return !inventory.nativeCpuAvailable||inventory.cuda.state==GpuDiscoveryState::Error||inventory.opencl.state==GpuDiscoveryState::Error;}
std::string formatWindowsGpuInventory(const WindowsGpuInventory& inventory) {
  std::ostringstream out;out<<"Hardware inventory (enumeration; no search kernels)\n"
    <<"CPU: "<<(inventory.nativeCpuAvailable?"DETECTED (native x64)":"UNAVAILABLE")
    <<"\nLogical processors: "<<inventory.logicalProcessors<<"\nSystem memory MiB: "<<inventory.systemMemoryMiB
    <<"\nCPU engine: KeyHunt; Windows executable, self-test and benchmark PENDING\n";
  const auto show=[&](const char* name,const WindowsGpuBackend& backend){
    out<<name<<": "<<stateName(backend.state)<<"\nDetails: "<<backend.detail<<'\n';
    for(const auto& d:backend.devices){out<<"Inventory index: "<<d.inventoryIndex<<"\nDevice: "<<d.name<<"\nVendor: "<<d.vendor<<"\nDriver: "<<d.driverVersion
      <<"\nMemory MiB: "<<d.memoryMiB<<"\n"<<(d.platformIndex<0?"Multiprocessors: ":"Compute units: ")<<d.computeUnits
      <<"\nNative device index: "<<d.nativeDeviceIndex<<"\nDevice available: "<<(d.available?"yes":"no")<<'\n';
      if(d.platformIndex>=0)out<<"OpenCL platform: "<<d.platformIndex<<"\nOpenCL compiler: "<<(d.compilerAvailable?"yes":"no")<<'\n';}
  };show("CUDA",inventory.cuda);show("OpenCL",inventory.opencl);
  out<<"GPU engine: BitCrack; Windows executables, kernel tests and benchmarks PENDING\n"
    <<"Search engines and benchmark profiles: PENDING\nInventory indexes require engine verification before launch.\n";return out.str();
}
} // namespace openpuzzle
