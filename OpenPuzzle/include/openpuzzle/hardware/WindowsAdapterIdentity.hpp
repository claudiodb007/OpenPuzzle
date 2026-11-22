#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace openpuzzle {
inline std::string canonicalWindowsGpuInstanceId(std::string value){
 if(value.size()<23||value.size()>512)return {};
 for(auto& c:value){if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
  if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='\\'||c=='&'||c=='_'||c=='-'||c=='.'))return {};}
 if(!value.starts_with("PCI\\VEN_")||value.substr(12,5)!="&DEV_")return {};
 for(auto pos:{8u,17u}){unsigned n=0;const auto r=std::from_chars(value.data()+pos,value.data()+pos+4,n,16);
  if(r.ec!=std::errc{}||r.ptr!=value.data()+pos+4)return {};}
 const auto instance=value.find('\\',4);
 if(instance==std::string::npos||instance+1==value.size()||value.find('\\',instance+1)!=std::string::npos)return {};
 return value;
}
struct WindowsAdapterLocation {unsigned bus=0,device=0,function=0;};
struct WindowsAdapterIdentity {
 std::string instanceId,name;
 unsigned vendor=0;
 std::optional<WindowsAdapterLocation> location;
 std::optional<std::array<unsigned char,8>> luid;
};
inline const WindowsAdapterIdentity* matchWindowsAdapter(const std::vector<WindowsAdapterIdentity>& adapters,unsigned vendor,
 const std::optional<WindowsAdapterLocation>& location,const std::optional<std::array<unsigned char,8>>& luid){
 if(!location&&!luid)return nullptr;
 if(location&&(location->bus>255||location->device>31||location->function>7))return nullptr;
 if(luid&&std::all_of(luid->begin(),luid->end(),[](auto b){return b==0;}))return nullptr;
 const WindowsAdapterIdentity* found=nullptr;
 for(const auto& a:adapters){
  const auto id=canonicalWindowsGpuInstanceId(a.instanceId);if(a.vendor!=vendor||id.empty())continue;
  unsigned reportedVendor=0;std::from_chars(id.data()+8,id.data()+12,reportedVendor,16);if(reportedVendor!=vendor)continue;
  if(location&&(!a.location||a.location->bus!=location->bus||a.location->device!=location->device||a.location->function!=location->function))continue;
  if(luid&&(!a.luid||*a.luid!=*luid))continue;
  if(found)return nullptr; // Never guess between PCI segments or duplicate identities.
  found=&a;
 }
 return found;
}
#ifdef _WIN32
std::vector<WindowsAdapterIdentity> inspectWindowsAdapterIdentities();
#endif
}
