// MIT. One pinned assignment through a numeric-loopback fixture. Never production.
#include "VanityPreviewEngine.hpp"
#include "LoopbackHttp.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/PosixProcessRunner.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include "openpuzzle/client/SolutionExporter.hpp"
#include "openpuzzle/runtime/ClientRuntime.hpp"
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
namespace {
using namespace openpuzzle;
using namespace openpuzzle::vanityPreview;
namespace fs=std::filesystem;
volatile sig_atomic_t cancelled=0;
void onSignal(int){cancelled=1;}
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
void directory(const fs::path& p){require(!fs::is_symlink(fs::symlink_status(p)),"Symlink directory refused");if(!fs::exists(p))fs::create_directory(p);require(fs::is_directory(p),"Missing directory");fs::permissions(p,fs::perms::owner_all);}
void write(const fs::path& p,const std::string& value){
 require(!fs::is_symlink(fs::symlink_status(p)),"Symlink record refused");require(writePrivateFileAtomically(p,value),"Private record write failed");
 int fd=open(p.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);bool ok=fd>=0&&fsync(fd)==0;if(fd>=0)close(fd);require(ok,"Record directory sync failed");
}
std::string read(const fs::path& p,size_t limit=8192){require(!fs::is_symlink(fs::symlink_status(p))&&fs::is_regular_file(p)&&fs::file_size(p)<=limit,"Missing/unsafe record");std::ifstream f(p);std::string s((std::istreambuf_iterator<char>(f)),{});require(!f.bad(),"Unreadable record");return s;}
std::string invoke(const std::string& cmd){PosixProcessRunner runner;std::string out;bool large=false;auto r=runner.run(cmd,[&](const auto& line){if(out.size()+line.size()>8192)large=true;else out+=line+'\n';},10,[&]{return large;});require(!large&&r.started&&r.exitCode==0,"Target identity/decoding failed");return out;}
struct Lock {int fd;explicit Lock(const fs::path& p){fd=open(p.c_str(),O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0||flock(fd,LOCK_EX|LOCK_NB)){if(fd>=0)close(fd);throw std::runtime_error("HTTP preview already owned");}}~Lock(){close(fd);}};
struct State {std::string phase="idle",assignment;unsigned launches=0;int range=0;};
struct Checkpoint {uint64_t checked=0;std::string status;bool safe=false;};
void flatClaimEnvelope(const std::string& body){
 // The local fixture has a deliberately small flat schema. Reject duplicate
 // fields and trailing garbage before using the production claim parser.
 std::istringstream in(body);char c;require(bool(in>>c)&&c=='{',"Malformed claim JSON");std::set<std::string> fields;
 const std::set<std::string> allowed={"assignment_id","puzzle","range_id","target","search_mode","required_backend","start","end","public_key","available"};
 auto string=[&]{std::string out;while(in.get(c)){if(c=='"')return out;require(static_cast<unsigned char>(c)>=32&&c!='\\',"Unsupported claim JSON string");out+=c;}throw std::runtime_error("Unterminated claim JSON string");};
 while(true){require(bool(in>>c)&&c=='"',"Malformed claim JSON key");auto key=string();require(allowed.count(key)&&fields.insert(key).second,"Unknown/duplicate claim field");require(bool(in>>c)&&c==':'&&bool(in>>c),"Malformed claim value");
  if(c=='"'){require(key!="puzzle"&&key!="range_id"&&key!="available","Wrong claim field type");(void)string();}
  else{std::string token(1,c);while(in.peek()!=EOF&&in.peek()!=','&&in.peek()!='}'&&!std::isspace(in.peek()))token+=char(in.get());
   require((key=="available"&&(token=="true"||token=="false"))||((key=="puzzle"||key=="range_id")&&!token.empty()&&token.size()<=9&&token[0]!='0'&&token.find_first_not_of("0123456789")==std::string::npos),"Wrong claim scalar type");}
  require(bool(in>>c),"Truncated claim JSON");if(c=='}')break;require(c==',',"Malformed claim separator");
 }
 require(!(in>>c),"Trailing claim JSON");
}
}

