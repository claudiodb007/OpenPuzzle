// MIT License. Isolated OpenPuzzle process/protocol adapter; no HTTP client.
#include "openpuzzle/core/PosixProcessRunner.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/runtime/VanityRangeProtocol.hpp"
#include <csignal>
#include <fstream>
#include <iostream>
#include <chrono>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/prctl.h>
#include <unistd.h>

namespace fs=std::filesystem;
using namespace openpuzzle::vanityPreview;
static volatile sig_atomic_t interrupted=0;
static void cancel(int) {interrupted=1;}
static std::string quote(const std::string& s) {
  if(s.find('\0')!=std::string::npos)throw std::runtime_error("NUL in argument");
  std::string out="'";for(char c:s)out+=c=='\''?"'\\''":std::string(1,c);return out+"'";
}
static void privateDir(const fs::path& p) {
  if(fs::is_symlink(fs::symlink_status(p)))throw std::runtime_error("Symlink workspace refused");
  if(!fs::exists(p))fs::create_directory(p);
  if(!fs::is_directory(p))throw std::runtime_error("Not a workspace directory");
  fs::permissions(p,fs::perms::owner_all,fs::perm_options::replace);
}
static bool hasSolutions(const fs::path& root) {
  if(!fs::exists(root))return false;
  for(const auto& item:fs::directory_iterator(root)) {
    if(item.path().filename().string().rfind("attempt-",0)!=0)continue;
    auto p=item.path()/"solutions.txt";
    if(fs::is_symlink(fs::symlink_status(p)))return true;
    if(fs::is_regular_file(p) && fs::file_size(p)>0)return true;
  }
  return false;
}
struct Lock {
  int fd=-1;
  explicit Lock(const fs::path& p) {
    fd=open(p.c_str(),O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);
    if(fd<0 || flock(fd,LOCK_EX|LOCK_NB)!=0) {if(fd>=0)close(fd);fd=-1;throw std::runtime_error("Workspace already owned or lock unavailable");}
  }
  ~Lock(){if(fd>=0)close(fd);}
};
int main(int argc,char** argv) {
 try {
  // Explicit synthetic or public-target fixture gate. Production remains disabled.
  if(argc!=11&&argc!=13)throw std::runtime_error("Usage: ENGINE WORKSPACE START COUNT GRID KNOWN_KEYS exhaust|stop BINDING CONTROL_DIR --synthetic-only [--owner PID]");
  bool targetMode=std::string(argv[10])=="--targets-only";
  if(!targetMode&&std::string(argv[10])!="--synthetic-only")throw std::runtime_error("Explicit input gate required");
  Plan plan(argv[3],decimal(argv[4]),decimal(argv[5]));
  std::string mode=argv[7],binding=argv[8];
  if((mode!="exhaust"&&mode!="stop") || binding.size()!=64 || binding.find_first_not_of("0123456789abcdef")!=std::string::npos)
    throw std::runtime_error("Invalid mode/binding");
  fs::path engine=fs::absolute(argv[1]),root=fs::absolute(argv[2]),targets=fs::absolute(argv[6]),controls=fs::absolute(argv[9]);
  if(!fs::is_regular_file(engine)||!fs::is_regular_file(targets)||!fs::is_directory(controls))throw std::runtime_error("Missing engine/fixture/control directory");
  bool existed=fs::exists(root);privateDir(root);
  auto checkpoint=root/"checkpoint.txt";
  if(fs::is_symlink(fs::symlink_status(checkpoint)))throw std::runtime_error("Symlink checkpoint refused");
  if(existed&&!fs::exists(checkpoint)) {
    for(const auto& f:fs::directory_iterator(root))
      if(f.path().filename()!="owner.lock")throw std::runtime_error("Occupied workspace without checkpoint");
  }
  Lock lock(root/"owner.lock");
  uint64_t checked=0;std::string status="running";bool safe=false;
  if(fs::exists(checkpoint)) {
    if(!fs::is_regular_file(checkpoint)||fs::file_size(checkpoint)>4096)throw std::runtime_error("Invalid checkpoint file");
    std::ifstream file(checkpoint);std::string magic,b,s,count,blocks,m,progress,state,safeText,extra;
    if(!(file>>magic>>b>>s>>count>>blocks>>m>>progress>>state>>safeText) || (file>>extra) ||
       magic!="OPV-CHECKPOINT-1" || b!=binding || s!=plan.start || decimal(count)!=plan.count || decimal(blocks)!=plan.blocks || m!=mode ||
       (safeText!="0"&&safeText!="1"))throw std::runtime_error("Checkpoint binding/format mismatch; preserved");
    checked=decimal(progress);safe=safeText=="1";status=state;
    if(checked>plan.count)throw std::runtime_error("Checkpoint exceeds range");
    if(status!="running" && status!="interrupted" && status!="complete" && status!="solution" && status!="review" && status!="faulted")
      throw std::runtime_error("Unknown checkpoint state");
    if(status=="complete" || status=="solution" || status=="review" || status=="faulted" || checked==plan.count || hasSolutions(root)) {
      std::cout<<"OPV_CLIENT BLOCKED "<<status<<" checked="<<checked<<"\n";return 20;
    }
  }
  auto publish=[&] {
    std::string record="OPV-CHECKPOINT-1\n"+binding+" "+plan.start+" "+std::to_string(plan.count)+" "+std::to_string(plan.blocks)+" "+mode+"\n"+
      std::to_string(checked)+" "+status+" "+(safe?"1":"0")+"\n";
    if(!openpuzzle::writePrivateFileAtomically(checkpoint,record))throw std::runtime_error("Checkpoint atomic write failed");
    int fd=open(root.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    bool ok=fd>=0 && fsync(fd)==0;if(fd>=0)close(fd);
    if(!ok)throw std::runtime_error("Checkpoint directory fsync failed");
  };
  signal(SIGTERM,cancel);signal(SIGINT,cancel);signal(SIGPIPE,SIG_IGN);
  if(argc==13){
    const auto owner=decimal(argv[12]);
    if(std::string(argv[11])!="--owner"||owner<2||owner>uint64_t(INT32_MAX)||prctl(PR_SET_PDEATHSIG,SIGTERM)!=0||getppid()!=pid_t(owner))
      throw std::runtime_error("Local integration owner changed");
  }
  openpuzzle::PosixProcessRunner runner;
  std::string identity;
  auto id=runner.run("exec "+quote(engine.string())+" --identity",[&](const std::string& l){identity+=l;},20);
  if(!id.started||id.exitCode!=0||identity!=(targetMode?"OpenPuzzle-VanityRange 0.2.0 CUDA target protocol 1":"OpenPuzzle-VanityRange 0.1.0 CUDA synthetic protocol 1"))throw std::runtime_error("Unrecognized experimental engine");
  uint64_t resumed=checked;
  Protocol protocol(Plan(add(plan.start,resumed),plan.count-resumed,plan.blocks));
  std::string token=std::to_string(getpid())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  auto attempt=root/("attempt-"+token);if(!fs::create_directory(attempt))throw std::runtime_error("Attempt collision");privateDir(attempt);
  auto solutions=attempt/"solutions.txt";
  std::ofstream log(attempt/"engine.log");fs::permissions(attempt/"engine.log",fs::perms::owner_read|fs::perms::owner_write);
  status="running";publish();
  std::string command="exec "+quote(engine.string());
  for(const auto& a:std::vector<std::string>{targetMode?"--target-file":"--synthetic-only",protocol.plan.start,std::to_string(protocol.plan.count),std::to_string(plan.blocks),targets.string(),solutions.string(),mode,std::to_string(getpid())})command+=" "+quote(a);
  bool bad=false,stopRequested=false;std::string error;
  auto shouldStop=[&] {
    try {
      if(fs::exists(controls/"safe-stop"))safe=true;
      if(interrupted || fs::exists(controls/"cancel"))stopRequested=true;
    } catch(const std::exception& e){bad=true;error=e.what();}
    return bad||stopRequested;
  };
  auto result=runner.run(command,[&](const std::string& line){
    log<<line<<"\n";log.flush();
    if(bad)return;
    try {
      protocol.consume(line);checked=resumed+protocol.checked;
      if(line.rfind("OPV1 PROGRESS ",0)==0) {publish();std::cout<<"OPV_CLIENT PROGRESS "<<checked<<"\n"<<std::flush;if(!std::cout)interrupted=1;}
    } catch(const std::exception& e){bad=true;error=e.what();}
  },900,shouldStop);
  if(bad)status="faulted";
  else if(protocol.solution && result.exitCode==10 && fs::is_regular_file(solutions) && fs::file_size(solutions)>0)status="solution";
  else if(protocol.completed(result.exitCode) && !stopRequested)status="complete";
  else if(hasSolutions(root) || checked==plan.count)status="review";
  else status="interrupted";
  // Safe stop is sticky across attempts and blocks the next assignment.
  if(fs::exists(controls/"safe-stop"))safe=true;
  publish();
  std::cout<<"OPV_CLIENT RESULT "<<status<<" checked="<<checked<<" resumed="<<resumed<<" safe_stop="<<(safe?1:0)<<" next_blocked="<<((safe||status!="complete")?1:0)<<"\n";
  if(!error.empty())std::cerr<<"Protocol refused: "<<error<<"\n";
  if(status=="complete")return 0;if(status=="solution")return 10;if(status=="interrupted")return 130;return 2;
 }catch(const std::exception& e){std::cerr<<"Preview refused: "<<e.what()<<"\n";return 2;}
}
