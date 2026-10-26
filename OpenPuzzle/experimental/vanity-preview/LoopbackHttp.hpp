// MIT. Deliberately restricted preview transport: no DNS, proxy or redirects.
#pragma once
#include <algorithm>
#include <chrono>
#include <cctype>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <sstream>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
namespace openpuzzle::vanityPreview {
struct LocalHttpResponse { int status;std::string body; };
class LoopbackHttp {
 unsigned short port_;
 public:
 explicit LoopbackHttp(const std::string& url) {
  std::smatch m;
  if(!std::regex_match(url,m,std::regex("http://127\\.0\\.0\\.1:([1-9][0-9]{3,4})")))throw std::runtime_error("Only explicit numeric loopback HTTP ports are allowed");
  auto p=std::stoul(m[1]);if(p<1024||p>65535)throw std::runtime_error("Invalid loopback port");port_=static_cast<unsigned short>(p);
 }
 LocalHttpResponse post(const std::string& path,const std::string& body) const {
  if(path.empty()||path[0]!='/'||path.find_first_not_of("/abcdefghijklmnopqrstuvwxyz-")!=std::string::npos||body.size()>8192)throw std::runtime_error("Invalid local HTTP request");
  struct Socket {int fd=-1;~Socket(){if(fd>=0)close(fd);}} s;
  s.fd=socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);if(s.fd<0)throw std::runtime_error("Loopback socket failed");
  auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
  auto wait=[&](short events){
   while(true){auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();if(ms<=0)throw std::runtime_error("Loopback HTTP deadline exceeded");
    pollfd p{s.fd,events,0};int n=poll(&p,1,int(ms));if(n<0&&errno==EINTR)continue;if(n<=0||p.revents&POLLNVAL)throw std::runtime_error("Loopback HTTP unavailable");return;}
  };
  sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(port_);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(connect(s.fd,reinterpret_cast<sockaddr*>(&address),sizeof address)!=0&&errno!=EINPROGRESS)throw std::runtime_error("Loopback connection refused");
  wait(POLLOUT);int error=0;socklen_t size=sizeof error;if(getsockopt(s.fd,SOL_SOCKET,SO_ERROR,&error,&size)||error)throw std::runtime_error("Loopback connection failed");
  std::string request="POST "+path+" HTTP/1.1\r\nHost: 127.0.0.1:"+std::to_string(port_)+"\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
  size_t sent=0;while(sent<request.size()){wait(POLLOUT);auto n=send(s.fd,request.data()+sent,request.size()-sent,MSG_NOSIGNAL);if(n<0&&(errno==EAGAIN||errno==EINTR))continue;if(n<=0)throw std::runtime_error("Loopback send failed");sent+=size_t(n);}
  std::string raw;char buffer[4096];
  while(true){wait(POLLIN);auto n=recv(s.fd,buffer,sizeof buffer,0);if(n<0&&(errno==EINTR||errno==EAGAIN))continue;if(n<0)throw std::runtime_error("Loopback read failed");if(!n)break;raw.append(buffer,size_t(n));if(raw.size()>73728)throw std::runtime_error("Oversized loopback response");}
  auto boundary=raw.find("\r\n\r\n");if(boundary==std::string::npos||boundary>8192)throw std::runtime_error("Malformed HTTP headers");
  std::istringstream in(raw.substr(0,boundary));std::string line;std::getline(in,line);std::smatch m;
  if(!std::regex_match(line,m,std::regex("HTTP/1\\.[01] ([1-5][0-9]{2}) [^\\r\\n]*\\r")))throw std::runtime_error("Malformed HTTP status");
  int status=std::stoi(m[1]);std::map<std::string,std::string> headers;
  while(std::getline(in,line)){
   if(!line.empty()&&line.back()=='\r')line.pop_back();auto colon=line.find(':');if(colon==std::string::npos)throw std::runtime_error("Malformed HTTP header");
   auto name=line.substr(0,colon);std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});auto value=line.substr(colon+1);
   while(!value.empty()&&value.front()==' ')value.erase(value.begin());
   if(!headers.emplace(name,value).second)throw std::runtime_error("Duplicate HTTP header");
  }
  if(headers.count("transfer-encoding")||!headers.count("content-length"))throw std::runtime_error("Only bounded Content-Length responses are accepted");
  auto length=headers.at("content-length");if(length.empty()||length.size()>5||length.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid Content-Length");
  auto result=raw.substr(boundary+4);if(result.size()!=std::stoul(length)||result.size()>65536)throw std::runtime_error("Truncated/oversized HTTP body");
  if(status>=300&&status<400)throw std::runtime_error("HTTP redirects are forbidden in local preview");
  return {status,result};
 }
};
}
