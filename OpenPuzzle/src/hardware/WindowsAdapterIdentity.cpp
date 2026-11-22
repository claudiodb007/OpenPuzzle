#include "openpuzzle/hardware/WindowsAdapterIdentity.hpp"
#include <windows.h>
#include <setupapi.h>
#include <cstring>
#include <set>
namespace openpuzzle {
namespace {
// GUID_DISPLAY_DEVICE_ARRIVAL includes render-only and compute-only adapters.
constexpr GUID displayArrival{0x1ca05180,0xa699,0x450a,{0x9a,0x0c,0xde,0x4f,0xbe,0x3d,0xdd,0x89}};
struct OpenAdapter {const wchar_t* name;unsigned handle;LUID luid;};
struct CloseAdapter {unsigned handle;};
static_assert(sizeof(OpenAdapter)==24&&offsetof(OpenAdapter,luid)==12&&sizeof(CloseAdapter)==4);
using Open=LONG(WINAPI*)(OpenAdapter*);using Close=LONG(WINAPI*)(CloseAdapter*);
struct Devices {HDEVINFO value=SetupDiGetClassDevsW(&displayArrival,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 ~Devices(){if(value!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(value);}};
struct Gdi {HMODULE value=LoadLibraryExW(L"gdi32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);~Gdi(){if(value)FreeLibrary(value);}};
std::string utf8(const wchar_t* value,std::size_t count){
 if(!count||count>512)return {};const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,static_cast<int>(count),nullptr,0,nullptr,nullptr);
 if(n<=0)return {};std::string out(n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,static_cast<int>(count),out.data(),n,nullptr,nullptr);return out;
}
std::optional<unsigned> number(HDEVINFO devices,SP_DEVINFO_DATA& device,DWORD property){
 DWORD type=0,size=0,value=0;
 if(!SetupDiGetDeviceRegistryPropertyW(devices,&device,property,&type,reinterpret_cast<BYTE*>(&value),sizeof value,&size)||type!=REG_DWORD||size!=sizeof value)return {};
 return value;
}
std::string name(HDEVINFO devices,SP_DEVINFO_DATA& device,DWORD property){
 std::array<wchar_t,513> text{};DWORD type=0,size=0;
 if(!SetupDiGetDeviceRegistryPropertyW(devices,&device,property,&type,reinterpret_cast<BYTE*>(text.data()),sizeof text,&size)||
    type!=REG_SZ||size<sizeof(wchar_t)||size>sizeof text||size%sizeof(wchar_t))return {};
 const auto count=size/sizeof(wchar_t);if(text[count-1]!=0||std::find(text.begin(),text.begin()+count-1,0)!=text.begin()+count-1)return {};
 auto value=utf8(text.data(),count-1);if(value.size()>512||std::any_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;}))return {};return value;
}
}
std::vector<WindowsAdapterIdentity> inspectWindowsAdapterIdentities(){
 Devices devices;if(devices.value==INVALID_HANDLE_VALUE)return {};
 Gdi gdi;auto open=gdi.value?reinterpret_cast<Open>(GetProcAddress(gdi.value,"D3DKMTOpenAdapterFromDeviceName")):nullptr;
 auto close=gdi.value?reinterpret_cast<Close>(GetProcAddress(gdi.value,"D3DKMTCloseAdapter")):nullptr;
 std::vector<WindowsAdapterIdentity> result;std::set<std::string> seen;
 for(DWORD index=0;index<=256;++index){
  SP_DEVICE_INTERFACE_DATA iface{};iface.cbSize=sizeof iface;
  if(!SetupDiEnumDeviceInterfaces(devices.value,nullptr,&displayArrival,index,&iface)){
   if(GetLastError()!=ERROR_NO_MORE_ITEMS)return {};return result;
  }
  if(index==256)return {};
  DWORD needed=0;SetupDiGetDeviceInterfaceDetailW(devices.value,&iface,nullptr,0,&needed,nullptr);
  if(GetLastError()!=ERROR_INSUFFICIENT_BUFFER||needed<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)||needed>65536)return {};
  std::vector<unsigned char> bytes(needed,0);auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(bytes.data());detail->cbSize=sizeof *detail;
  SP_DEVINFO_DATA device{};device.cbSize=sizeof device;
  if(!SetupDiGetDeviceInterfaceDetailW(devices.value,&iface,detail,needed,nullptr,&device))return {};
  const auto pathBytes=needed-static_cast<DWORD>(offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA_W,DevicePath));
  if(pathBytes<sizeof(wchar_t)||pathBytes%sizeof(wchar_t)||std::find(detail->DevicePath,detail->DevicePath+pathBytes/sizeof(wchar_t),0)==detail->DevicePath+pathBytes/sizeof(wchar_t))return {};
  std::array<wchar_t,513> id{};DWORD length=0;
  if(!SetupDiGetDeviceInstanceIdW(devices.value,&device,id.data(),static_cast<DWORD>(id.size()),&length)||length<2||length>id.size()||id[length-1]!=0)continue;
  WindowsAdapterIdentity a;a.instanceId=canonicalWindowsGpuInstanceId(utf8(id.data(),length-1));if(a.instanceId.empty())continue;
  if(!seen.insert(a.instanceId).second)return {};
  std::from_chars(a.instanceId.data()+8,a.instanceId.data()+12,a.vendor,16);
  a.name=name(devices.value,device,SPDRP_FRIENDLYNAME);if(a.name.empty())a.name=name(devices.value,device,SPDRP_DEVICEDESC);
  const auto bus=number(devices.value,device,SPDRP_BUSNUMBER),address=number(devices.value,device,SPDRP_ADDRESS);
  if(bus&&address&&*bus<=255&&(*address>>16)<=31&&(*address&0xffff)<=7)a.location=WindowsAdapterLocation{*bus,*address>>16,*address&0xffff};
  if(open&&close){OpenAdapter adapter{detail->DevicePath,0,{}};
   if(open(&adapter)==0&&adapter.handle){std::array<unsigned char,8> luid{};std::memcpy(luid.data(),&adapter.luid,8);CloseAdapter closed{adapter.handle};close(&closed);
    if(std::any_of(luid.begin(),luid.end(),[](auto b){return b!=0;}))a.luid=luid;}}
  result.push_back(std::move(a));
 }
 return {};
}
}
