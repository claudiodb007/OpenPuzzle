#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include "openpuzzle/client/WindowsHttpTransport.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
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
  for(int round=0;round<(canonical?rounds:1);++round){
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
struct Comma:std::numpunct<char>{char do_decimal_point()const override{return ',';}};
struct LocaleGuard{std::locale old=std::locale();LocaleGuard(){std::locale::global(std::locale(std::locale::classic(),new Comma));}~LocaleGuard(){std::locale::global(old);}};
int main(){try{
 Winsock sockets;
 {Server s;auto r=WindowsHttpTransport::request("POST",s.url()+"/a%20b?x=y","{\"word\":\"ç 中文 %PATH% & >\"}");require(r.success()&&r.status==200&&r.body=="{}","POST result failed");s.path("POST","/a%20b?x=y");require(s.body()=="{\"word\":\"ç 中文 %PATH% & >\"}","UTF-8 payload changed");require(lower(s.request).find("content-type: application/json; charset=utf-8")!=std::string::npos,"JSON header missing");}
 {Server s("","204 No Content");auto r=WindowsHttpTransport::request("HEAD",s.url()+"/probe");require(r.success()&&r.body.empty(),"HEAD failed");s.path("HEAD","/probe");require(s.body().empty(),"HEAD sent body");}
 {const std::string binary("a\0bcd",5);Server s(binary);auto r=WindowsHttpTransport::request("GET",s.url());require(r.success()&&r.body==binary,"Binary response truncated");s.finish();}
 {std::string raw="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n3\r\n"+std::string("a\0b",3)+"\r\n2\r\ncd\r\n0\r\n\r\n";Server s(raw,"","",0,false,true);auto r=WindowsHttpTransport::request("GET",s.url());require(r.success()&&r.body==std::string("a\0bcd",5),"Chunked response damaged");s.finish();}
 {Server s(std::string(HttpRequestPolicy::maxResponse,'x'));auto r=WindowsHttpTransport::request("GET",s.url());require(r.success()&&r.body.size()==HttpRequestPolicy::maxResponse,"Exact response limit rejected");s.finish();}
 {Server s(std::string(HttpRequestPolicy::maxResponse+1,'x'));auto r=WindowsHttpTransport::request("GET",s.url());require(!r.success()&&r.body.empty()&&r.error=="HTTP response exceeds byte limit","Oversized body accepted");s.finish();}
 {Server s("HTTP/1.1 200 OK\r\nContent-Length: 100\r\nConnection: close\r\n\r\nshort","","",0,false,true);auto r=WindowsHttpTransport::request("GET",s.url());require(!r.success()&&r.body.empty(),"Truncated body reported success");s.finish();}
 {Server s("{}","302 Found","Location: http://127.0.0.1:9/must-not-follow\r\n");auto r=WindowsHttpTransport::request("POST",s.url(),"{}");require(!r.success()&&r.status==302&&r.body=="{}","Redirect automatically followed or replayed");s.finish();}
 {Server s("{}","200 OK","",0,false,false,true);auto r=WindowsHttpTransport::request("POST",s.url()+"/old","{\"fixture\":1}");require(r.success(),"Same-origin canonical redirect failed");s.path("POST","/canonical");require(s.requests.size()==2&&s.requests[0].starts_with("POST /old ")&&s.body()=="{\"fixture\":1}","Canonical redirect lost POST bytes");}
 {Server s("{}","302 Found","Location: /canonical\r\n",0,false,false,true,4);auto r=WindowsHttpTransport::request("POST",s.url()+"/loop","{}");require(!r.success()&&r.error=="HTTP redirect limit exceeded","Redirect loop was not bounded");s.finish();require(s.requests.size()==4,"Unexpected redirect attempts");}
 {Server s("{}","401 Unauthorized","WWW-Authenticate: Basic realm=\"fixture\"\r\n");auto r=WindowsHttpTransport::request("POST",s.url(),"{}");require(!r.success()&&r.status==401,"Authentication challenge retried");s.finish();}
 {Server s("{}","200 OK","",700);auto began=GetTickCount64();auto r=WindowsHttpTransport::request("GET",s.url(),{},100);require(!r.success()&&!r.error.empty()&&GetTickCount64()-began<2000,"Stalled response timeout failed");s.finish();}
 {Server s("","","",0,true);auto r=WindowsHttpTransport::request("GET","https"+s.url().substr(4),{},1000);require(!r.success()&&!r.error.empty(),"Broken TLS accepted");s.finish();}
 for(const auto& method:{"DELETE","POST\r\nInjected: x"})require(!WindowsHttpTransport::request(method,"http://127.0.0.1:9").success(),"Unsupported method accepted");
 require(WindowsHttpTransport::request("POST","http://127.0.0.1:9",std::string(HttpRequestPolicy::maxRequest+1,'a')).error=="Invalid HTTP request parameters","Oversized request not rejected before connection");
 require(WindowsHttpTransport::request("POST","http://127.0.0.1:9",std::string("\xff",1)).error=="Invalid HTTP request parameters","Invalid UTF-8 sent");
 require(!WindowsHttpTransport::request("GET","http://user:secret@127.0.0.1:9/").success(),"URL credentials accepted");
 // Exercise the real client's six endpoints against only local fixtures.
 {Server s;HttpRangeClient client(s.url());require(client.registerClient({"client ç & % \n\x01"}),"Native client registration failed");s.path("POST","/api/client/register");require(s.body()=="{\"client_id\":\"client ç & % \\n\\u0001\"}","Client control characters not escaped");}
 {LocaleGuard locale;Server s;HttpRangeClient client(s.url());ClientHeartbeat h;h.clientId="fixture-client";h.version="1.0.46";h.platform="Windows";h.cpu={"fixture cpu",1,1};h.gpuTelemetryReported=true;ClientGpuTelemetry t;t.vendor="fixture";t.deviceId="gpu-0";t.device=0;t.temperatureC=51.5;h.gpuTelemetry.push_back(t);require(client.heartbeat(h),"Native heartbeat failed");s.path("POST","/api/client/heartbeat");require(s.body().find("\"temperature_c\":51.5")!=std::string::npos,"Heartbeat used locale comma");}
 {LocaleGuard locale;Server s(R"({"assignment_id":"fixture-only","puzzle":20,"range_id":1,"target":"fixture","start":"1","end":"F"})");require(SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT","cuda-5"),"Fixture slot failed");HttpRangeClient client(s.url());auto a=client.claimResult("fixture-client",20,1,12.5,"cuda");require(a.assigned()&&a.assignment->assignmentId=="fixture-only","Native assignment parsing failed");s.path("POST","/api/range/claim");require(s.body().find("\"speed_mkeys\":12.500000")!=std::string::npos&&s.body().find("\"execution_slot\":\"cuda-5\"")!=std::string::npos,"Claim numeric formatting or dynamic slot lost");}
 {Server s(R"({"available":false,"message":"fixture unavailable"})");HttpRangeClient client(s.url());require(client.claimResult("fixture",20,1).unavailable(),"No-work response became failure");s.finish();}
 {LocaleGuard locale;Server s;HttpRangeClient client(s.url());require(client.progress("fixture-assignment","fixture-client",12.5,"100"),"Native progress failed");s.path("POST","/api/range/progress");require(s.body().find("\"speed_mkeys\":12.5")!=std::string::npos,"Progress used locale comma");}
 {Server s;HttpRangeClient client(s.url());require(client.complete("fixture-assignment","fixture-client",0,"completed","100"),"Native completion failed");s.path("POST","/api/range/complete");}
 {Server s;HttpRangeClient client(s.url());require(client.reportSolution("fixture-assignment","fixture-client"),"Native solution signal failed");s.path("POST","/api/puzzle/report-solution");require(s.body()=="{\"assignment_id\":\"fixture-assignment\",\"client_id\":\"fixture-client\"}","Solution signal disclosed extra data");}
 {Server s(R"({"error":"assignment_lease_expired","message":"fixture expired"})","409 Conflict");HttpRangeClient client(s.url());require(!client.progress("fixture-assignment","fixture-client",1,"100")&&client.lastErrorCode()=="assignment_lease_expired","Server error code discarded");s.finish();require(!client.claim("fixture-client",20,1,std::numeric_limits<double>::infinity())&&client.lastErrorCode().empty(),"Stale HTTP code or nonfinite claim retained");}
 for(double n:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
  HttpRangeClient client("file:///unused");require(!client.progress("fixture","fixture",n,"100")&&client.lastError().find("finite")!=std::string::npos,"Nonfinite progress sent");}
 std::cout<<"WindowsHttpTransportTests passed: local WinHTTP requests and six client endpoints, literal JSON, limits, redirects, failures and timeout\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
