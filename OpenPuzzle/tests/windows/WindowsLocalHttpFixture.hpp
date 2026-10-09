#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include "openpuzzle/client/HttpRequestPolicy.hpp"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
using namespace openpuzzle::client;
void require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
struct Winsock{Winsock(){WSADATA d{};require(WSAStartup(MAKEWORD(2,2),&d)==0,"Winsock init failed");}~Winsock(){WSACleanup();}};
struct Socket{SOCKET s=INVALID_SOCKET;explicit Socket(SOCKET v):s(v){}~Socket(){if(s!=INVALID_SOCKET)closesocket(s);}Socket(const Socket&)=delete;};
std::string lower(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return s;}
class Server{
 Socket listener{socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)};
 std::thread worker;
 std::exception_ptr failure;
 unsigned short port=0;
 std::string response;
 unsigned delay,rounds;
 bool brokenTls,canonical;
 void serve(){try{
  for(unsigned round=0;round<(canonical?rounds:1u);++round){
   request.clear();
   fd_set fds;FD_ZERO(&fds);FD_SET(listener.s,&fds);timeval t{5,0};
   require(select(0,&fds,nullptr,nullptr,&t)==1,"Local fixture accept timed out");
   Socket peer(accept(listener.s,nullptr,nullptr));require(peer.s!=INVALID_SOCKET,"Fixture accept failed");
   DWORD timeout=2000;setsockopt(peer.s,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
   setsockopt(peer.s,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
   char buf[4096];
   if(brokenTls){int n=recv(peer.s,buf,sizeof(buf),0);require(n>0&&static_cast<unsigned char>(buf[0])==22,"TLS handshake was not attempted");return;}
   std::size_t expected=0;
   for(;;){int n=recv(peer.s,buf,sizeof(buf),0);require(n>0,"Local request incomplete");request.append(buf,n);
     auto h=request.find("\r\n\r\n");if(h==std::string::npos)continue;
     if(!expected){auto header=lower(request.substr(0,h));auto at=header.find("content-length:");
       std::size_t length=at==std::string::npos?0:std::stoul(header.substr(at+15));expected=h+4+length;}
     if(request.size()>=expected)break;
     require(request.size()<HttpRequestPolicy::maxRequest+16384,"Fixture request too large");
   }
   requests.push_back(request);
   if(delay)Sleep(delay);
   const auto reply=canonical&&round==0?std::string("HTTP/1.1 302 Found\r\nContent-Length: 0\r\nConnection: close\r\nLocation: /canonical\r\n\r\n"):response;
   std::size_t sent=0;
   while(sent<reply.size()){
     int n=send(peer.s,reply.data()+sent,static_cast<int>(std::min<std::size_t>(8192,reply.size()-sent)),0);
     if(n<=0)break;
     sent+=n;
   }
  }
 }catch(...){failure=std::current_exception();}}
public:
 std::string request;
 std::vector<std::string> requests;
 Server(std::string body="{}",std::string status="200 OK",std::string headers={},unsigned wait=0,bool tls=false,bool raw=false,bool redirect=false,unsigned hops=2):response(raw?body:"HTTP/1.1 "+status+"\r\nContent-Type: application/json\r\nContent-Length: "+std::to_string(body.size())+"\r\nConnection: close\r\n"+headers+"\r\n"+body),delay(wait),rounds(hops),brokenTls(tls),canonical(redirect){
   require(listener.s!=INVALID_SOCKET,"Fixture socket failed");
   sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
   require(bind(listener.s,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0&&listen(listener.s,4)==0,"Fixture bind failed");
   int size=sizeof(address);require(getsockname(listener.s,reinterpret_cast<sockaddr*>(&address),&size)==0,"Fixture port failed");port=ntohs(address.sin_port);
   worker=std::thread([this]{serve();});
 }
 ~Server(){if(worker.joinable()){shutdown(listener.s,SD_BOTH);worker.join();}}
 void finish(){if(worker.joinable())worker.join();if(failure)std::rethrow_exception(failure);}
 std::string url()const{return "http://127.0.0.1:"+std::to_string(port);}
 std::string body(){finish();auto h=request.find("\r\n\r\n");return h==std::string::npos?std::string{}:request.substr(h+4);}
 void path(const std::string& method,const std::string& target){finish();require(request.starts_with(method+' '+target+" HTTP/1.1\r\n"),"Method or endpoint changed");}
};
