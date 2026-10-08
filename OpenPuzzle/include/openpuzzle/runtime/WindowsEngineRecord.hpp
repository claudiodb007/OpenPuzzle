#pragma once
#include "openpuzzle/runtime/WindowsRuntimeRecord.hpp"
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
namespace openpuzzle {
struct WindowsEngineRecord {
  WindowsRuntimeRecord process;
  std::string generation;
  static constexpr std::string_view platform="windows-engine-filetime-v1";
  bool operator==(const WindowsEngineRecord&) const = default;
  bool valid()const{
    if(!process.valid() || generation.empty() || generation.size()>80)return false;
    for(char c:generation)if(!((c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='-'))return false;
    return true;
  }
  std::string encode()const{
    if(!valid())return {};
    return std::string(platform)+'\n'+std::to_string(process.pid)+'\n'+std::to_string(process.creationTime)+'\n'+generation+'\n';
  }
  static std::optional<WindowsEngineRecord> parse(std::string_view text){
    if(text.empty()||text.size()>224)return std::nullopt;
    auto line=[&text]()->std::optional<std::string_view>{
      auto n=text.find('\n');if(n==std::string_view::npos)return std::nullopt;
      auto s=text.substr(0,n);text.remove_prefix(n+1);
      if(!s.empty()&&s.back()=='\r')s.remove_suffix(1);
      return s.empty()?std::nullopt:std::optional(s);
    };
    auto tag=line(),pid=line(),time=line(),id=line();
    if(!tag||*tag!=platform||!pid||!time||!id||!text.empty())return std::nullopt;
    auto r=WindowsRuntimeRecord::parse(std::string(*pid)+'\n'+std::string(WindowsRuntimeRecord::platform)+'\n'+std::string(*time)+'\n');
    if(!r)return std::nullopt;
    WindowsEngineRecord record{*r,std::string(*id)};
    return record.valid()?std::optional(record):std::nullopt;
  }
  std::string completion(std::uint32_t code)const{return encode()+std::to_string(code)+'\n';}
  static std::optional<std::uint32_t> completionCode(std::string_view text,const WindowsEngineRecord& expected){
    if(text.empty()||text.size()>240||text.back()!='\n')return std::nullopt;
    text.remove_suffix(1);auto n=text.rfind('\n');if(n==std::string_view::npos)return std::nullopt;
    auto code=text.substr(n+1);if(!code.empty()&&code.back()=='\r')code.remove_suffix(1);
    auto record=parse(text.substr(0,n+1));
    if(!record||*record!=expected||code.empty())return std::nullopt;
    std::uint32_t value=0;auto p=std::from_chars(code.data(),code.data()+code.size(),value);
    if(p.ec!=std::errc{}||p.ptr!=code.data()+code.size())return std::nullopt;
    return value;
  }
};
}
