// GPL-3.0-only. Public-target exact-range engine; experimental, not registered for production.
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <thread>
#include <fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>
#include "SECP256k1.h"
#include "TargetInput.hpp"
#include "GPU/GPUEngine.h"
#include <cuda_runtime.h>
#include "client/openpuzzle/runtime/VanityRangeProtocol.hpp"

using namespace openpuzzle::vanityPreview;
static volatile sig_atomic_t stopping=0;
static void stop(int) { stopping=1; }
static Int number(const std::string& s) { std::string v=canonical(s);Int n;n.SetBase16(&v[0]);return n; }
static std::string hex(const unsigned char* p,size_t n) {
  std::ostringstream s;for(size_t i=0;i<n;++i)s<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(p[i]);return s.str();
}
static void writeAll(int fd,const std::string& s) {
  size_t n=0;while(n<s.size()) {ssize_t k=write(fd,s.data()+n,s.size()-n);if(k<0&&errno==EINTR)continue;if(k<=0)throw std::runtime_error("Solution write failed");n+=k;}
  if(fsync(fd))throw std::runtime_error("Solution fsync failed");
}
int main(int argc,char** argv) {
  int solutionFd=-1;
  try {
    if(argc==2 && std::string(argv[1])=="--identity") {
      std::cout<<"OpenPuzzle-VanityRange 0.2.0 CUDA target protocol 1\n";return 0;
    }
    if(argc==3 && std::string(argv[1])=="--target-info") {
      auto targets=opvTargets::load(argv[2]);
      for(const auto& pair:targets)std::cout<<pair.first<<" "<<pair.second.address<<"\n";return 0;
    }
    Secp256K1 secp;secp.Init();
    if(argc==3 && std::string(argv[1])=="--reference") {
      Int k=number(argv[2]),one(uint64_t(1)),last=number("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364140");
      if(k.IsLower(&one)||k.IsGreater(&last))throw std::runtime_error("Invalid reference scalar");
      Point p=secp.ComputePublicKey(&k);unsigned char h[20];secp.GetHash160(P2PKH,true,p,h);
      std::cout<<hex(h,20)<<" "<<secp.GetAddress(P2PKH,true,p)<<"\n";return 0;
    }
    // Search accepts public targets only; private fixture scalars never enter this route.
    if(argc!=9 || std::string(argv[1])!="--target-file")
      throw std::runtime_error("Usage: --target-file START COUNT GRID TARGETS RESULTS exhaust|stop OWNER_PID");
    Plan plan(argv[2],decimal(argv[3]),decimal(argv[4]));
    std::string mode=argv[7];if(mode!="exhaust" && mode!="stop")throw std::runtime_error("Invalid target mode");
    uint64_t owner=decimal(argv[8]);if(owner<2 || owner>uint64_t(INT32_MAX))throw std::runtime_error("Invalid owner PID");
    signal(SIGTERM,stop);signal(SIGINT,stop);
    if(prctl(PR_SET_PDEATHSIG,SIGTERM)!=0 || getppid()!=pid_t(owner))throw std::runtime_error("Owner changed");
    int devices=0;if(cudaGetDeviceCount(&devices)!=cudaSuccess || devices!=1)throw std::runtime_error("Exactly one visible CUDA GPU required");
    auto targets=opvTargets::load(argv[5]);
    std::map<uint16_t,std::set<uint32_t>> buckets;
    for(const auto& pair:targets) {
      const auto& h=pair.second.hash;uint16_t a;uint32_t b;
      memcpy(&a,h.data(),2);memcpy(&b,h.data(),4);buckets[a].insert(b);
    }
    GPUEngine gpu(int(plan.blocks),256,0,4096);
    if(!gpu.Ready() || gpu.GetGroupSize()!=1024 || gpu.GetNumThreadsGPU()!=int(plan.blocks*256))throw std::runtime_error("GPU geometry mismatch");
    std::vector<LADDRESS> lookup;uint32_t unique=0;
    for(const auto& pair:buckets) {LADDRESS e;e.sAddress=pair.first;e.lAddresses.assign(pair.second.begin(),pair.second.end());unique+=e.lAddresses.size();lookup.push_back(e);}
    gpu.SetSearchMode(SEARCH_COMPRESSED);gpu.SetSearchType(P2PKH);gpu.SetAddress(lookup,unique);
    Int strideScalar(plan.batch);Point stride=secp.ComputePublicKey(&strideScalar);
    bool trace=plan.count<=65536;gpu.SetExactRange(plan.count,stride,trace);
    uint64_t threads=plan.blocks*256;
    std::vector<Point> points(threads);std::vector<std::thread> workers;
    unsigned parallel=std::min(8u,std::max(1u,std::thread::hardware_concurrency()));
    for(unsigned w=0;w<parallel;++w)workers.emplace_back([&,w]{
      for(uint64_t tid=w;tid<threads;tid+=parallel) {Int k=number(add(plan.start,tid*1024+512));points[tid]=secp.ComputePublicKey(&k);}
    });
    for(auto& t:workers)t.join();
    solutionFd=open(argv[6],O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(solutionFd<0)throw std::runtime_error("Occupied/unsafe solution file");
    std::cout<<"OPV1 BEGIN "<<plan.start<<" "<<plan.count<<" "<<plan.batch<<"\n"<<std::flush;
    if(stopping) {std::cout<<"OPV1 STOPPED 0\n"<<std::flush;close(solutionFd);return 130;}
    if(!gpu.SetKeys(points.data()))throw std::runtime_error("Kernel start failed");
    std::vector<ITEM> hits;std::set<std::string> recovered;uint64_t checked=0;
    while(checked<plan.count) {
      if(!gpu.Launch(hits,true,false))throw std::runtime_error("GPU result or overflow failure");
      bool matched=false;
      for(const ITEM& hit:hits) {
        if(hit.thId>=threads || hit.incr<0 || hit.incr>=1024 || hit.endo || !hit.mode)throw std::runtime_error("Invalid result coordinates");
        uint64_t offset=checked+uint64_t(hit.thId)*1024+hit.incr;
        if(offset>=plan.count)throw std::runtime_error("Result outside inclusive interval");
        std::string h=hex(hit.hash,20);if(!targets.count(h))continue;
        std::string key=add(plan.start,offset);Int k=number(key);Point p=secp.ComputePublicKey(&k);unsigned char reference[20];secp.GetHash160(P2PKH,true,p,reference);
        if(hex(reference,20)!=h || !recovered.insert(key).second)throw std::runtime_error("CPU verification or duplicate result failure");
        writeAll(solutionFd,"MATCH "+key+" "+h+" "+targets.at(h).address+" "+secp.GetPrivAddress(true,k)+"\n");matched=true;
      }
      checked+=std::min(plan.batch,plan.count-checked);
      // Only acknowledge a completed batch after its solutions are durable.
      gpu.VerifyTrace(checked);
      std::cout<<"OPV1 PROGRESS "<<checked<<"\n"<<std::flush;
      if(matched && mode=="stop") {
        std::cout<<"OPV1 SOLUTION "<<checked<<"\n"<<std::flush;close(solutionFd);return 10;
      }
      if(stopping) {std::cout<<"OPV1 STOPPED "<<checked<<"\n"<<std::flush;close(solutionFd);return 130;}
      if(checked<plan.count && !gpu.QueueNext())throw std::runtime_error("Next kernel failed");
    }
    if(cudaDeviceSynchronize()!=cudaSuccess)throw std::runtime_error("Final GPU synchronization failed");
    if(trace)std::cout<<"OPV1 TRACE "<<checked<<"\n";
    std::cout<<"OPV1 COMPLETE "<<checked<<"\n"<<std::flush;close(solutionFd);return 0;
  } catch(const std::exception& e) {
    if(solutionFd>=0)close(solutionFd);
    std::cerr<<"FAILED: "<<e.what()<<"\n";return 2;
  }
}