int runVanityHttpAssignment(const std::vector<std::string>& args){
 try{
  require(args.size()==12,"Internal HTTP preview arguments invalid");
  const std::string engine=args[0],adapter=args[1],server=args[4],binding=args[11];
  const fs::path root=args[2],controls=args[3],workspace=root/"assignment",targetFile=root/"targets.txt";
  LoopbackHttp http(server);Plan plan(args[5],decimal(args[6]),decimal(args[7]));
  require(args[8]=="p2pkh"||args[8]=="hash160","Unknown target kind");
  require(args[9].size()<=40&&args[9].find_first_not_of("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz0") == std::string::npos,"Malformed public target");
  require(binding.size()==64&&binding.find_first_not_of("0123456789abcdef")==std::string::npos,"Malformed HTTP binding");
  directory(root);directory(controls);Lock owner(root/"owner.lock");
  cancelled=0;signal(SIGINT,onSignal);signal(SIGTERM,onSignal);
  State state;auto stateFile=root/"http-state.txt";bool existing=fs::exists(stateFile);
  auto save=[&]{write(stateFile,"OPV-HTTP-1\n"+binding+"\n"+state.phase+" "+(state.assignment.empty()?"-":state.assignment)+" "+std::to_string(state.range)+" "+std::to_string(state.launches)+"\n");};
  if(existing){std::istringstream in(read(stateFile));std::string magic,id,extra;require(bool(in>>magic>>id>>state.phase>>state.assignment>>state.range>>state.launches)&&!(in>>extra)&&magic=="OPV-HTTP-1"&&id==binding,"HTTP identity/state changed; preserved");if(state.assignment=="-")state.assignment.clear();
   if(state.phase=="done"||state.phase=="rejected"||state.phase=="claiming"||state.phase=="faulted"||state.phase=="stopped"||state.phase=="reported"){std::cout<<"OPV_HTTP BLOCKED "<<state.phase<<'\n';return 20;}
   require(state.phase=="idle"||state.phase=="assigned"||state.phase=="pending-complete"||state.phase=="solution","Unknown HTTP phase");
  }else{for(const auto& e:fs::directory_iterator(root))require(e.path().filename()=="owner.lock","Occupied HTTP root; preserved");save();}
  auto targetText="OPV-TARGETS-1\n"+args[8]+" "+args[9]+"\n";
  if(fs::exists(targetFile))require(read(targetFile)==targetText,"Target input changed; preserved");else write(targetFile,targetText);
  require(invoke(shellQuote(engine)+" --identity")=="OpenPuzzle-VanityRange 0.2.0 CUDA target protocol 1\n","Public-target engine required");
  std::istringstream info(invoke(shellQuote(engine)+" --target-info "+shellQuote(targetFile.string())));std::string hash,address,extra;
  require(bool(info>>hash>>address)&&!(info>>extra)&&hash.size()==40&&hash.find_first_not_of("0123456789abcdef")==std::string::npos&&address.size()>=26&&address.size()<=35&&address[0]=='1'&&address.find_first_not_of("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz")==std::string::npos,"Invalid decoded target");
  const std::string client="opv-local-"+binding.substr(0,24);
  auto send=[&](const std::string& path,const std::string& body){
   LocalHttpResponse response{503,""};
   for(int attempt=0;attempt<3;++attempt){
    try{response=http.post(path,body);}catch(const std::exception&){if(path=="/api/range/claim")throw;if(attempt==2)throw;std::this_thread::sleep_for(std::chrono::milliseconds(50));continue;}
    if(response.status<500||attempt==2)return response;std::this_thread::sleep_for(std::chrono::milliseconds(50));
   }return response;
  };
  auto accepted=[&](const LocalHttpResponse& r){
   if(r.status==409||r.status==410){state.phase="rejected";save();throw std::runtime_error("Local lease rejected; no next assignment");}
   // This fixture intentionally uses one exact acknowledgement envelope.
   require(r.status==200&&r.body=="{\"success\":true}","Local request not acknowledged; state preserved");
  };
  if(state.phase=="idle"){
   if(fs::exists(controls/"safe-stop")||fs::exists(controls/"cancel")||cancelled){state.phase="stopped";save();return 130;}
   state.phase="claiming";save(); // An ambiguous claim must not issue another claim on restart.
   auto r=send("/api/range/claim","{\"client_id\":\""+client+"\",\"puzzle\":20,\"execution_slot\":\"cuda-0\",\"backend\":\"cuda\",\"target_duration_minutes\":1,\"speed_mkeys\":0}");
   require(r.status==200,"Local claim failed; request preserved for review");
   flatClaimEnvelope(r.body);
   auto claim=client::HttpRangeClient::parseClaimResult(r.body);require(claim.assigned(),"Invalid/missing local assignment");auto a=*claim.assignment;
   require(a.assignmentId.size()<=64&&!a.assignmentId.empty()&&a.assignmentId.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos,"Unsafe assignment identifier");
   require(a.puzzle==20&&a.rangeId>0&&a.target==address&&a.searchMode=="linear"&&a.publicKey.empty()&&a.requiredBackend=="cuda"&&canonical(a.start)==plan.start&&canonical(a.end)==plan.end,"Server assignment differs from pinned local target/range");
   state.assignment=a.assignmentId;state.range=a.rangeId;state.phase="assigned";save();
  }
  require(!state.assignment.empty()&&state.assignment.size()<=64&&state.assignment.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos&&state.range>0,"Invalid saved assignment");
  const std::string ids="\"assignment_id\":\""+state.assignment+"\",\"client_id\":\""+client+"\"";
  auto checkpoint=[&]{Checkpoint c;if(!fs::exists(workspace/"checkpoint.txt"))return c;
   std::istringstream in(read(workspace/"checkpoint.txt"));std::string magic,b,start,count,grid,mode,checked,safe,tail;
   require(bool(in>>magic>>b>>start>>count>>grid>>mode>>checked>>c.status>>safe)&&!(in>>tail)&&magic=="OPV-CHECKPOINT-1"&&b==binding&&start==plan.start&&decimal(count)==plan.count&&decimal(grid)==plan.blocks&&mode=="stop"&&(safe=="0"||safe=="1"),"Native checkpoint differs from HTTP assignment");
   c.checked=decimal(checked);c.safe=safe=="1";require(c.checked<=plan.count,"Checkpoint exceeds assignment");return c;
  };
  auto progress=[&](uint64_t n){accepted(send("/api/range/progress","{"+ids+",\"status\":\"running\",\"speed_mkeys\":0,\"keys_checked\":\""+std::to_string(n)+"\"}"));};
  auto c=checkpoint();
  // Pending completion retries only completion, not a lease probe that could reject an already committed ACK.
  if(c.status=="complete"&&c.checked==plan.count&&state.phase=="assigned"){state.phase="pending-complete";save();}
  if(c.status=="solution"&&state.phase=="assigned"){state.phase="solution";save();}
  if(state.phase=="assigned"){
   require(c.status.empty()||c.status=="running"||c.status=="interrupted","Native state needs review; no launch");
   if(fs::exists(controls/"cancel")||cancelled)return 130;
   if(c.safe&&c.checked==0){state.phase="stopped";save();return 20;}
   progress(c.checked); // Validate the lease before the first kernel and every resume.
   AdapterConfig config{adapter,engine,targetFile.string(),binding,controls.string(),"stop",true};
   EngineRegistry registry;EngineFactory factory;registerVanityPreview(registry,factory,config,true);auto selected=factory.create("vanity-preview",engine);require(selected->prepare(),"Engine unavailable");
   EngineLaunchRequest request;request.engine="vanity-preview";request.backend="cuda";request.device=0;request.blocks=int(plan.blocks);request.threads=256;request.points=1024;request.startKey=plan.start;request.endKey=plan.end;request.workspace=workspace;request.targetFile=targetFile;
   VanityPreviewProgressParser parser(plan.count,c.checked);bool bad=false;std::string error;++state.launches;save();
   auto logFile=root/("launch-"+std::to_string(state.launches)+".log");std::ofstream log(logFile);fs::permissions(logFile,fs::perms::owner_read|fs::perms::owner_write);
   PosixProcessRunner runner;auto result=runner.run(selected->buildCommand(request),[&](const auto& line){
    log<<line<<'\n';log.flush();if(bad)return;
    try{auto p=parser.parseLine(line);if(p&&line.rfind("OPV_CLIENT PROGRESS ",0)==0)progress(decimal(p->keysChecked));}
    catch(const std::exception& e){bad=true;error=e.what();}
   },900,[&]{return bad||cancelled;});
   c=checkpoint();
   if(c.status=="solution"){state.phase="solution";save();}
   else if(bad){std::cerr<<"HTTP progress refused: "<<error<<'\n';return state.phase=="rejected"?2:3;}
   else if(result.exitCode==130||cancelled)return 130;
   else if(result.started&&result.exitCode==0&&c.status=="complete"&&c.checked==plan.count){state.phase="pending-complete";save();}
   else{state.phase="faulted";save();throw std::runtime_error("Incomplete/invalid engine result");}
  }
  client::ClientExecutionState execution;execution.active=true;execution.assignmentId=state.assignment;execution.clientId=client;execution.puzzle=20;execution.rangeId=state.range;execution.pid=getpid();execution.engine="vanity-preview";execution.backend="cuda";execution.start=plan.start;execution.end=plan.end;execution.target=address;execution.workspace=workspace;
  auto solution=root/"solution-v1.txt";
  if(state.phase=="solution"){
   unsigned matches=0;std::string canonicalText;
   for(const auto& entry:fs::directory_iterator(workspace)){
    if(entry.path().filename().string().rfind("attempt-",0)!=0)continue;auto p=entry.path()/"solutions.txt";if(!fs::exists(p))continue;
    std::istringstream in(read(p));std::string marker,key,h,a,wif;
    while(in>>marker){require(bool(in>>key>>h>>a>>wif)&&marker=="MATCH"&&key==canonical(key)&&key>=plan.start&&key<=plan.end&&h==hash&&a==address&&wif.size()==52&&wif.find_first_not_of("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz")==std::string::npos,"Invalid private match record");
     ++matches;canonicalText="OPENPUZZLE_SOLUTION_V1\naddress="+a+"\nprivate_key_wif="+wif+"\ncompression=compressed\n";}
   }
   require(matches==1,"Expected exactly one verified target match");if(fs::exists(solution))require(read(solution)==canonicalText,"Existing solution differs; preserved");else write(solution,canonicalText);
   auto exported=client::SolutionExporter::exportSolution(execution,solution);
   require(exported.success,"Protected export failed; raw solution preserved and report deferred");
  }
  ClientRuntimeDependencies d;
  d.heartbeat=[](const auto&){throw std::runtime_error("Unexpected monitor heartbeat");return client::ClientHeartbeatResult{};};
  d.finalizeAssignment=[](const auto&,const auto&,const auto&,int,const auto&,const auto&,auto&)->client::AssignmentUploadStatus{throw std::runtime_error("Unexpected external finalization");};
  d.finalKeysChecked=[&](const auto&){return std::to_string(c.checked);};d.removeState=[]()->bool{throw std::runtime_error("Preserved HTTP state must not be removed");};
  d.hasState=[] {return true;};d.acquireRuntime=[] {return true;};d.releaseRuntime=[]{};d.clearSafeStop=[] {return true;};
  d.stopRequested=[] {return false;};d.safeStopRequested=[&]{return c.safe;};d.prepareSignals=[]{};d.stopExecution=[](const auto&){return true;};d.sleep=[](auto){throw std::runtime_error("Unexpected unbounded HTTP monitor retry");};
  d.sync=[&](const auto&){client::ExecutionSyncResult r;r.hasState=true;r.state=execution;
   if(state.phase=="solution"){r.solutionFound=true;r.solutionPath=solution;return r;}
   require(state.phase=="pending-complete"&&c.status=="complete"&&c.checked==plan.count,"Completion lacks exact proof");
   accepted(send("/api/range/complete","{"+ids+",\"exit_code\":0,\"status\":\"completed\",\"keys_checked\":\""+std::to_string(c.checked)+"\"}"));
   state.phase="done";save();r.hasExitCode=true;r.exitCode=0;r.completionStatus=client::AssignmentUploadStatus::Uploaded;r.completionUploaded=true;r.stateRemoved=true;return r;
  };
  d.reportSolution=[&](const auto&,const auto&,const auto&,auto& error){try{auto r=send("/api/puzzle/report-solution",client::HttpRangeClient::buildSolutionReportPayload(state.assignment,client));require(r.status==200&&r.body=="{\"success\":true}","Solution report not acknowledged; local solution preserved");state.phase="reported";save();return true;}catch(const std::exception& e){error=e.what();return false;}};
  ClientRuntime runtime(d);int result=runtime.run(server,state.assignment,client,workspace.string());
  if(state.phase=="reported"||state.phase=="solution"){
   const char* home=getenv("HOME");require(home&&*home,"Missing isolated export home");auto wallet=fs::path(home)/"OpenPuzzle-Solutions"/"Puzzle-20"/state.assignment/"wallet-import.txt";
   require(fs::is_regular_file(wallet)&&!fs::is_symlink(fs::symlink_status(wallet))&&(fs::status(wallet).permissions()&(fs::perms::group_all|fs::perms::others_all))==fs::perms::none,"Protected export missing");
  }
  std::cout<<"OPV_HTTP RESULT "<<state.phase<<" launches="<<state.launches<<" next_claim_blocked=1\n";return result;
 }catch(const std::exception& e){std::cerr<<"Local HTTP preview refused: "<<e.what()<<'\n';return 2;}
}
