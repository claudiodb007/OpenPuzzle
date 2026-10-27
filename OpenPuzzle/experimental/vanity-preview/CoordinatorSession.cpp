// MIT. Native client payloads and common completion monitor; loopback-only stage.
#include "CoordinatorSession.hpp"
#include "CoordinatorPlan.hpp"
#include "CoordinatorPacer.hpp"
#include "VanityPreviewEngine.hpp"
#include "LoopbackHttp.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/SolutionExporter.hpp"
#include "openpuzzle/runtime/ClientRuntime.hpp"
#include "openpuzzle/core/PosixProcessRunner.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include <csignal>
#include <iostream>
#include <tuple>
#include <fstream>
#include <iomanip>
#include <set>
#include <thread>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
namespace openpuzzle::vanityPreview {
namespace {
namespace fs=std::filesystem;
void need(bool v,const std::string& m){if(!v)throw std::runtime_error(m);}
volatile sig_atomic_t interrupted=0;
void onSignal(int){interrupted=1;}
struct Signals {using Handler=void(*)(int);Handler term,intr;Signals():term(signal(SIGTERM,onSignal)),intr(signal(SIGINT,onSignal)){interrupted=0;}~Signals(){signal(SIGTERM,term);signal(SIGINT,intr);}};
void directory(const fs::path& p){need(p.is_absolute()&&!fs::is_symlink(fs::symlink_status(p)),"Unsafe coordinator directory");if(!fs::exists(p))fs::create_directory(p);struct stat st{};need(stat(p.c_str(),&st)==0&&S_ISDIR(st.st_mode)&&st.st_uid==getuid()&&(st.st_mode&0077)==0&&fs::canonical(p)==p,"Coordinator directory must be private and canonical");}
std::string read(const fs::path& p){need(!fs::is_symlink(fs::symlink_status(p))&&fs::is_regular_file(p)&&fs::file_size(p)<1024*1024,"Missing/unsafe coordinator record");std::ifstream f(p);return std::string((std::istreambuf_iterator<char>(f)),{});}
void write(const fs::path& p,const std::string& text){need(!fs::is_symlink(fs::symlink_status(p))&&writePrivateFileAtomically(p,text),"Coordinator record not published");int fd=open(p.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);bool ok=fd>=0&&fsync(fd)==0;if(fd>=0)close(fd);need(ok,"Coordinator directory sync failed");}
struct Lock{int fd;explicit Lock(const fs::path& p){fd=open(p.c_str(),O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0||flock(fd,LOCK_EX|LOCK_NB)){if(fd>=0)close(fd);throw std::runtime_error("Coordinator workspace already owned");}}~Lock(){close(fd);}};
std::string invoke(const std::string& command){PosixProcessRunner runner;std::string text;bool large=false;auto result=runner.run(command,[&](const auto& row){text+=row+'\n';large=text.size()>65536;},20,[&]{return large;});need(result.started&&result.exitCode==0&&!large,"Bound executable identity/target/checksum failed");return text;}
std::string digest(const std::string& value){auto output=invoke("printf %s "+shellQuote(value)+" | sha256sum");return output.substr(0,64);}
struct State{std::string phase="fresh";client::RangeAssignment a;uint64_t offset=0,checked=0,launches=0;};
std::string encode(const State& s,const std::string& binding){std::ostringstream o;o<<"OPV-COORD-1\n"<<binding<<'\n'<<s.phase<<' '<<s.offset<<' '<<s.checked<<' '<<s.launches<<'\n'<<std::quoted(s.a.assignmentId)<<' '<<s.a.puzzle<<' '<<s.a.rangeId<<' '<<std::quoted(s.a.target)<<' '<<std::quoted(s.a.start)<<' '<<std::quoted(s.a.end)<<' '<<std::quoted(s.a.searchMode)<<' '<<std::quoted(s.a.requiredBackend)<<' '<<std::quoted(s.a.publicKey)<<'\n';return o.str();}
State decode(const std::string& text,const std::string& binding){State s;std::istringstream in(text);std::string magic,b,extra;need(bool(in>>magic>>b>>s.phase>>s.offset>>s.checked>>s.launches>>std::quoted(s.a.assignmentId)>>s.a.puzzle>>s.a.rangeId>>std::quoted(s.a.target)>>std::quoted(s.a.start)>>std::quoted(s.a.end)>>std::quoted(s.a.searchMode)>>std::quoted(s.a.requiredBackend)>>std::quoted(s.a.publicKey))&&!(in>>extra)&&magic=="OPV-COORD-1"&&b==binding,"Coordinator binding/state differs");return s;}
struct Native {uint64_t checked=0;std::string status="fresh";bool safe=false;};
Native checkpoint(const fs::path& path,const Plan& plan,const std::string& binding){if(!fs::exists(path))return {};std::istringstream in(read(path));std::string magic,b,start,n,grid,mode,count,status,safe,extra;need(bool(in>>magic>>b>>start>>n>>grid>>mode>>count>>status>>safe)&&!(in>>extra)&&magic=="OPV-CHECKPOINT-1"&&b==binding&&start==plan.start&&decimal(n)==plan.count&&decimal(grid)==plan.blocks&&mode=="stop"&&(safe=="0"||safe=="1"),"Native segment checkpoint differs");auto checked=decimal(count);need(checked<=plan.count,"Native segment counter exceeds range");return {checked,status,safe=="1"};}
void evidence(const fs::path& workspace,const Plan& plan,const Native& checkpoint){
 if(checkpoint.status=="fresh")return;
 std::vector<std::tuple<uint64_t,uint64_t,bool>> spans;
 for(const auto& item:fs::directory_iterator(workspace)){
  if(!item.path().filename().string().starts_with("attempt-"))continue;
  need(!fs::is_symlink(item.symlink_status())&&fs::is_directory(item.path())&&fs::canonical(item.path())==item.path(),"Unsafe native attempt");
  std::istringstream rows(read(item.path()/"engine.log"));std::string row;std::vector<std::string> records;
  while(std::getline(rows,row))if(row.starts_with("OPV1 "))records.push_back(row);
  need(!records.empty(),"Missing native range evidence");std::istringstream begin(records.front());std::string magic,kind,start,count,batch,extra;
  need(bool(begin>>magic>>kind>>start>>count>>batch)&&!(begin>>extra)&&magic=="OPV1"&&kind=="BEGIN","Invalid native range evidence");
  Plan sub(start,decimal(count),plan.blocks);need(sub.start>=plan.start&&sub.end==plan.end,"Native attempt escaped segment");
  Protocol protocol(sub);for(const auto& record:records)protocol.consume(record);
  uint64_t offset=sub.start==plan.start?0:inclusiveCount(plan.start,sub.start)-1;
  spans.emplace_back(offset,protocol.checked,protocol.complete);
 }
 std::sort(spans.begin(),spans.end());uint64_t checked=0;bool terminal=false;
 for(const auto& [offset,count,complete]:spans){need(offset==checked,"Native attempts overlap or skip keys");checked+=count;terminal=complete;}
 need(checked==checkpoint.checked,"Checkpoint exceeds or differs from native coverage");
 if(checkpoint.status=="complete")need(terminal&&checked==plan.count,"Completion lacks exact native terminal proof");
}
bool rejected(const std::string& code){return code=="assignment_not_found"||code=="assignment_client_mismatch"||code=="invalid_assignment_state"||code=="assignment_lease_expired"||code=="puzzle_verification_pending"||code=="puzzle_solved";}
}
int runCoordinatorSession(const std::map<std::string,std::string>& o,bool resume){
 try{
  const auto get=[&](const char* key)->const std::string&{return o.at(key);};
  const auto server=get("--server"),clientId=get("--client-id"),target=get("--target");LoopbackHttp local(server);
  const auto grid=decimal(get("--grid")),cap=decimal(get("--segment-keys"));const int puzzle=20;need(get("--puzzle")=="20","Local puzzle must be exactly 20");
  need(puzzle==20&&std::regex_match(clientId,std::regex("opv-coord-[A-Za-z0-9-]{1,80}")),"Isolated coordinator identity/puzzle required");
  const auto scopeStart=canonical(get("--scope-start")),scopeEnd=canonical(get("--scope-end"));need(scopeEnd>=scopeStart,"Descending local scope");
  const char* visible=getenv("CUDA_VISIBLE_DEVICES");need(visible&&get("--gpu-uuid")==visible,"Physical GPU binding required");
  need(std::regex_match(get("--speed-mkeys"),std::regex("[0-9]+(\\.[0-9]+)?")),"Invalid planning speed syntax");double speed=std::stod(get("--speed-mkeys"));need(std::isfinite(speed)&&speed>0&&speed<1000000,"Invalid measured planning speed");
  const fs::path root=get("--root"),controls=get("--controls"),engine=get("--engine"),adapter=get("--adapter");
  need(engine.is_absolute()&&adapter.is_absolute()&&root.is_absolute()&&controls==root/"controls","Explicit owned coordinator paths required");
  for(auto pair:{std::pair{engine,get("--engine-sha256")},std::pair{adapter,get("--adapter-sha256")}}){need(!fs::is_symlink(fs::symlink_status(pair.first))&&fs::is_regular_file(pair.first)&&access(pair.first.c_str(),X_OK)==0,"Bound executable missing/not executable");need(invoke("sha256sum < "+shellQuote(pair.first.string())).substr(0,64)==pair.second,"Bound executable changed");}
  client::RangeAssignment localScope;localScope.assignmentId="scope";localScope.puzzle=20;localScope.rangeId=1;localScope.target=target;localScope.start=scopeStart;localScope.end=scopeEnd;localScope.requiredBackend="cuda";
  CoordinatorPlan scope(localScope,puzzle,target,grid,cap);(void)scope;
  need(getenv("HOME")&&fs::path(getenv("HOME"))==root/"home","Isolated coordinator export home required");directory(root);directory(controls);if(fs::exists(root/"home"))directory(root/"home");Lock lock(root/"owner.lock");Signals signals;
  std::string raw;for(const auto& [k,v]:o)raw+=k+'='+v+'\n';raw+="execution_slot="+client::ClientStateStore::executionSlot()+"\n";raw+="coordinator_cli_sha256="+invoke("sha256sum < "+shellQuote(fs::canonical("/proc/self/exe").string())).substr(0,64)+"\n";const auto binding=digest(raw);State state;
  const auto stateFile=root/"coordinator-state.txt",targetFile=root/"targets.txt";
  auto save=[&]{write(stateFile,encode(state,binding));};
  if(fs::exists(stateFile)){
   state=decode(read(stateFile),binding);need(read(root/"session-binding.txt")==raw,"Published coordinator binding differs");
   if(!resume||!(state.phase=="assigned"||state.phase=="paused"||state.phase=="running"||state.phase=="pending-complete"||state.phase=="solution")) {std::cout<<"OPV_COORD BLOCKED "<<state.phase<<" next_claim_blocked=1\n";return 20;}
  }else{need(!resume,"No coordinator run to resume");for(const auto& f:fs::directory_iterator(root))need(f.path().filename()=="owner.lock"||f.path().filename()=="controls"||f.path().filename()=="home","Occupied coordinator root preserved");write(root/"session-binding.txt",raw);save();}
  const auto targetText="OPV-TARGETS-1\np2pkh "+target+"\n";
  if(fs::exists(targetFile))need(read(targetFile)==targetText,"Coordinator public target changed");else write(targetFile,targetText);
  need(invoke(shellQuote(engine.string())+" --identity")=="OpenPuzzle-VanityRange 0.2.0 CUDA target protocol 1\n","Wrong target engine identity");
  std::istringstream info(invoke(shellQuote(engine.string())+" --target-info "+shellQuote(targetFile.string())));std::string hash,address,extra;need(bool(info>>hash>>address)&&!(info>>extra)&&address==target&&hash.size()==40,"Public target decode differs");
  const char* suppliedAttempt=getenv("OPENPUZZLE_VANITY_ATTEMPT");
  const std::string attemptToken=suppliedAttempt?suppliedAttempt:("native-"+std::to_string(getpid()));
  need(std::regex_match(attemptToken,std::regex("[A-Za-z0-9-]{1,80}")),"Invalid coordinator attempt identity");
  int transportStatus=0;
  std::string transportKind="review",failureKind,failureOperation;
  auto publishOutcome=[&](const std::string& kind,const std::string& operation,int code){
   std::ostringstream out;out<<"OPV-RECOVERY-1\n"<<binding<<'\n'<<kind<<' '<<operation<<' '<<transportStatus<<' '<<code<<' '<<state.phase<<' '<<state.offset<<' '<<state.checked<<' '<<state.launches<<' '<<getpid()<<' '<<attemptToken<<'\n';
   write(root/"coordinator-outcome.txt",out.str());
  };
  auto backoff=[&](int attempt){
   const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(attempt==0?100:250);
   while(!interrupted&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(10));
  };
  client::HttpRangeClient http(server,[&](const std::string& url,const std::string& body){
   try{need(url.starts_with(server+"/api/"),"Transport escaped local endpoint");auto response=local.post(url.substr(server.size()),body);
    transportStatus=response.status;transportKind=(response.status==408||response.status==429||(response.status>=500&&response.status<=599))?"network":"review";
    if(response.status>=200&&response.status<300&&url!=server+"/api/range/claim"&&(response.status!=200||response.body!="{\"success\":true}")){transportKind="review";return client::HttpPostResult{502,response.body,"Unacknowledged local response"};}
    return client::HttpPostResult{response.status,response.body,{}};
   }catch(const std::exception& error){transportStatus=0;transportKind="network";return client::HttpPostResult{0,{},error.what()};}
  });
  auto heartbeat=[&](const std::string& status){client::ClientHeartbeat h;h.clientId=clientId;h.version="vanity-coordinator-0.11.0";h.platform="linux-local-fixture";h.status=status;h.cpu={"Isolated coordinator fixture",1,1};h.engines.push_back({"Vanity","cuda",true,true});if(status=="running"){h.activeEngine="Vanity";h.activeBackend="cuda";h.activeBackends={"cuda"};}return http.heartbeat(h);};
  auto stopped=[&]{return interrupted||fs::exists(controls/"cancel");};
  if(state.phase=="fresh"){
   if(stopped()||fs::exists(controls/"safe-stop")){state.phase="stopped";save();return 130;}
   need(http.registerClient({clientId}),"Local registration failed: "+http.lastError());need(heartbeat("idle"),"Local heartbeat failed: "+http.lastError());
   state.phase="claiming";save();auto claim=http.claimResult(clientId,puzzle,60,speed,"cuda","linear");
   if(claim.unavailable()){state.phase="unavailable";save();return 0;}
   need(claim.assigned(),"Local claim failed/ambiguous; preserve state: "+claim.message);
   state.a=*claim.assignment;CoordinatorPlan valid(state.a,puzzle,target,grid,cap);need(valid.start>=scopeStart&&valid.end<=scopeEnd,"Assignment escaped local metadata scope");state.phase="assigned";save();
  }
  CoordinatorPlan plan(state.a,puzzle,target,grid,cap);need(plan.start>=scopeStart&&plan.end<=scopeEnd&&state.offset<=state.checked&&state.checked<=plan.total,"Stored assignment differs from scope");
  need(state.offset==plan.total||state.offset%cap==0,"Unaligned completed segment prefix");
  for(uint64_t offset=0;offset<state.offset;){auto segment=plan.segment(offset);auto workspace=root/("segment-"+std::to_string(offset/cap));auto n=checkpoint(workspace/"checkpoint.txt",segment,digest(binding+"\n"+std::to_string(offset)));need(n.status=="complete"&&n.checked==segment.count,"Completed segment checkpoint missing");evidence(workspace,segment,n);offset+=segment.count;}
  // After abrupt owner loss, the adapter may have durably acknowledged more
  // local batches than the last coordinator counter. Validate before probing.
  if(resume&&state.phase!="pending-complete"&&state.phase!="solution"&&state.offset<plan.total){
   auto segment=plan.segment(state.offset);auto workspace=root/("segment-"+std::to_string(state.offset/cap));
   auto native=checkpoint(workspace/"checkpoint.txt",segment,digest(binding+"\n"+std::to_string(state.offset)));evidence(workspace,segment,native);
   need(native.status=="fresh"||native.status=="interrupted"||native.status=="complete","Native segment requires review");
   need(state.checked<=state.offset+native.checked,"Coordinator counter exceeds durable coverage");
   state.checked=state.offset+native.checked;save();
  }
  auto failed=[&](const std::string& operation){
   failureKind=rejected(http.lastErrorCode())?"lease":transportKind;failureOperation=operation;
   state.phase=failureKind=="lease"?"rejected":"paused";save();return false;
  };
  CoordinatorPacer pacer;
  auto nextHeartbeat=std::chrono::steady_clock::now()+std::chrono::seconds(30);
  auto progress=[&](uint64_t checked,double rate=0.,bool force=false){
   need(checked>=state.checked&&checked<=plan.total,"Cumulative coordinator progress regressed/overflowed");state.checked=checked;save();
   if(!pacer.due(checked,std::chrono::steady_clock::now(),force))return true;
   if(std::chrono::steady_clock::now()>=nextHeartbeat){if(!heartbeat("running"))return failed("heartbeat");nextHeartbeat=std::chrono::steady_clock::now()+std::chrono::seconds(30);}
   for(int i=0;i<3;++i){if(http.progress(state.a.assignmentId,clientId,rate,std::to_string(checked))){pacer.acknowledged(checked,std::chrono::steady_clock::now());return true;}
    if(rejected(http.lastErrorCode())||transportKind!="network"||interrupted)break;
    if(i<2)backoff(i);
   }
   return failed("progress");
  };
  auto ensureProgress=[&](uint64_t checked,const char* message,bool probe=true){
   if(!probe&&pacer.confirmed(checked))return;
   if(!progress(checked,0.,true)){publishOutcome(failureKind,failureOperation,2);throw std::runtime_error(message);}
  };
  auto pause=[&](){
   if(!pacer.confirmed(state.checked)&&!progress(state.checked,0.,true)){
    int code=state.phase=="rejected"?2:3;publishOutcome(failureKind,failureOperation,code);return code;
   }
   state.phase="paused";save();publishOutcome("paused","control",130);return 130;
  };
  auto ensureHeartbeat=[&]{
   if(!heartbeat("running")){failed("heartbeat");publishOutcome(failureKind,failureOperation,2);throw std::runtime_error("Running heartbeat failed");}
  };
  if(state.phase!="pending-complete"&&state.phase!="solution"){
   // Probe the current lease before every launch/relaunch. No new claim on resume.
   ensureProgress(state.checked,"Lease/progress probe refused; no next assignment");ensureHeartbeat();
   if(resume && fs::exists(controls/"cancel")){
    need(!fs::is_symlink(fs::symlink_status(controls/"cancel"))&&fs::is_regular_file(controls/"cancel")&&fs::file_size(controls/"cancel")==0,"Unsafe cancel marker preserved");
    const auto archive=root/("resume-before-"+std::to_string(state.launches)+"-"+std::to_string(getpid())+".txt");need(!fs::exists(archive),"Resume journal already exists");write(archive,read(stateFile));fs::remove(controls/"cancel");
   }
   while(state.offset<plan.total){
    if(stopped())return pause();
    auto segment=plan.segment(state.offset);auto workspace=root/("segment-"+std::to_string(state.offset/cap));
    if(!fs::exists(workspace))fs::create_directory(workspace);directory(workspace);
    const auto segmentBinding=digest(binding+"\n"+std::to_string(state.offset));auto native=checkpoint(workspace/"checkpoint.txt",segment,segmentBinding);evidence(workspace,segment,native);
    need(native.status!="review"&&native.status!="faulted","Native segment requires review");
    if(native.status=="solution"){state.phase="solution";save();break;}
    if(native.status=="complete"){
     need(native.checked==segment.count,"Incomplete native terminal");ensureProgress(state.offset+segment.count,"Recovered segment progress refused",false);state.offset+=segment.count;save();continue;
    }
    need(state.checked<=state.offset+native.checked,"Coordinator acknowledged more than durable checkpoint");
    ensureProgress(state.offset+native.checked,"Resume lease probe refused");
    state.phase="running";++state.launches;save();
    const auto logFile=root/("launch-"+std::to_string(state.launches)+".log");need(!fs::exists(logFile),"Existing launch log preserved");std::ofstream log(logFile);fs::permissions(logFile,fs::perms::owner_read|fs::perms::owner_write);
    AdapterConfig config{adapter.string(),engine.string(),targetFile.string(),segmentBinding,controls.string(),"stop",true};
    EngineRegistry registry;EngineFactory factory;registerVanityPreview(registry,factory,config,true);auto selected=factory.create("vanity-preview",engine.string());need(selected&&selected->prepare(),"Registered Vanity engine unavailable");
    EngineLaunchRequest request;request.engine="vanity-preview";request.backend="cuda";request.device=0;request.blocks=int(grid);request.threads=256;request.points=1024;request.startKey=segment.start;request.endKey=segment.end;request.workspace=workspace;request.targetFile=targetFile;
    VanityPreviewProgressParser parser(segment.count,native.checked);bool bad=false;std::string error;
    PosixProcessRunner runner;auto result=runner.run(selected->buildCommand(request),[&](const std::string& line){log<<line<<'\n';log.flush();if(bad)return;
     try{auto value=parser.parseLine(line);if(value&&line.starts_with("OPV_CLIENT PROGRESS ")){if(!progress(state.offset+decimal(value->keysChecked),value->speedMKeys,decimal(value->keysChecked)==segment.count))throw std::runtime_error("Coordinator progress rejected");std::cout<<"OPV_COORD PROGRESS "<<state.checked<<" total="<<plan.total<<"\n"<<std::flush;}}
     catch(const std::exception& e){bad=true;error=e.what();}
    },900,[&]{return bad||interrupted;});
    native=checkpoint(workspace/"checkpoint.txt",segment,segmentBinding);evidence(workspace,segment,native);
    if(native.status=="solution"){state.phase="solution";save();break;}
    if(bad){
     // Reconcile only against the adapter's durable, independently checked range.
     state.checked=state.offset+native.checked;save();int code=state.phase=="rejected"?2:3;
     publishOutcome(failureKind,failureOperation,code);std::cerr<<error<<'\n';return code;
    }
    if(result.exitCode==130||stopped()){state.checked=state.offset+native.checked;return pause();}
    need(result.started&&result.exitCode==0&&native.status=="complete"&&native.checked==segment.count,"Native segment did not complete exactly");
    ensureProgress(state.offset+segment.count,"Final segment progress refused",false);state.offset+=segment.count;save();
   }
   if(state.phase!="solution"){need(state.offset==plan.total&&state.checked==plan.total,"Incomplete assignment");state.phase="pending-complete";save();}
  }
  client::ClientExecutionState execution;execution.active=true;execution.assignmentId=state.a.assignmentId;execution.clientId=clientId;execution.puzzle=puzzle;execution.rangeId=state.a.rangeId;execution.pid=getpid();execution.engine="vanity-preview";execution.backend="cuda";execution.start=plan.start;execution.end=plan.end;execution.target=target;execution.workspace=root;
  auto solution=root/"solution-v1.txt";
  if(state.phase=="solution"){
   unsigned matches=0;std::string canonicalText;
   for(const auto& item:fs::recursive_directory_iterator(root)){
    if(item.path().filename()!="solutions.txt"||!fs::is_regular_file(item.path()))continue;
    std::istringstream in(read(item.path()));std::string marker,key,h,a,wif;
    while(in>>marker){need(bool(in>>key>>h>>a>>wif)&&marker=="MATCH"&&key==canonical(key)&&key>=plan.start&&key<=plan.end&&h==hash&&a==target&&wif.size()==52,"Invalid private solution record");++matches;canonicalText="OPENPUZZLE_SOLUTION_V1\naddress="+a+"\nprivate_key_wif="+wif+"\ncompression=compressed\n";}
   }
   need(matches==1,"Expected one protected match");if(fs::exists(solution))need(read(solution)==canonicalText,"Existing solution differs");else write(solution,canonicalText);
  }
  ClientRuntimeDependencies d;d.prepareSignals=[]{};d.stopRequested=[&]{return state.phase=="solution"||interrupted!=0;};d.safeStopRequested=[&]{return fs::exists(controls/"safe-stop");};d.clearSafeStop=[] {return true;};d.hasState=[&]{return state.phase!="done";};d.acquireRuntime=[] {return true;};d.releaseRuntime=[]{};d.removeState=[]()->bool{throw std::runtime_error("Preserved coordinator state must not be erased");};d.stopExecution=[](const auto&){return true;};
  d.finalKeysChecked=[&](const auto&){return std::to_string(state.checked);};
  d.reportSolution=[&](const auto&,const auto& assignment,const auto& client,std::string& error){bool ok=http.reportSolution(assignment,client);error=http.lastError();if(ok){state.phase="reported";save();}return ok;};
  d.finalizeAssignment=[](const auto&,const auto&,const auto&,int,const auto&,const auto&,auto&)->client::AssignmentUploadStatus{throw std::runtime_error("Unexpected incomplete finalization");};
  d.heartbeat=[&](const auto&){client::ClientHeartbeatResult r;r.success=heartbeat("running");r.error=http.lastError();return r;};
  d.sleep=[](auto){throw std::runtime_error("Bounded coordinator retry exhausted; resume preserved state");};
  d.sync=[&](const auto&){client::ExecutionSyncResult r;r.hasState=true;r.state=execution;
   if(state.phase=="solution"){r.solutionFound=true;r.solutionPath=solution;return r;}
   need(state.phase=="pending-complete"&&state.checked==plan.total,"Completion without exact assignment coverage");
   bool ok=false;for(int i=0;i<3&&!ok;++i){
    ok=http.complete(state.a.assignmentId,clientId,0,"completed",std::to_string(plan.total));
    // A lost completion reply may already have committed. Do not replay it here.
    if(!ok&&(transportStatus==0||rejected(http.lastErrorCode())||transportKind!="network"||interrupted))break;
    if(!ok&&i<2)backoff(i);
   }
   if(!ok){failureKind=rejected(http.lastErrorCode())?"lease":transportKind;failureOperation="complete";
    if(failureKind=="lease"){state.phase="rejected";save();}}
   r.hasExitCode=true;r.exitCode=0;r.completionUploaded=ok;r.stateRemoved=ok;r.completionError=http.lastError();r.completionStatus=ok?client::AssignmentUploadStatus::Uploaded:client::AssignmentUploadStatus::PermanentFailure;
   if(ok){state.phase="done";save();}return r;
  };
  ClientRuntime runtime(d);int code=runtime.run(server,state.a.assignmentId,clientId,root);
  if(state.phase=="done")publishOutcome("done","complete",code);
  else if(state.phase=="pending-complete"||state.phase=="rejected")publishOutcome(failureKind,failureOperation,code);
  std::cout<<"OPV_COORD RESULT "<<state.phase<<" checked="<<state.checked<<" launches="<<state.launches<<" next_claim_blocked=1\n";return code;
 }catch(const std::exception& error){std::cerr<<"Vanity coordinator refused: "<<error.what()<<'\n';return 2;}
}
}
