#include "GpuControlInternal.hpp"
#include <cstdio>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace openpuzzle {
std::string controlPci(unsigned domain,unsigned bus,unsigned device,unsigned function){
 if(bus>255||device>31||function>7)return {};char text[32]{};std::snprintf(text,sizeof text,"%08x:%02x:%02x.%x",domain,bus,device,function);return text;
}
namespace {
class Combined final:public GpuControlBackend {
 std::unique_ptr<GpuControlBackend> nvidia_=makeNvmlControl(),amd_=makeAmdControl();
public:
 std::vector<GpuControlDevice> inspect()override{
  auto n=nvidia_->inspect(),a=amd_->inspect();n.insert(n.end(),a.begin(),a.end());
  for(auto& d:n)for(const auto& other:n)if(&d!=&other&&d.pci==other.pci){d.power=d.fan=d.automatic=false;d.detail="Ambiguous physical identity";}
  return n;
 }
 void apply(const GpuControlDevice& d,const GpuControlRequest& r)override{(d.vendor=="nvidia"?nvidia_:amd_)->apply(d,r);}
};
class WriteLock {
#ifdef _WIN32
 HANDLE h_=nullptr;
public:
 WriteLock(){h_=CreateMutexW(nullptr,FALSE,L"Global\\OpenPuzzleGpuControl-v1");if(!h_)throw std::runtime_error("GPU control lock unavailable");const auto r=WaitForSingleObject(h_,0);if(r!=WAIT_OBJECT_0&&r!=WAIT_ABANDONED){CloseHandle(h_);h_=nullptr;throw std::runtime_error("Another GPU control operation is active");}}
 ~WriteLock(){if(h_){ReleaseMutex(h_);CloseHandle(h_);}}
#else
 int fd_=-1;
public:
 WriteLock(){
  if(geteuid()!=0)throw std::runtime_error("GPU controls require root permission; use sudo openpuzzle gpu-control apply with explicit GPU and value");
  fd_=open("/run/lock/openpuzzle-gpu-control.lock",O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
  if(fd_<0)throw std::runtime_error("GPU control lock unavailable");struct stat status{};
  if(fstat(fd_,&status)||!S_ISREG(status.st_mode)||status.st_uid!=0||(status.st_mode&0077)){close(fd_);fd_=-1;throw std::runtime_error("GPU control lock ownership or permissions invalid");}
  if(flock(fd_,LOCK_EX|LOCK_NB)){close(fd_);fd_=-1;throw std::runtime_error("Another GPU control operation is active");}
 }
 ~WriteLock(){if(fd_>=0)close(fd_);}
#endif
};
}
std::vector<GpuControlDevice> inspectGpuControls(){return Combined().inspect();}
void applyGpuControl(const GpuControlRequest& request){WriteLock lock;Combined backend;applyGpuControlChecked(backend,request);}
}
