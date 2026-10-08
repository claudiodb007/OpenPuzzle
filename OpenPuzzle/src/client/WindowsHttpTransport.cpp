#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include "openpuzzle/client/WindowsHttpTransport.hpp"
#include <algorithm>
#include <array>
#include <optional>
namespace openpuzzle::client {
namespace {
class InternetHandle {
  HINTERNET value_;
public:
  explicit InternetHandle(HINTERNET value):value_(value){}
  ~InternetHandle(){if(value_)WinHttpCloseHandle(value_);}
  InternetHandle(const InternetHandle&)=delete;
  HINTERNET get()const{return value_;}
  explicit operator bool()const{return value_!=nullptr;}
};
std::wstring ascii(const std::string& value){return {value.begin(),value.end()};}
bool utf8(const std::string& text){
  if(text.empty())return true;
  return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0)>0;
}
HttpResponse failure(HttpResponse r,const char* stage){
  r.error=std::string("WinHTTP ")+stage+" failed (code "+std::to_string(GetLastError())+")";
  r.body.clear();return r;
}
}
static HttpResponse requestOnce(const std::string& method,const std::string& url,
    const std::string& body,std::uint32_t timeoutMilliseconds){
  HttpResponse r;auto endpoint=HttpRequestPolicy::endpoint(url);
  if(!endpoint){r.error="Invalid HTTP server URL";return r;}
  if((method!="POST"&&method!="GET"&&method!="HEAD")||
      (method!="POST"&&!body.empty())||body.size()>HttpRequestPolicy::maxRequest||!utf8(body)||
      timeoutMilliseconds==0||timeoutMilliseconds>120000){r.error="Invalid HTTP request parameters";return r;}
  const auto started=GetTickCount64();
  InternetHandle session(WinHttpOpen(L"OpenPuzzle/1.0.46",
      endpoint->loopback()?WINHTTP_ACCESS_TYPE_NO_PROXY:WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
  if(!session)return failure(std::move(r),"session");
  auto timeout=static_cast<int>(timeoutMilliseconds);
  if(!WinHttpSetTimeouts(session.get(),std::min(timeout,10000),std::min(timeout,10000),timeout,timeout))return failure(std::move(r),"timeouts");
  DWORD protocols=WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
  if(!WinHttpSetOption(session.get(),WINHTTP_OPTION_SECURE_PROTOCOLS,&protocols,sizeof(protocols)))return failure(std::move(r),"TLS policy");
  const auto host=ascii(endpoint->host);
  InternetHandle connection(WinHttpConnect(session.get(),host.c_str(),endpoint->port,0));
  if(!connection)return failure(std::move(r),"connect");
  const auto verb=ascii(method),target=ascii(endpoint->target);
  InternetHandle request(WinHttpOpenRequest(connection.get(),verb.c_str(),target.c_str(),nullptr,
      WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,endpoint->secure?WINHTTP_FLAG_SECURE:0));
  if(!request)return failure(std::move(r),"request");
  DWORD disable=WINHTTP_DISABLE_REDIRECTS|WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION;
  DWORD retries=1; // One connection attempt, not zero attempts.
  if(!WinHttpSetOption(request.get(),WINHTTP_OPTION_DISABLE_FEATURE,&disable,sizeof(disable))||
      !WinHttpSetOption(request.get(),WINHTTP_OPTION_CONNECT_RETRIES,&retries,sizeof(retries)))return failure(std::move(r),"request policy");
  const wchar_t* headers=method=="POST"?L"Content-Type: application/json; charset=utf-8\r\nAccept: application/json\r\n":L"Accept: application/json\r\n";
  if(!WinHttpSendRequest(request.get(),headers,static_cast<DWORD>(-1),
      body.empty()?WINHTTP_NO_REQUEST_DATA:const_cast<char*>(body.data()),
      static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),0))return failure(std::move(r),"send");
  if(!WinHttpReceiveResponse(request.get(),nullptr))return failure(std::move(r),"response");
  DWORD status=0,size=sizeof(status);
  if(!WinHttpQueryHeaders(request.get(),WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
      WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))return failure(std::move(r),"status");
  r.status=status;
  if(status==301||status==302||status==307||status==308){
    DWORD bytes=0;
    WinHttpQueryHeaders(request.get(),WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,nullptr,&bytes,WINHTTP_NO_HEADER_INDEX);
    if(GetLastError()==ERROR_INSUFFICIENT_BUFFER&&bytes>0&&bytes<=(HttpRequestPolicy::maxUrl+1)*sizeof(wchar_t)){
      std::wstring location(bytes/sizeof(wchar_t),L'\0');
      if(!WinHttpQueryHeaders(request.get(),WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location.data(),&bytes,WINHTTP_NO_HEADER_INDEX))return failure(std::move(r),"redirect header");
      while(!location.empty()&&location.back()==L'\0')location.pop_back();
      for(wchar_t c:location){if(c<33||c>=127){r.error="Invalid HTTP redirect";return r;}r.redirectLocation+=static_cast<char>(c);}
    }
  }
  if(method!="HEAD"){
    std::optional<DWORD> expectedLength;
    DWORD length=0,lengthSize=sizeof(length);
    if(WinHttpQueryHeaders(request.get(),WINHTTP_QUERY_CONTENT_LENGTH|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,&length,&lengthSize,WINHTTP_NO_HEADER_INDEX)){
      if(length>HttpRequestPolicy::maxResponse){r.body.clear();r.error="HTTP response exceeds byte limit";return r;}
      expectedLength=length;
    }else if(GetLastError()!=ERROR_WINHTTP_HEADER_NOT_FOUND){return failure(std::move(r),"content length");}
    std::array<char,8192> buffer{};
    for(;;){
      auto elapsed=GetTickCount64()-started;
      if(elapsed>=timeoutMilliseconds){r.error="HTTP request deadline exceeded";r.body.clear();return r;}
      const auto remaining=static_cast<int>(timeoutMilliseconds-elapsed);
      if(!WinHttpSetTimeouts(request.get(),remaining,remaining,remaining,remaining))return failure(std::move(r),"read timeout");
      DWORD read=0;
      if(!WinHttpReadData(request.get(),buffer.data(),static_cast<DWORD>(buffer.size()),&read))return failure(std::move(r),"read");
      if(read==0)break;
      if(r.body.size()+read>HttpRequestPolicy::maxResponse){r.body.clear();r.error="HTTP response exceeds byte limit";return r;}
      r.body.append(buffer.data(),read);
    }
    if(expectedLength&&r.body.size()!=*expectedLength){r.body.clear();r.error="HTTP response was truncated";return r;}

  }
  if(status<200||status>=300)r.error="HTTP status "+std::to_string(status);
  return r;
}
HttpResponse WindowsHttpTransport::request(const std::string& method,const std::string& url,
    const std::string& body,std::uint32_t timeoutMilliseconds){
  if(timeoutMilliseconds==0||timeoutMilliseconds>120000){HttpResponse r;r.error="Invalid HTTP request parameters";return r;}
  const auto origin=HttpRequestPolicy::endpoint(url);
  if(!origin){HttpResponse r;r.error="Invalid HTTP server URL";return r;}
  std::string current=url;
  const auto began=GetTickCount64();
  for(unsigned redirects=0;redirects<=3;++redirects){
    auto elapsed=GetTickCount64()-began;
    if(elapsed>=timeoutMilliseconds){HttpResponse r;r.error="HTTP request deadline exceeded";return r;}
    auto r=requestOnce(method,current,body,static_cast<std::uint32_t>(timeoutMilliseconds-elapsed));
    if(r.redirectLocation.empty()||(r.status!=301&&r.status!=302&&r.status!=307&&r.status!=308))return r;
    if(redirects==3){r.error="HTTP redirect limit exceeded";return r;}
    std::string next;
    if(r.redirectLocation.find("://")!=std::string::npos)next=r.redirectLocation;
    else {
      const auto authorityStart=current.find("://")+3;
      const auto authorityEnd=current.find_first_of("/?",authorityStart);
      const auto base=current.substr(0,authorityEnd);
      if(r.redirectLocation.front()=='/')next=base+r.redirectLocation;
      else {
        const auto e=HttpRequestPolicy::endpoint(current);
        const auto path=e->target.substr(0,e->target.find('?'));
        next=base+path.substr(0,path.rfind('/')+1)+r.redirectLocation;
      }
    }
    auto endpoint=HttpRequestPolicy::endpoint(next);
    auto normalized=[](std::string host){for(char& c:host)if(c>='A'&&c<='Z')c+=32;return host;};
    if(!endpoint||endpoint->secure!=origin->secure||endpoint->port!=origin->port||
        normalized(endpoint->host)!=normalized(origin->host)||r.redirectLocation.starts_with("//")){
      r.error="HTTP redirect leaves configured origin or is invalid";return r;
    }
    current=std::move(next);
  }
  HttpResponse r;r.error="HTTP redirect limit exceeded";return r;
}

}
