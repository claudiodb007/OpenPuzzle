#pragma once
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
namespace openpuzzle::client {
struct HttpEndpoint {
  bool secure=false;
  std::string host;
  unsigned short port=80;
  std::string target;
  bool loopback()const{return host=="127.0.0.1"||host=="::1"||host=="localhost";}
};
struct HttpRequestPolicy {
  static constexpr std::size_t maxUrl=16384,maxRequest=2*1024*1024,maxResponse=256*1024;
  static bool validSpeed(double speed){return std::isfinite(speed)&&speed>=0.0;}
  static std::optional<HttpEndpoint> endpoint(std::string_view url){
    if(url.empty()||url.size()>maxUrl)return {};
    auto hex=[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');};
    for(std::size_t i=0;i<url.size();++i){
      const auto c=static_cast<unsigned char>(url[i]);
      if(c<=32||c>=127||c=='\\'||c=='#')return {};
      if(c=='%'&&(i+2>=url.size()||!hex(url[i+1])||!hex(url[i+2])))return {};
    }
    HttpEndpoint e;
    if(url.starts_with("https://")){e.secure=true;e.port=443;url.remove_prefix(8);}
    else if(url.starts_with("http://"))url.remove_prefix(7);
    else return {};
    const auto end=url.find_first_of("/?");const auto authority=url.substr(0,end);
    if(authority.empty()||authority.find('@')!=std::string_view::npos)return {};
    std::string_view host,port;
    if(authority.front()=='['){
      auto close=authority.find(']');if(close==std::string_view::npos)return {};
      host=authority.substr(1,close-1);
      if(host.empty()||host.find(':')==std::string_view::npos)return {};
      for(char c:host)if(!hex(c)&&c!=':'&&c!='.')return {};
      if(close+1<authority.size()){
        if(authority[close+1]!=':')return {};
        port=authority.substr(close+2);if(port.empty())return {};
      }
    }else{
      auto colon=authority.find(':');host=authority.substr(0,colon);
      if(host.empty()||host.front()=='.'||host.back()=='.')return {};
      for(char c:host)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'))return {};
      if(colon!=std::string_view::npos){port=authority.substr(colon+1);if(port.empty())return {};}
    }
    if(!port.empty()){
      unsigned value=0;auto n=std::from_chars(port.data(),port.data()+port.size(),value);
      if(n.ec!=std::errc{}||n.ptr!=port.data()+port.size()||value==0||value>65535)return {};
      e.port=static_cast<unsigned short>(value);
    }
    e.host=host;e.target=end==std::string_view::npos?"/":std::string(url.substr(end));
    if(e.target.front()=='?')e.target.insert(e.target.begin(),'/');
    return e;
  }
};
}
