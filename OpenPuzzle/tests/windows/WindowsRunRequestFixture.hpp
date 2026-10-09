#pragma once
#include "WindowsLocalHttpFixture.hpp"
#include <atomic>
#include <functional>
class RunServer {
  Socket listener{socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)};
  std::thread worker;std::atomic_bool closing{false};std::exception_ptr failure;
  unsigned short port=0;std::function<std::string(const std::string&)> respond,status;
  void serve(){try{while(!closing){
    fd_set fds;FD_ZERO(&fds);FD_SET(listener.s,&fds);timeval wait{0,200000};
    auto ready=select(0,&fds,nullptr,nullptr,&wait);if(closing)break;
    require(ready>=0,"Session fixture select failed");if(!ready)continue;
    Socket peer(accept(listener.s,nullptr,nullptr));require(peer.s!=INVALID_SOCKET,"Session fixture accept failed");
    DWORD timeout=2000;setsockopt(peer.s,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
    std::string request;char buffer[4096];std::size_t length=0;
    for(;;){int n=recv(peer.s,buffer,sizeof(buffer),0);require(n>0,"Session request incomplete");request.append(buffer,n);
      auto at=request.find("\r\n\r\n");if(at==std::string::npos)continue;
      if(!length){const auto header=lower(request.substr(0,at));auto size=header.find("content-length:");length=at+4+(size==std::string::npos?0:std::stoul(header.substr(size+15)));}
      if(request.size()>=length)break;require(request.size()<65536,"Session fixture request exceeded bound");
    }
    requests.push_back(request);auto body=respond(request);const auto reply="HTTP/1.1 "+(status?status(request):std::string("200 OK"))+"\r\nContent-Type: application/json\r\nContent-Length: "+std::to_string(body.size())+"\r\nConnection: close\r\n\r\n"+body;
    std::size_t sent=0;while(sent<reply.size()){int n=send(peer.s,reply.data()+sent,static_cast<int>(reply.size()-sent),0);require(n>0,"Session fixture reply failed");sent+=n;}
  }}catch(...){failure=std::current_exception();}}
public:
  std::vector<std::string> requests;
  explicit RunServer(std::function<std::string(const std::string&)> callback,
      std::function<std::string(const std::string&)> statusCallback={}):respond(std::move(callback)),status(std::move(statusCallback)){
    require(listener.s!=INVALID_SOCKET,"Session socket failed");sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    require(bind(listener.s,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0&&listen(listener.s,4)==0,"Session bind failed");
    int size=sizeof(address);require(getsockname(listener.s,reinterpret_cast<sockaddr*>(&address),&size)==0,"Session port failed");port=ntohs(address.sin_port);worker=std::thread([this]{serve();});
  }
  ~RunServer(){closing=true;if(worker.joinable())worker.join();}
  void finish(){closing=true;if(worker.joinable())worker.join();if(failure)std::rethrow_exception(failure);}
  std::string url()const{return "http://127.0.0.1:"+std::to_string(port);}
};
