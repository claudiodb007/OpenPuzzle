#include "openpuzzle/client/HttpRequestPolicy.hpp"
#include "openpuzzle/core/JsonString.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace openpuzzle::client;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
  auto e=HttpRequestPolicy::endpoint("https://claudiodb.com/api/range/claim");
  require(e&&e->secure&&e->port==443&&e->host=="claudiodb.com"&&e->target=="/api/range/claim","HTTPS endpoint parsed incorrectly");
  e=HttpRequestPolicy::endpoint("http://127.0.0.1:1234?x=a%20b");
  require(e&&e->loopback()&&e->port==1234&&e->target=="/?x=a%20b","Query or local port lost");
  e=HttpRequestPolicy::endpoint("http://[::1]:65535/");require(e&&e->loopback()&&e->port==65535,"IPv6 local endpoint invalid");
  for(const auto& url:{"","ftp://host/a","file:///a","http://","http://user:pw@host/a","http://host:0/a","http://host:65536/a","http://host:+80/a","http://host:80x/a","http://host:/a","http://host\\x/a","http://host/a#fragment","http://host/a b","http://host/\r\nHeader: bad","http://host/%xy","http://host/%","http://[::1]x/a","http://host:80:90/a"})require(!HttpRequestPolicy::endpoint(url),"Invalid URL accepted");
  require(!HttpRequestPolicy::endpoint(std::string("http://host/a\0b",15)),"NUL URL accepted");
  require(!HttpRequestPolicy::endpoint("http://host/"+std::string(HttpRequestPolicy::maxUrl,'x')),"Oversize URL accepted");
  require(HttpRequestPolicy::validSpeed(0)&&HttpRequestPolicy::validSpeed(1234.5),"Valid speed rejected");
  require(!HttpRequestPolicy::validSpeed(-1)&&!HttpRequestPolicy::validSpeed(std::numeric_limits<double>::infinity())&&!HttpRequestPolicy::validSpeed(std::numeric_limits<double>::quiet_NaN()),"Nonfinite speed accepted");
  require(openpuzzle::escapeJsonString(std::string("a\0b\b\f\x01",7))=="a\\u0000b\\b\\f\\u0001\\u0000","Control-byte encoding corrupted");
  std::cout<<"HttpRequestPolicyTests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
