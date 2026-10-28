#include "openpuzzle/engines/VanityRunProfile.hpp"
#include "VanityCommonEngine.hpp"
#include "openpuzzle/client/HttpRequestPolicy.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/PosixProcessRunner.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"
#include "openpuzzle/tools/ToolManager.hpp"
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
  need(r.started&&r.exitCode==0&&!large,"Vanity executable verification failed");return result;
}
std::string hashFile(const fs::path& path){
  need(path.is_absolute()&&fs::canonical(path)==path&&fs::is_regular_file(fs::symlink_status(path)),"Vanity executable missing/unsafe");
  return invoke("sha256sum < "+shellQuote(path.string())).substr(0,64);
}
std::string privateRead(const fs::path& path){struct stat st{};
  need(path.is_absolute()&&fs::canonical(path)==path&&lstat(path.c_str(),&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==getuid()&&!(st.st_mode&0077)&&st.st_size<=65536,"Private Vanity record required");
  std::ifstream input(path);return std::string((std::istreambuf_iterator<char>(input)),{});
}
void immutableProfileBinding(const fs::path& path,const std::string& text){
  if(fs::exists(path)||fs::is_symlink(path))need(privateRead(path)==text,"Vanity workspace executable/server changed");
  else need(writePrivateFileAtomically(path,text),"Cannot bind common Vanity workspace");
}
std::optional<GpuProfileRecord> profileFor(const std::string& gpu){
  const char* home=std::getenv("HOME");if(!home||!*home)return std::nullopt;
  Database database;if(!database.open((fs::path(home)/".local/share/OpenPuzzle/openpuzzle.db").string())||!database.createSchema())return std::nullopt;
  return GpuProfileManager(database).load(gpu,"CUDA","Vanity");
}
std::vector<std::string> fields(const std::string& line){std::istringstream input(line);std::vector<std::string> result;std::string value;
  while(std::getline(input,value,',')){const auto first=value.find_first_not_of(" \t\r"),last=value.find_last_not_of(" \t\r");result.push_back(first==std::string::npos?"":value.substr(first,last-first+1));}return result;
}
}

bool VanityRunProfile::configured(){
  const char* legacy=std::getenv("OPENPUZZLE_VANITY_COMMON_PROFILE");
  if(legacy&&*legacy)return true;
  try {
    const auto engine=ToolManager::vanityPath(),adapter=ToolManager::vanityAdapterPath();
    if(!engine||!adapter)return false;
    VanityEngineConfiguration configuration{*engine,*adapter,hashFile(*engine),hashFile(*adapter)};
    return VanityCommonEngine(configuration).prepare();
  } catch(...) {return false;}
}

