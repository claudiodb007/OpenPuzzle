#include "openpuzzle/performance/WindowsAutoBenchmark.hpp"
#include <iostream>
#include <limits>
#include <tuple>
using namespace openpuzzle;
void check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
BenchmarkResult stable(double speed=100){BenchmarkResult r;r.success=true;r.averageSpeed=r.speedMKeys=speed;r.minimumSpeed=speed-1;r.maximumSpeed=speed+1;r.samples=8;return r;}
int main(){try{
 for(const bool cuda:{false,true}){
  const auto matrix=AutoTuner{}.defaultMatrix(68,10240,cuda);int candidates=0,finalists=0;
  const auto tuned=runWindowsAutoBenchmark(68,10240,cuda,30,[&](const auto& c,int seconds,bool final)->std::optional<BenchmarkResult>{
   check(AutoTuner::estimatedMemoryMb(c)<=10240*70/100,"Exceeded Linux memory budget");
   check(cuda||c.threads<=256,"OpenCL received CUDA-only thread candidate");
   if(final){++finalists;check(seconds==60,"Finalist duration differs from Linux default");return stable(120);}
   check(seconds==30,"Candidate duration changed");++candidates;
   const auto& expected=matrix.at(candidates-1).configuration;
   check(std::tie(c.blocks,c.threads,c.points)==std::tie(expected.blocks,expected.threads,expected.points),"Matrix differs from Linux");
   if(candidates==1)return {}; // Unsupported candidate is skipped.
   if(candidates==2){auto invalid=stable();invalid.averageSpeed=std::numeric_limits<double>::quiet_NaN();return invalid;}
   if(candidates==3){auto unstable=stable(150);unstable.minimumSpeed=1;unstable.maximumSpeed=400;return unstable;}
   return stable(candidates==4?140:100);
  });
  check(candidates==static_cast<int>(matrix.size())&&finalists==1&&tuned.candidatesTested==candidates,"Incomplete sweep or repeated finalist");
  check(tuned.planningSpeed==120*.97&&tuned.validated.averageSpeed==120,"Final validation speed/margin not used");
  const auto& best=matrix.at(3).configuration;
  check(std::tie(tuned.validated.configuration.blocks,tuned.validated.configuration.threads,tuned.validated.configuration.points)==std::tie(best.blocks,best.threads,best.points),"Unstable peak won over sustained throughput");
 }
 int calls=0;bool refused=false;
 try{runWindowsAutoBenchmark(68,10240,true,30,[&](const auto&,int,bool)->std::optional<BenchmarkResult>{++calls;if(calls==3)throw std::runtime_error("owned cancellation");return stable();});}catch(...){refused=true;}
 check(refused&&calls==3,"Cancellation did not abort before publication");
 refused=false;int finals=0;
 try{runWindowsAutoBenchmark(68,10240,true,30,[&](const auto&,int,bool final)->std::optional<BenchmarkResult>{if(final){++finals;return {};}return stable();});}catch(...){refused=true;}
 check(refused&&finals==1,"Failed finalist yielded a usable profile");
 refused=false;finals=0;
 try{runWindowsAutoBenchmark(68,10240,true,30,[&](const auto&,int,bool final)->std::optional<BenchmarkResult>{if(final)++finals;return {};});}catch(...){refused=true;}
 check(refused&&finals==0,"All failed candidates still launched a finalist");
 refused=false;calls=0;
 try{runWindowsAutoBenchmark(1,256,true,30,[&](const auto&,int,bool)->std::optional<BenchmarkResult>{++calls;return stable();});}catch(...){refused=true;}
 check(refused&&calls==0,"Unsafe fallback on low-memory GPU was executed");
 for(const int units:{0,-1,4097}){refused=false;try{runWindowsAutoBenchmark(units,10240,true,30,{});}catch(...){refused=true;}check(refused,"Invalid capabilities accepted");}
 std::cout<<"WindowsAutoBenchmarkTests passed: shared Linux matrix, memory budget, stability, finalist, margin, rejection and cancellation\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
