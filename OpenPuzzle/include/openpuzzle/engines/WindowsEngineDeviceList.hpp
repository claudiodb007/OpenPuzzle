#pragma once
#include "openpuzzle/core/JsonString.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <algorithm>
#include <cstdint>
#include <charconv>
#include <set>
#include <sstream>
#include <vector>
namespace openpuzzle {
struct WindowsEngineDevice {
 int index=0,platform=0,native=0;
 std::string name;
 std::uint64_t memoryBytes=0;
 std::string displayName;
 std::string identity()const{return name+"; platform="+std::to_string(platform)+"; native="+std::to_string(native)+"; memory="+std::to_string(memoryBytes);}
};
// Use the engine's actual index, which may differ from a filtered GPU ordinal
// when an OpenCL platform also exposes CPUs or several vendors are installed.
inline std::vector<WindowsEngineDevice> parseWindowsEngineDevices(const std::string& json){
 if(json.empty()||json.size()>262144||json.find_first_not_of(" \r\n\t")==std::string::npos||json[json.find_first_not_of(" \r\n\t") ]!='[')
  throw std::invalid_argument("Invalid engine device list");
 boost::property_tree::ptree tree;std::istringstream input(json);boost::property_tree::read_json(input,tree);
 if(tree.size()>256)throw std::invalid_argument("Engine device list exceeds limit");
 std::vector<WindowsEngineDevice> result;std::set<int> indexes;
 for(const auto& item:tree){
  if(!item.first.empty())throw std::invalid_argument("Engine device list must be an array");
  std::set<std::string> fields;for(const auto& f:item.second)if(!fields.insert(f.first).second||!f.second.empty())throw std::invalid_argument("Ambiguous engine device field");
  auto integer=[&](const char* field,std::uint64_t maximum){const auto text=item.second.get<std::string>(field);std::uint64_t n=0;
   const auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);
   if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||n>maximum)throw std::invalid_argument("Invalid engine device integer");return n;};
  const auto index=static_cast<int>(integer("engine_index",255));
  if(!indexes.insert(index).second)throw std::invalid_argument("Duplicate engine device index");
  const auto gpu=item.second.get<std::string>("gpu");if(gpu!="true"&&gpu!="false")throw std::invalid_argument("Invalid engine device type");
  if(gpu=="false")continue;
  WindowsEngineDevice d;d.index=index;d.platform=static_cast<int>(integer("platform_index",63));d.native=static_cast<int>(integer("native_index",255));
  d.name=item.second.get<std::string>("name");d.memoryBytes=integer("memory_bytes",UINT64_MAX);
  d.displayName=item.second.get<std::string>("display_name","");
  if(d.name.empty()||d.name.size()>512||d.memoryBytes<1048576||std::any_of(d.name.begin(),d.name.end(),[](unsigned char c){return c<32||c==127;}))throw std::invalid_argument("Invalid engine device identity");
  if(d.displayName.size()>512||std::any_of(d.displayName.begin(),d.displayName.end(),[](unsigned char c){return c<32||c==127;}))throw std::invalid_argument("Invalid GPU display name");
  result.push_back(std::move(d));
 }
 return result;
}
inline std::string formatWindowsEngineDevices(const std::vector<WindowsEngineDevice>& devices){
 std::ostringstream out;out<<'[';bool first=true;
 for(const auto& d:devices){if(!first)out<<',';first=false;
  out<<"{\"engine_index\":"<<d.index<<",\"platform_index\":"<<d.platform<<",\"native_index\":"<<d.native
   <<",\"gpu\":true,\"name\":\""<<escapeJsonString(d.name)<<"\",\"memory_bytes\":"<<d.memoryBytes;
  if(!d.displayName.empty())out<<",\"display_name\":\""<<escapeJsonString(d.displayName)<<'"';out<<'}';}
 out<<']';return out.str();
}
}