VanityRunProfile VanityRunProfile::load(int selectedDevice){
  need(selectedDevice>=0,"Invalid Vanity CUDA device");
  if(const char* legacy=std::getenv("OPENPUZZLE_VANITY_COMMON_PROFILE");legacy&&*legacy){
    const fs::path path=legacy;const auto text=privateRead(path);std::istringstream input(text);input.imbue(std::locale::classic());
    std::string magic,cliHash,scope,extra;VanityRunProfile profile;unsigned files=0;profile.device=selectedDevice;profile.localOnly=true;
    need(bool(input>>magic>>cliHash>>scope>>std::quoted(profile.executables.engine)>>std::quoted(profile.executables.adapter)
        >>profile.executables.engineSha256>>profile.executables.adapterSha256>>std::quoted(profile.uuid)>>std::quoted(profile.gpu)
        >>std::quoted(profile.driver)>>profile.grid>>profile.planningMKeys>>profile.powerLimit>>files)
        &&magic=="OPV-RUN-PROFILE-1"&&scope=="local-only"&&files>0&&files<=32,"Invalid bound Vanity run profile");
    need(std::regex_match(profile.uuid,std::regex("GPU-[0-9a-fA-F-]{36}"))&&!profile.gpu.empty()&&!profile.driver.empty()
         &&profile.grid&&profile.grid<=4096&&!(profile.grid&(profile.grid-1))&&std::isfinite(profile.planningMKeys)&&profile.planningMKeys>0
         &&std::isfinite(profile.powerLimit)&&profile.powerLimit>0,"Invalid measured Vanity launch context");
    need(hashFile(fs::canonical("/proc/self/exe"))==cliHash,"Vanity profile CLI binding changed");
    for(unsigned i=0;i<files;++i){std::string name,hash;need(bool(input>>std::quoted(name)>>hash)
      &&std::regex_match(hash,std::regex("[0-9a-f]{64}"))&&hashFile(name)==hash,"Measured Vanity evidence changed");}
    need(!(input>>extra),"Trailing Vanity profile data");
    need(VanityCommonEngine(profile.executables).prepare(),"Verified Vanity binaries unavailable/changed");
    profile.fingerprint=hashFile(path);return profile;
  }
  const auto engine=ToolManager::vanityPath(),adapter=ToolManager::vanityAdapterPath();
  need(engine.has_value()&&adapter.has_value(),"Bundled Vanity engine/adapter unavailable");
  VanityRunProfile profile;profile.device=selectedDevice;
  profile.executables={*engine,*adapter,hashFile(*engine),hashFile(*adapter)};
  need(VanityCommonEngine(profile.executables).prepare(),"Bundled Vanity executable identity rejected");
  profile.gpu=GpuManager::currentGpu("CUDA",selectedDevice).name;
  need(!profile.gpu.empty(),"Selected Vanity CUDA device is unavailable");
  if(const auto measured=profileFor(profile.gpu)){
    need(measured->blocks>0&&measured->blocks<=4096&&!(measured->blocks&(measured->blocks-1))
         &&measured->threads==256&&measured->points==1024&&std::isfinite(measured->averageSpeed)&&measured->averageSpeed>0,
         "Stored Vanity GPU profile is invalid");
    profile.grid=static_cast<unsigned>(measured->blocks);profile.planningMKeys=measured->averageSpeed;
  }
  profile.fingerprint=invoke("printf %s "+shellQuote(profile.executables.engineSha256+profile.executables.adapterSha256+
      std::to_string(profile.device)+profile.gpu+std::to_string(profile.grid))+" | sha256sum").substr(0,64);
  return profile;
}

void VanityRunProfile::requireServer(const std::string& server){
  const auto endpoint=client::HttpRequestPolicy::endpoint(server);
  const char* legacy=std::getenv("OPENPUZZLE_VANITY_COMMON_PROFILE");
  need(endpoint.has_value()&&(legacy&&*legacy ? endpoint->loopback() : (endpoint->secure||endpoint->loopback())),
       "Vanity requires HTTPS or a loopback test server");
}

std::uint64_t VanityRunProfile::maximumKeys()const{
  return std::min<std::uint64_t>(std::uint64_t(1)<<40,std::uint64_t(grid)*256*1024*8192);
}

std::uint64_t VanityRunProfile::requestedKeys(int minutes)const{
  need(minutes>=1&&minutes<=360,"Invalid Vanity duration");
  if(!(std::isfinite(planningMKeys)&&planningMKeys>0))return maximumKeys();
  const double desired=planningMKeys*1000000*60*minutes;
  return desired>=double(maximumKeys())?maximumKeys():std::max<std::uint64_t>(1,std::uint64_t(std::ceil(desired)));
}

void VanityRunProfile::validateHardware(int selectedDevice,bool)const{
  need(selectedDevice==device&&selectedDevice>=0,"Vanity CUDA device changed");
  if(localOnly){
    std::istringstream rows(invoke("nvidia-smi --query-gpu=index,uuid,name,driver_version,power.limit,temperature.gpu --format=csv,noheader,nounits"));
    std::string line;unsigned matches=0;
    while(std::getline(rows,line)){const auto values=fields(line);if(values.size()!=6||values[1]!=uuid)continue;
      need(decimal(values[0])==std::uint64_t(selectedDevice)&&values[2]==gpu&&values[3]==driver,"Measured Vanity physical GPU/driver differs");
      const auto power=std::stod(values[4]),temperature=std::stod(values[5]);
      need(std::isfinite(power)&&std::abs(power-powerLimit)<=.5&&std::isfinite(temperature)&&temperature<85,"Measured Vanity power/thermal context differs");++matches;
    }
    need(matches==1,"Measured Vanity GPU is missing/ambiguous");
    std::istringstream processes(invoke("nvidia-smi --query-compute-apps=pid,gpu_uuid --format=csv,noheader,nounits"));
    while(std::getline(processes,line)){const auto values=fields(line);need(values.size()!=2||values[1]!=uuid,"Selected Vanity GPU is busy");}
    need(setenv("CUDA_VISIBLE_DEVICES",uuid.c_str(),1)==0&&setenv("CUDA_DEVICE_ORDER","PCI_BUS_ID",1)==0,"Cannot isolate measured Vanity GPU");return;
  }
  need(GpuManager::currentGpu("CUDA",selectedDevice).name==gpu,"Vanity CUDA device identity changed");
  need(setenv("CUDA_VISIBLE_DEVICES",std::to_string(selectedDevice).c_str(),1)==0&&setenv("CUDA_DEVICE_ORDER","PCI_BUS_ID",1)==0,
       "Cannot isolate selected Vanity GPU");
}

