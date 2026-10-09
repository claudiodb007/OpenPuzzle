#include "openpuzzle/runtime/ClientRuntime.hpp"
#include <iostream>
#include <stdexcept>
using namespace openpuzzle;
using Result=client::ExecutionSyncResult;
using Upload=client::AssignmentUploadStatus;
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
struct Fixture {
  bool stop=false,safe=false,pending=false,owned=false,canStop=true,wake=false;
  int sleeps=0,clears=0,finalized=0,heartbeats=0,attempts=0;
  ClientRuntimeDependencies deps(){
    ClientRuntimeDependencies d;
    d.sync=[this](const std::string&){++attempts;Result r;r.hasState=true;r.hasExitCode=true;r.exitCode=0;r.completionUploaded=true;r.stateRemoved=true;return r;};
    d.heartbeat=[this](const std::string&){++heartbeats;client::ClientHeartbeatResult r;r.success=heartbeats>1;return r;};
    d.stopExecution=[this](const std::string&){return canStop;};
    d.reportSolution=[](const auto&,const auto&,const auto&,auto&){return true;};
    d.finalizeAssignment=[this](const auto&,const auto&,const auto&,int,const auto&,const auto&,auto&){++finalized;return Upload::TemporaryFailure;};
    d.finalKeysChecked=[](const auto&){return std::string("8");};
    d.removeState=[this]{++clears;pending=false;return true;};d.hasState=[this]{return pending;};
    d.acquireRuntime=[this]{owned=true;return true;};d.releaseRuntime=[this]{owned=false;};
    d.stopRequested=[this]{return stop;};d.safeStopRequested=[this]{return safe;};d.clearSafeStop=[] {return true;};
    d.prepareSignals=[]{};d.sleep=[this](auto){++sleeps;};d.wakeRequested=[this]{return wake;};return d;
  }
};
int main(){try{
  {Fixture f;f.safe=true;int requested=0;require(ClientRuntime(f.deps()).runContinuous("fixture",[&]{++requested;return ClientIterationResult{};})==0&&requested==0&&!f.owned,"Idle safe stop requested more work");}
  {Fixture f;int requested=0;auto d=f.deps();d.sleep=[&](auto){++f.sleeps;f.wake=true;};
    require(ClientRuntime(d).runContinuous("fixture",[&]{++requested;if(requested==2)f.safe=true;return ClientIterationResult{};})==0&&requested==2&&f.heartbeats==2&&!f.owned&&f.sleeps==1,"Availability acknowledgement or native wake lost");}
  {Fixture f;f.pending=true;f.stop=true;f.canStop=false;auto d=f.deps();d.sync=[](const auto&){Result r;r.hasState=true;r.running=true;return r;};
    require(ClientRuntime(d).run("fixture","assignment","client","workspace")==1&&f.pending&&f.clears==0&&f.finalized==0,"Failed stop erased pending state");}
  {Fixture f;f.pending=true;auto d=f.deps();d.sync=[&](const auto&){++f.attempts;Result r;r.hasState=true;r.hasExitCode=true;r.completionStatus=Upload::TemporaryFailure;if(f.attempts==2){r.completionUploaded=true;r.stateRemoved=true;}return r;};
    require(ClientRuntime(d).run("fixture","assignment","client","workspace")==0&&f.attempts==2&&f.sleeps==60&&f.clears==0,"Temporary completion was not retried at the normal interval");}
  {Fixture f;f.pending=true;auto d=f.deps();d.sync=[&](const auto&){++f.attempts;Result r;r.hasState=true;r.identityUnavailable=true;return r;};d.sleep=[&](auto){throw std::runtime_error("end-fixture");};
    try{ClientRuntime(d).run("fixture","assignment","client","workspace");require(false,"Unknown identity did not wait");}catch(const std::runtime_error& e){require(std::string(e.what())=="end-fixture","Unexpected monitor failure");}
    require(f.pending&&f.clears==0&&f.finalized==0&&f.attempts==1,"Unknown identity was finalized or discarded");}
  std::cout<<"ClientRuntimeLoopTests passed: shared continuous/monitor loops, wake, acknowledgement, retries and preservation\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
