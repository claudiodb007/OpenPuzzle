// MIT. Synthetic coordinator only; no HTTP transport or production dependencies.
#include "VanityPreviewEngine.hpp"
#include "openpuzzle/core/PosixProcessRunner.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/runtime/ClientRuntime.hpp"
#include "openpuzzle/client/RangeAssignment.hpp"
#include <fstream>
#include <iostream>
#include <set>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
using namespace openpuzzle;
using namespace openpuzzle::vanityPreview;
namespace fs=std::filesystem;
static volatile sig_atomic_t interrupted=0;
static void stopSignal(int){interrupted=1;}
static void insist(bool b,const char* m){if(!b)throw std::runtime_error(m);}
static void privateDirectory(const fs::path& p){insist(!fs::is_symlink(fs::symlink_status(p)),"Symlink directory refused");if(!fs::exists(p))fs::create_directory(p);insist(fs::is_directory(p),"Not a directory");fs::permissions(p,fs::perms::owner_all,fs::perm_options::replace);}
static void writePrivate(const fs::path& p,const std::string& text){insist(!fs::is_symlink(fs::symlink_status(p)),"Symlink record refused");insist(writePrivateFileAtomically(p,text),"Atomic private write failed");int fd=open(p.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);bool ok=fd>=0&&fsync(fd)==0;if(fd>=0)close(fd);insist(ok,"Directory fsync failed");}
struct Owner{int fd;explicit Owner(const fs::path& p){fd=open(p.c_str(),O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0||flock(fd,LOCK_EX|LOCK_NB)!=0){if(fd>=0)close(fd);throw std::runtime_error("Local runtime already owned");}}~Owner(){close(fd);}};
struct Ledger {
 unsigned index=0,claims=0,launches=0,completed=0,heartbeats=0,ackAttempts=0;
 bool controlUsed=false,ackFailureUsed=false;std::string phase="idle";
};
struct Checkpoint{std::string binding,start,mode,status;uint64_t count=0,grid=0,checked=0;bool safe=false;};
static Checkpoint readCheckpoint(const fs::path& p){
 insist(!fs::is_symlink(fs::symlink_status(p))&&fs::is_regular_file(p)&&fs::file_size(p)<4096,"Missing/unsafe native checkpoint");
 std::ifstream file(p);Checkpoint c;std::string magic,count,grid,checked,safe,extra;
 insist(bool(file>>magic>>c.binding>>c.start>>count>>grid>>c.mode>>checked>>c.status>>safe)&&!(file>>extra)&&magic=="OPV-CHECKPOINT-1","Malformed native checkpoint");
 c.count=decimal(count);c.grid=decimal(grid);c.checked=decimal(checked);insist(safe=="0"||safe=="1","Malformed safe stop flag");c.safe=safe=="1";return c;
}
int runVanityLocalAssignment(int argc,char** argv){
 try{
  insist(argc==15,"Usage: vanity-preview ADAPTER ENGINE ROOT CONTROLS KNOWN_KEYS EXPECTED_SOLUTION START COUNT GRID BINDING_A BINDING_B SCENARIO --enable-experimental-vanity-local");
  insist(std::string(argv[1])=="vanity-preview"&&std::string(argv[14])=="--enable-experimental-vanity-local","Explicit local experimental selection required");
  std::string adapter=fs::absolute(argv[2]),engine=fs::absolute(argv[3]);fs::path root=fs::absolute(argv[4]),controls=fs::absolute(argv[5]),keys=fs::absolute(argv[6]),expectedFile=fs::absolute(argv[7]);
  Plan first(argv[8],decimal(argv[9]),decimal(argv[10]));Plan second(add(first.start,first.count),first.count,first.blocks);
  std::string bindings[2]={argv[11],argv[12]},scenario=argv[13];
  for(const auto& b:bindings)insist(b.size()==64&&b.find_first_not_of("0123456789abcdef")==std::string::npos,"Invalid binding");
  const std::set<std::string> scenarios={"normal","safe-stop","idle-safe-stop","completion-retry","heartbeat-retry","cancel-resume","owner-crash","lease-rejected","solution"};
  insist(scenarios.count(scenario),"Unknown local scenario");insist(fs::is_regular_file(keys)&&fs::is_regular_file(expectedFile),"Known synthetic fixture required");
  privateDirectory(root);privateDirectory(controls);Owner owner(root/"owner.lock");auto ledgerFile=root/"local-ledger.txt";
  const std::string identity=bindings[0]+" "+bindings[1]+" "+first.start+" "+std::to_string(first.count)+" "+std::to_string(first.blocks)+" "+scenario;
  Ledger state;
  if(fs::exists(ledgerFile)){
   insist(!fs::is_symlink(fs::symlink_status(ledgerFile))&&fs::file_size(ledgerFile)<4096,"Unsafe ledger");std::ifstream in(ledgerFile);std::string magic,id,extra;std::getline(in,magic);std::getline(in,id);
   insist(magic=="OPV-LOCAL-1"&&id==identity,"Local assignment binding mismatch; preserved");
   insist(bool(in>>state.index>>state.claims>>state.launches>>state.completed>>state.heartbeats>>state.ackAttempts>>state.controlUsed>>state.ackFailureUsed>>state.phase)&&!(in>>extra),"Malformed ledger");
   insist(state.index<=2&&state.completed<=2&&state.claims<=2&&state.index==state.completed,"Impossible local assignment state");
   if(state.phase=="done"||state.phase=="stopped"||state.phase=="solution"||state.phase=="rejected"||state.phase=="faulted"){
    std::cout<<"OPV_LOCAL BLOCKED "<<state.phase<<"\n";return 20;
   }
   insist(state.phase=="idle"||state.phase=="assigned"||state.phase=="pending-complete","Unknown local phase");
  }else{
   for(const auto& e:fs::directory_iterator(root))insist(e.path().filename()=="owner.lock","Occupied local root without ledger");
  }
  auto save=[&]{std::ostringstream o;o<<"OPV-LOCAL-1\n"<<identity<<"\n"<<state.index<<' '<<state.claims<<' '<<state.launches<<' '<<state.completed<<' '<<state.heartbeats<<' '<<state.ackAttempts<<' '<<state.controlUsed<<' '<<state.ackFailureUsed<<' '<<state.phase<<"\n";writePrivate(ledgerFile,o.str());};
  auto event=[&](const std::string& name){std::ofstream out(root/"events.log",std::ios::app);fs::permissions(root/"events.log",fs::perms::owner_read|fs::perms::owner_write);out<<name<<"\n";out.flush();insist(bool(out),"Local event write failed");};
  bool stop=false,safe=scenario=="idle-safe-stop",owned=false,bad=false,rejected=false;
  std::string error;client::ClientExecutionState execution;fs::path currentWorkspace,canonicalSolution;
  save();signal(SIGINT,stopSignal);signal(SIGTERM,stopSignal);
  if(state.controlUsed&&scenario=="cancel-resume"&&fs::exists(controls/"cancel")){
   insist(!fs::exists(controls/"cancel-consumed"),"Occupied cancellation archive");fs::rename(controls/"cancel",controls/"cancel-consumed");
  }
  auto check=[&]{auto c=readCheckpoint(currentWorkspace/"checkpoint.txt");
    insist(state.index<2&&c.binding==bindings[state.index]&&c.start==canonical(execution.start)&&c.count==first.count&&c.grid==first.blocks&&c.checked<=c.count,"Native assignment identity/coverage mismatch");return c;};
  ClientRuntimeDependencies d;
  d.acquireRuntime=[&]{if(owned)return false;owned=true;return true;};d.releaseRuntime=[&]{owned=false;};
  d.hasState=[&]{return state.phase=="assigned"||state.phase=="pending-complete"||state.phase=="solution";};
  d.stopRequested=[&]{return stop||interrupted;};d.safeStopRequested=[&]{return safe;};d.clearSafeStop=[] {return true;};d.prepareSignals=[]{};
  d.sleep=[](auto){std::this_thread::sleep_for(std::chrono::milliseconds(1));}; // Virtual local coordinator retry time; no network.
  d.heartbeat=[&](const auto&){++state.heartbeats;save();client::ClientHeartbeatResult r;
    r.success=!(scenario=="heartbeat-retry"&&state.heartbeats==1);event(r.success?"idle-heartbeat-accepted":"idle-heartbeat-retry");return r;};
  d.stopExecution=[](const auto&){return true;}; // Monitor is entered only after the owned child is confirmed stopped.
  d.reportSolution=[&](const auto&,const auto&,const auto&,auto&){event("local-solution-pending-review");return true;};
  d.finalizeAssignment=[](const auto&,const auto&,const auto&,int,const auto&,const auto&,auto&)->client::AssignmentUploadStatus{throw std::runtime_error("Unexpected production-style finalization in local fixture");};
  d.finalKeysChecked=[&](const auto&){return std::to_string(check().checked);};
  d.removeState=[]()->bool{throw std::runtime_error("Unexpected external state removal");};
  d.sync=[&](const auto&){client::ExecutionSyncResult r;r.hasState=true;r.state=execution;
    if(state.phase=="solution"){r.solutionFound=true;r.solutionPath=canonicalSolution;return r;}
    insist(state.phase=="pending-complete","Unexpected local sync phase");auto c=check();insist(c.status=="complete"&&c.checked==c.count,"Completion requires exact native proof");
    ++state.ackAttempts;r.hasExitCode=true;r.exitCode=0;
    if(scenario=="completion-retry"&&!state.ackFailureUsed){state.ackFailureUsed=true;r.completionStatus=client::AssignmentUploadStatus::TemporaryFailure;r.completionError="synthetic acknowledgement retry";event("completion-retry");save();return r;}
    event("completion-accepted");++state.completed;++state.index;state.phase=state.index==2?"done":"idle";save();
    r.completionStatus=client::AssignmentUploadStatus::Uploaded;r.completionUploaded=true;r.stateRemoved=true;return r;};
  ClientRuntime runtime(d);
  int code=runtime.runContinuous("local-fixture",[&]()->ClientIterationResult{
    insist(state.index<2,"Unexpected third local assignment request");Plan plan(state.index==0?first:second);
    const std::string id=state.index==0?"local-a":"local-b";
    client::RangeAssignment assignment;assignment.assignmentId=id;assignment.puzzle=20;assignment.rangeId=int(state.index+1);assignment.target="synthetic-known-target";assignment.start=plan.start;assignment.end=plan.end;assignment.requiredBackend="cuda";insist(assignment.valid(),"Invalid synthetic assignment");
    currentWorkspace=root/id;execution={};execution.active=true;execution.assignmentId=id;execution.clientId="local-client";execution.puzzle=20;execution.rangeId=assignment.rangeId;execution.pid=getpid();execution.engine="vanity-preview";execution.backend="cuda";execution.start=plan.start;execution.end=plan.end;execution.workspace=currentWorkspace;
    if(state.phase=="idle"){++state.claims;state.phase="assigned";save();event("claim-"+id);}
    insist(state.phase=="assigned"||state.phase=="pending-complete","Preserved state prevents a new claim");
    uint64_t resumed=0;
    if(fs::exists(currentWorkspace/"checkpoint.txt")){
      auto c=check();resumed=c.checked;
      if(c.status=="complete"&&c.checked==plan.count)state.phase="pending-complete";
      else insist(c.status=="interrupted"||c.status=="running","Preserved native result requires review");
    }
    if(state.phase!="pending-complete"){
      AdapterConfig config{adapter,engine,keys.string(),bindings[state.index],controls.string(),scenario=="solution"?"stop":"exhaust"};
      EngineRegistry registry;EngineFactory factory;registerVanityPreview(registry,factory,config,true);
      auto selected=factory.create("vanity-preview",engine);insist(selected->prepare(),"Experimental engine unavailable");
      EngineLaunchRequest request;request.engine="vanity-preview";request.backend="cuda";request.startKey=plan.start;request.endKey=plan.end;request.device=0;request.blocks=int(plan.blocks);request.threads=256;request.points=1024;request.workspace=currentWorkspace;request.targetFile=keys;
      auto command=selected->buildCommand(request);++state.launches;save();event("launch-"+id);
      VanityPreviewProgressParser parser(plan.count,resumed);PosixProcessRunner process;
      auto logPath=root/("launch-"+std::to_string(state.launches)+".log");std::ofstream log(logPath);fs::permissions(logPath,fs::perms::owner_read|fs::perms::owner_write);
      auto result=process.run(command,[&](const std::string& line){
       log<<line<<'\n';log.flush();if(bad)return;
       try{auto progress=parser.parseLine(line);if(progress){
         if(!progress->keysChecked.empty()){
          auto n=decimal(progress->keysChecked);std::ostringstream h;h<<"engine=vanity-preview\nbackend=cuda\nslot=cuda-0\nassignment="<<id<<"\nkeys_checked="<<n<<"\nspeed_mkeys="<<progress->speedMKeys<<"\n";writePrivate(root/"local-heartbeat.txt",h.str());
          if(n>0&&n<plan.count&&!state.controlUsed){
           if(scenario=="safe-stop"){safe=true;state.controlUsed=true;writePrivate(controls/"safe-stop","safe-stop\n");event("safe-stop-requested");save();}
           if(scenario=="cancel-resume"||scenario=="lease-rejected"){
            state.controlUsed=true;rejected=scenario=="lease-rejected";writePrivate(controls/"cancel","cancel\n");event(rejected?"lease-rejected":"cancel-requested");save();}
          }
         }
       }}catch(const std::exception& e){bad=true;error=e.what();}
      },900,[&]{if(interrupted)writePrivate(controls/"cancel","cancel\n");return bad||interrupted;});
      if(bad){state.phase="faulted";save();throw std::runtime_error(error);}
      auto c=check();safe=safe||c.safe;
      if(rejected){state.phase="rejected";save();stop=true;return ClientIterationResult(2);}
      if(result.exitCode==130||interrupted){stop=true;save();return ClientIterationResult(130);}
      if(result.exitCode==10&&c.status=="solution"){
       std::ifstream expected(expectedFile);std::string expectedKey,expectedHash,address,wif,extra;
       insist(bool(expected>>expectedKey>>expectedHash>>address>>wif)&&!(expected>>extra),"Malformed known solution map");
       unsigned hits=0;for(const auto& a:fs::directory_iterator(currentWorkspace)){
        if(a.path().filename().string().rfind("attempt-",0)!=0)continue;auto p=a.path()/"solutions.txt";if(!fs::exists(p))continue;
        insist(!fs::is_symlink(fs::symlink_status(p)),"Unsafe synthetic result");std::ifstream file(p);std::string marker,key,hash;
        while(file>>marker>>key>>hash){insist(marker=="SYNTHETIC"&&key==expectedKey&&hash==expectedHash&&key>=plan.start&&key<=plan.end,"Unexpected synthetic solution");++hits;}
       }
       insist(hits==1,"Expected one independently specified synthetic solution");
       canonicalSolution=root/(id+"-solution-v1.txt");writePrivate(canonicalSolution,"OPENPUZZLE_SOLUTION_V1\naddress="+address+"\nprivate_key_wif="+wif+"\ncompression=compressed\n");
       execution.target=address;state.phase="solution";save();int monitored=runtime.run("local-fixture",id,"local-client",currentWorkspace.string());
       insist(monitored==ClientRuntime::SolutionFoundExitCode,"Shared solution monitor failed");
       const char* home=std::getenv("HOME");insist(home&&*home,"Missing isolated export home");
       auto wallet=fs::path(home)/"OpenPuzzle-Solutions"/"Puzzle-20"/id/"wallet-import.txt";
       insist(!fs::is_symlink(fs::symlink_status(wallet))&&fs::is_regular_file(wallet)&&fs::file_size(wallet)<8192,"Protected synthetic export missing");
       insist((fs::status(wallet).permissions()&(fs::perms::group_all|fs::perms::others_all))==fs::perms::none,"Synthetic export permissions invalid");
       std::ifstream walletFile(wallet);std::string walletText((std::istreambuf_iterator<char>(walletFile)),{});
       insist(walletText.find(wif)!=std::string::npos,"Protected synthetic export content mismatch");
       return ClientIterationResult::solutionFound();
      }
      insist(result.started&&result.exitCode==0&&c.status=="complete"&&c.checked==plan.count,"Native execution failed or incomplete");state.phase="pending-complete";save();
    }
    int monitored=runtime.run("local-fixture",id,"local-client",currentWorkspace.string());
    if(monitored!=0)return ClientIterationResult(monitored);
    if(state.index==2)stop=true;
    return ClientIterationResult{};
  });
  if(safe&&state.phase!="solution"){state.phase="stopped";save();}
  std::ostringstream result;result<<"code="<<code<<"\nclaims="<<state.claims<<"\nlaunches="<<state.launches<<"\ncompleted="<<state.completed<<"\nheartbeats="<<state.heartbeats<<"\nack_attempts="<<state.ackAttempts<<"\nphase="<<state.phase<<"\n";writePrivate(root/"result.txt",result.str());
  std::cout<<"OPV_LOCAL RESULT "<<state.phase<<" claims="<<state.claims<<" launches="<<state.launches<<" completed="<<state.completed<<"\n";return code;
 }catch(const std::exception& e){std::cerr<<"Local integration refused: "<<e.what()<<"\n";return 2;}
}

#ifdef OPENPUZZLE_VANITY_STANDALONE
int main(int argc,char** argv){return runVanityLocalAssignment(argc,argv);}
#endif
