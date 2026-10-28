#include "openpuzzle/engines/VanityRunProfile.hpp"
#include "VanityCommonEngine.hpp"
#include "LoopbackHttp.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/PosixProcessRunner.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <locale>
#include <regex>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace openpuzzle {
namespace {
namespace fs=std::filesystem;
using namespace vanityPreview;
void need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::string invoke(const std::string& command){PosixProcessRunner runner;std::string result;bool large=false;
  auto r=runner.run(command,[&](const auto& line){result+=line+'\n';large=result.size()>65536;},20,[&]{return large;});
  need(r.started&&r.exitCode==0&&!large,"Vanity profile/hardware verification failed");return result;
}
std::string hashFile(const fs::path& path){need(path.is_absolute()&&fs::canonical(path)==path&&fs::is_regular_file(fs::symlink_status(path)),"Vanity profile file missing/unsafe");
  return invoke("sha256sum < "+shellQuote(path.string())).substr(0,64);
}
std::string privateRead(const fs::path& path){struct stat st{};
  need(path.is_absolute()&&fs::canonical(path)==path&&lstat(path.c_str(),&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==getuid()&&!(st.st_mode&0077)&&st.st_size<=65536,"Private Vanity profile/record required");
  std::ifstream input(path);return std::string((std::istreambuf_iterator<char>(input)),{});
}
void immutableProfileBinding(const fs::path& path,const std::string& text){
  if(fs::exists(path)||fs::is_symlink(path))need(privateRead(path)==text,"Vanity workspace profile/server changed");
  else need(writePrivateFileAtomically(path,text),"Cannot bind common Vanity workspace");
}
std::vector<std::string> fields(const std::string& line){std::istringstream input(line);std::vector<std::string> result;std::string value;
  while(std::getline(input,value,',')){auto first=value.find_first_not_of(" \t\r"),last=value.find_last_not_of(" \t\r");result.push_back(first==std::string::npos?"":value.substr(first,last-first+1));}return result;
}
}
bool VanityRunProfile::configured(){const char* path=getenv("OPENPUZZLE_VANITY_COMMON_PROFILE");return path&&*path;}
VanityRunProfile VanityRunProfile::load(){
  need(configured(),"Verified Vanity run profile required; production remains disabled");
  fs::path path=getenv("OPENPUZZLE_VANITY_COMMON_PROFILE");auto text=privateRead(path);
  std::istringstream input(text);input.imbue(std::locale::classic());std::string magic,cliHash,local,extra;VanityRunProfile p;unsigned files=0;
  need(bool(input>>magic>>cliHash>>local>>std::quoted(p.executables.engine)>>std::quoted(p.executables.adapter)
      >>p.executables.engineSha256>>p.executables.adapterSha256>>std::quoted(p.uuid)>>std::quoted(p.gpu)
      >>std::quoted(p.driver)>>p.grid>>p.planningMKeys>>p.powerLimit>>files)
      &&magic=="OPV-RUN-PROFILE-1"&&local=="local-only"&&files>0&&files<=32,"Invalid bound Vanity run profile");
  need(std::regex_match(p.uuid,std::regex("GPU-[0-9a-fA-F-]{36}"))&&!p.gpu.empty()&&!p.driver.empty()
       &&p.grid&&p.grid<=4096&&!(p.grid&(p.grid-1))&&std::isfinite(p.planningMKeys)&&p.planningMKeys>0&&p.planningMKeys<1000000
       &&std::isfinite(p.powerLimit)&&p.powerLimit>0&&p.powerLimit<1000,"Invalid measured Vanity launch context");
  need(hashFile(fs::canonical("/proc/self/exe"))==cliHash,"Vanity profile CLI binding changed");
  for(unsigned i=0;i<files;++i){std::string name,hash;need(bool(input>>std::quoted(name)>>hash)
    &&std::regex_match(hash,std::regex("[0-9a-f]{64}"))&&hashFile(name)==hash,"Measured Vanity evidence changed");}
  need(!(input>>extra),"Trailing Vanity profile data");
  need(VanityCommonEngine(p.executables).prepare(),"Verified Vanity binaries unavailable/changed");
  p.fingerprint=hashFile(path);return p;
}
void VanityRunProfile::requireLocalServer(const std::string& server){vanityPreview::LoopbackHttp local(server);(void)local;}
std::uint64_t VanityRunProfile::maximumKeys()const{return std::min<std::uint64_t>(std::uint64_t(1)<<40,std::uint64_t(grid)*256*1024*8192);}
std::uint64_t VanityRunProfile::requestedKeys(int minutes)const{
  need(minutes>=1&&minutes<=360,"Invalid Vanity duration");double desired=planningMKeys*1000000*60*minutes;
  return desired>=double(maximumKeys())?maximumKeys():std::max<std::uint64_t>(1,std::uint64_t(std::ceil(desired)));
}
void VanityRunProfile::validateHardware(int device,bool idle)const{
  need(device>=0,"Invalid Vanity physical device");
  std::istringstream rows(invoke("nvidia-smi --query-gpu=index,uuid,name,driver_version,power.limit,temperature.gpu --format=csv,noheader,nounits"));
  std::string line;unsigned matches=0;
  while(std::getline(rows,line)){auto values=fields(line);if(values.size()!=6||values[1]!=uuid)continue;
    need(decimal(values[0])==std::uint64_t(device)&&values[2]==gpu&&values[3]==driver,"Measured Vanity physical GPU/driver differs");
    auto power=std::stod(values[4]),temperature=std::stod(values[5]);
    need(std::isfinite(power)&&std::abs(power-powerLimit)<=.5&&std::isfinite(temperature)&&temperature<85,"Measured Vanity power/thermal context differs");++matches;
  }
  need(matches==1,"Measured Vanity GPU is missing/ambiguous");
  if(idle){std::istringstream processes(invoke("nvidia-smi --query-compute-apps=pid,gpu_uuid --format=csv,noheader,nounits"));
    while(std::getline(processes,line)){auto values=fields(line);need(values.size()!=2||values[1]!=uuid,"Selected Vanity GPU is busy");}}
  need(setenv("CUDA_VISIBLE_DEVICES",uuid.c_str(),1)==0&&setenv("CUDA_DEVICE_ORDER","PCI_BUS_ID",1)==0,"Cannot isolate measured Vanity GPU");
}
void VanityRunProfile::validatePuzzle(const PuzzleExecutionMetadata& m)const{
  need(m.puzzle>=65&&m.puzzle<=80&&m.searchMode=="linear"&&m.publicKey.empty()&&(m.requiredBackend.empty()||m.requiredBackend=="cuda"),"Puzzle is outside validated Vanity capabilities");
  auto separator=m.keyspace.find(':');need(separator!=std::string::npos,"Explicit Vanity puzzle bounds required");
  auto start=canonical(m.keyspace.substr(0,separator)),end=canonical(m.keyspace.substr(separator+1));
  need(start>=canonical("10000000000000000")&&start<=end&&end<canonical("100000000000000000000")&&!m.address.empty(),"Puzzle exceeds measured Vanity scalar width");
}
void VanityRunProfile::validateAssignment(const client::RangeAssignment& a,const PuzzleExecutionMetadata& m)const{
  validatePuzzle(m);need(std::regex_match(a.assignmentId,std::regex("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-4[0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}"))&&a.valid()&&a.puzzle==m.puzzle&&a.target==m.address&&a.searchMode=="linear"&&a.publicKey.empty()
    &&(a.requiredBackend.empty()||a.requiredBackend=="cuda"),"Vanity assignment differs from validated puzzle/backend");
  auto sep=m.keyspace.find(':');auto start=canonical(a.start),end=canonical(a.end);
  need(start>=canonical(m.keyspace.substr(0,sep))&&end<=canonical(m.keyspace.substr(sep+1))&&start<=end
    &&inclusiveCount(start,end)<=maximumKeys(),"Vanity assignment escapes validated bounds/count");
}
void VanityRunProfile::bindWorkspace(const fs::path& workspace,const std::string& server)const{
  requireLocalServer(server);immutableProfileBinding(workspace/"vanity-run-profile.txt",fingerprint+'\n'+server+'\n');
}
void VanityRunProfile::requireWorkspaceBinding(const fs::path& workspace,const std::string& server)const{
  need(privateRead(workspace/"vanity-run-profile.txt")==fingerprint+'\n'+server+'\n',"Common Vanity recovery profile/server changed");
}
std::uint64_t VanityRunProfile::prepareRecovery(const client::ClientExecutionState& s,const fs::path& expected,const std::string& server)const{
  need(s.valid()&&s.engine=="Vanity"&&s.backend=="CUDA"&&s.workspace==expected.string()
    &&expected.filename()==s.assignmentId&&fs::canonical(expected)==expected,"Common Vanity recovery state/workspace differs");
  need(privateRead(expected/"vanity-run-profile.txt")==fingerprint+'\n'+server+'\n',"Common Vanity recovery profile/server changed");
  EngineLaunchRequest request;request.engine="Vanity";request.backend="CUDA";request.device=0;
  request.blocks=grid;request.threads=256;request.points=1024;request.targets={s.target};request.startKey=s.start;request.endKey=s.end;request.workspace=s.workspace;
  (void)VanityCommonEngine(executables).buildCommand(request);
  auto checked=vanityCommonCheckedCoverage(s.workspace);need(checked.has_value(),"Common Vanity native coverage requires review");return *checked;
}
}
