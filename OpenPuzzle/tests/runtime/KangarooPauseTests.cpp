#include "openpuzzle/runtime/KangarooPause.hpp"
#include <iostream>
#include <sys/wait.h>

using namespace openpuzzle;
namespace {
volatile sig_atomic_t stopping = 0;
void stop(int) { stopping = 1; }
void require(bool ok, const char* error) { if (!ok) throw std::runtime_error(error); }
void put(const std::filesystem::path& path, const std::string& data) {
  require(writePrivateFileAtomically(path, data), "fixture write failed");
}
int engine(const std::string& workspace, bool fail) {
  std::signal(SIGTERM, stop);
  put(std::filesystem::path(workspace)/"ready", "ready");
  while (!stopping) ::usleep(10000);
  // Saving takes longer than the ordinary stopper's two-second grace period.
  ::usleep(2300000);
  if (fail) { std::cout << "Checkpoint saving failed.\n" << std::flush; return 1; }
  std::string data(200, 'x'); data.replace(0,8,std::string("OPZKCP1\0",8));
  put(std::filesystem::path(workspace)/"kangaroo.checkpoint", data);
  std::cout << "Checkpoint saved successfully.\nExit completed.\n" << std::flush;
  return 0;
}
std::string quote(const std::string& s) {
  std::string r="'"; for(char c:s) r += c=='\'' ? "'\\''" : std::string(1,c); return r+"'";
}
}
int main(int argc, char** argv) {
  if (argc == 4 && std::string(argv[1]) == "--engine") return engine(argv[2], std::string(argv[3])=="fail");
  std::filesystem::path root;
  int child=0;
  try {
    char pattern[]="/tmp/openpuzzle-pause-XXXXXX";
    const auto folder=::mkdtemp(pattern); require(folder,"mkdtemp failed"); root=folder;
    ::umask(0077);
    ::setenv("HOME",root.c_str(),1); ::unsetenv("XDG_DATA_HOME"); ::unsetenv("OPENPUZZLE_EXECUTION_SLOT");
    require(ClientRuntimeControl::acquire(),"runtime ownership failed");
    if (argc==2 && std::string(argv[1])=="--markers-only") {
      client::ClientExecutionState s;
      s.active=true;s.assignmentId="fixture";s.clientId="client";s.pid=1;s.puzzle=140;s.rangeId=1;
      s.engine="Kangaroo";s.backend="CUDA";s.workspace=(root/"assignment").string();
      std::filesystem::create_directory(s.workspace);
      std::filesystem::permissions(s.workspace,std::filesystem::perms::owner_all);
      require(KangarooPause::request(s,"primary"),"durable intent write failed");
      require(KangarooPause::request(s,"primary"),"request retry failed");
      require(KangarooPause::requestedByThisRuntime(s)&&!KangarooPause::paused(s),"pause advertised before save");
      auto changed=s;changed.clientId="different";
      bool refused=false;try{KangarooPause::paused(changed);}catch(...){refused=true;}
      require(refused,"different assignment owner accepted");
      require(KangarooPause::clear(s),"intent clear failed");
      std::filesystem::create_symlink("elsewhere",std::filesystem::path(s.workspace)/"kangaroo.pause");
      refused=false;try{KangarooPause::request(s,"primary");}catch(...){refused=true;}
      require(refused,"symlink accepted");
      require(ClientRuntimeControl::release(),"release failed");
      std::filesystem::remove_all(root);
      std::cout<<"Kangaroo pause durable marker tests passed\n";return 0;
    }
    const auto executable=std::filesystem::canonical("/proc/self/exe");
    for(bool fail:{false,true}) {
      client::ClientExecutionState s;
      s.active=true;s.assignmentId=fail?"22222222-2222-4222-8222-222222222222":"11111111-1111-4111-8111-111111111111";
      s.clientId="client";s.puzzle=140;s.rangeId=12;s.engine="Kangaroo";s.backend="CUDA";
      s.kangarooWalkSeed="0123456789ABCDEF";s.start="100";s.end="1ff";
      s.workspace=(root/s.assignmentId).string();
      std::filesystem::create_directory(s.workspace);
      std::filesystem::permissions(s.workspace,std::filesystem::perms::owner_all);
      put(std::filesystem::path(s.workspace)/"kangaroo.log", "fixture\n");
      const auto command=quote(executable.string())+" --engine "+quote(s.workspace)+" "+(fail?"fail":"pass")+
        " >> "+quote(s.workspace+"/kangaroo.log")+"; rc=$?; printf '%s\\n' \"$rc\" > "+quote(s.workspace+"/exit.code");
      child=::fork();require(child>=0,"fork failed");
      if(!child){::setsid();::execl("/bin/sh","sh","-c",command.c_str(),static_cast<char*>(nullptr));::_exit(127);}
      s.pid=child;s.bootId=client::ClientStateStore::currentBootId();
      for(int i=0;i<100&&!s.processStartTime;++i){s.processStartTime=LinuxProcessIdentity::startTime(child).value_or(0);if(!s.processStartTime)::usleep(10000);}
      require(s.processStartTime>0,"missing process identity");
      for(int i=0;i<500&&!std::filesystem::exists(std::filesystem::path(s.workspace)/"ready");++i)::usleep(10000);
      require(std::filesystem::exists(std::filesystem::path(s.workspace)/"ready"),"native child not ready");
      require(KangarooPause::request(s,"primary"),"pause request failed");
      require(KangarooPause::request(s,"primary"),"repeated pause not idempotent");
      require(KangarooPause::requestedByThisRuntime(s),"runtime identity not retained");
      require(!KangarooPause::paused(s),"request prematurely advertised as paused");
      auto other=s;other.assignmentId="wrong";
      bool refused=false;try{KangarooPause::paused(other);}catch(...){refused=true;}
      require(refused,"mismatched assignment accepted");
      const auto started=std::chrono::steady_clock::now();
      require(KangarooPause::stopAndSave(s,executable,std::chrono::seconds(10))==!fail,"checkpoint acknowledgement not enforced");
      require(std::chrono::steady_clock::now()-started>=std::chrono::seconds(2),"save was cut short");
      int status=0;::waitpid(child,&status,0);child=0;
      require(WIFEXITED(status),"supervisor killed while saving");
      if(fail) {
        require(!KangarooPause::paused(s)&&KangarooPause::present(s),"failed save discarded intent");
        require(!KangarooPause::archiveExit(s),"failed save allowed resume");
      } else {
        require(KangarooPause::paused(s)&&KangarooPause::checkpointReady(s),"checkpoint not confirmed");
        require(KangarooPause::archiveExit(s),"paused exit not archived");
        require(!std::filesystem::exists(std::filesystem::path(s.workspace)/"exit.code"),"old exit can finalize resumed engine");
      }
      require(KangarooPause::clear(s),"intent clear failed");
      std::filesystem::create_symlink("kangaroo.log",std::filesystem::path(s.workspace)/"kangaroo.pause");
      refused=false;try{KangarooPause::request(s,"primary");}catch(...){refused=true;}
      require(refused,"symlink intent accepted");
    }
    require(ClientRuntimeControl::release(),"runtime release failed");
    std::filesystem::remove_all(root);
    std::cout<<"KangarooPauseTests passed: cooperative slow save, failed save, private intent, identity and exit preservation\n";
    return 0;
  } catch(const std::exception& e) {
    if(child){::kill(-child,SIGKILL);::waitpid(child,nullptr,0);}
    if(!root.empty())std::filesystem::remove_all(root);
    std::cerr<<e.what()<<'\n';return 1;
  }
}