void VanityRunProfile::validatePuzzle(const PuzzleExecutionMetadata& metadata)const{
  need(metadata.puzzle>=65&&metadata.puzzle<=80&&metadata.searchMode=="linear"&&metadata.publicKey.empty()
       &&(metadata.requiredBackend.empty()||metadata.requiredBackend=="cuda"),"Puzzle is outside validated Vanity capabilities");
  const auto separator=metadata.keyspace.find(':');need(separator!=std::string::npos,"Explicit Vanity puzzle bounds required");
  const auto start=canonical(metadata.keyspace.substr(0,separator)),end=canonical(metadata.keyspace.substr(separator+1));
  need(start>=canonical("10000000000000000")&&start<=end&&end<canonical("100000000000000000000")&&!metadata.address.empty(),
       "Puzzle exceeds validated Vanity scalar width");
}

void VanityRunProfile::validateAssignment(const client::RangeAssignment& assignment,const PuzzleExecutionMetadata& metadata)const{
  validatePuzzle(metadata);need(std::regex_match(assignment.assignmentId,std::regex("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-4[0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}"))
    &&assignment.valid()&&assignment.puzzle==metadata.puzzle&&assignment.target==metadata.address&&assignment.searchMode=="linear"
    &&assignment.publicKey.empty()&&(assignment.requiredBackend.empty()||assignment.requiredBackend=="cuda"),
    "Vanity assignment differs from validated puzzle/backend");
  const auto separator=metadata.keyspace.find(':');const auto start=canonical(assignment.start),end=canonical(assignment.end);
  need(start>=canonical(metadata.keyspace.substr(0,separator))&&end<=canonical(metadata.keyspace.substr(separator+1))&&start<=end
    &&inclusiveCount(start,end)<=maximumKeys(),"Vanity assignment escapes validated bounds/count");
}

void VanityRunProfile::bindWorkspace(const fs::path& workspace,const std::string& server)const{
  requireServer(server);immutableProfileBinding(workspace/"vanity-run-profile.txt",fingerprint+'\n'+server+'\n');
}

void VanityRunProfile::requireWorkspaceBinding(const fs::path& workspace,const std::string& server)const{
  need(privateRead(workspace/"vanity-run-profile.txt")==fingerprint+'\n'+server+'\n',"Common Vanity recovery executable/device/server changed");
}

std::uint64_t VanityRunProfile::prepareRecovery(const client::ClientExecutionState& state,const fs::path& expected,const std::string& server)const{
  need(state.valid()&&state.engine=="Vanity"&&state.backend=="CUDA"&&state.device==device&&state.workspace==expected.string()
    &&expected.filename()==state.assignmentId&&fs::canonical(expected)==expected,"Common Vanity recovery state/workspace differs");
  requireWorkspaceBinding(expected,server);
  EngineLaunchRequest request;request.engine="Vanity";request.backend="CUDA";request.device=0;
  request.blocks=grid;request.threads=256;request.points=1024;request.targets={state.target};request.startKey=state.start;request.endKey=state.end;request.workspace=state.workspace;
  (void)VanityCommonEngine(executables).buildCommand(request);
  const auto checked=vanityCommonCheckedCoverage(state.workspace);need(checked.has_value(),"Common Vanity native coverage requires review");return *checked;
}
}
