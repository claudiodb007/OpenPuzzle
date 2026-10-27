// MIT. Experimental local CLI; no production coordinator is called here.
#include "VanityCommand.hpp"
#include "CoordinatorSession.hpp"
#include "VanityPreviewEngine.hpp"
#include "LoopbackHttp.hpp"
#include "openpuzzle/core/PosixProcessRunner.hpp"
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
int runVanityLocalAssignment(int,char**);
int runVanityHttpAssignment(const std::vector<std::string>&);
namespace openpuzzle {
namespace {
namespace fs=std::filesystem;
using vanityPreview::shellQuote;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool digest(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
std::string read(const fs::path& p){
 require(!fs::is_symlink(fs::symlink_status(p))&&fs::is_regular_file(p)&&fs::file_size(p)<=65536,"Missing, unsafe or oversized profile/evidence file");
 std::ifstream f(p);std::string s((std::istreambuf_iterator<char>(f)),{});require(bool(f)||f.eof(),"Unable to read profile");return s;
}
std::string execute(const std::string& command){
 PosixProcessRunner runner;std::string output;bool oversized=false;
 auto result=runner.run(command,[&](const auto& line){if(output.size()+line.size()>8192){oversized=true;return;}output+=line+'\n';},15,[&]{return oversized;});
 require(!oversized&&result.started&&result.exitCode==0,"Local identity/checksum command failed");return output;
}
std::string hashCommand(const std::string& command){auto out=execute(command);std::istringstream in(out);std::string h;in>>h;require(digest(h),"Malformed SHA-256 result");return h;}
std::string hashFile(const fs::path& p){require(!fs::is_symlink(fs::symlink_status(p))&&fs::is_regular_file(p),"Missing/unsafe bound file");return hashCommand("sha256sum < "+shellQuote(p.string()));}
std::string hashText(const std::string& text){return hashCommand("printf %s "+shellQuote(text)+" | sha256sum");}
struct Profile {
 std::string engine,engineHash,adapter,adapterHash,gpuUuid,gpuName,performance,performanceHash,measuredAdapterHash,integration,integrationHash;
 uint64_t grid=0,threads=0,points=0,samples=0;double median=0,minimum=0,maximum=0,planning=0;
};
Profile parse(const std::string& data){
 std::istringstream in(data);std::string magic;std::getline(in,magic);require(magic=="OPV-CLI-PROFILE-1","Unsupported experimental profile format");Profile p;
 auto string=[&](const char* field,std::string& value){std::string name;require(bool(in>>name>>std::quoted(value))&&name==field&&!value.empty()&&value.size()<4096&&value.find_first_of("\r\n\0",0,3)==std::string::npos,"Invalid profile string");};
 auto number=[&](const char* field,auto& value){std::string name;require(bool(in>>name>>value)&&name==field,"Invalid profile numeric field");};
 string("engine",p.engine);string("engine_sha256",p.engineHash);string("adapter",p.adapter);string("adapter_sha256",p.adapterHash);
 string("gpu_uuid",p.gpuUuid);string("gpu_name",p.gpuName);number("grid",p.grid);number("threads",p.threads);number("points",p.points);
 number("median_mkeys",p.median);number("minimum_mkeys",p.minimum);number("maximum_mkeys",p.maximum);number("planning_mkeys",p.planning);number("samples",p.samples);
 string("performance_summary",p.performance);string("performance_sha256",p.performanceHash);string("measurement_adapter_sha256",p.measuredAdapterHash);string("integration_summary",p.integration);string("integration_sha256",p.integrationHash);
 std::string extra;require(!(in>>extra),"Trailing experimental profile data");
 require(digest(p.engineHash)&&digest(p.adapterHash)&&digest(p.performanceHash)&&digest(p.measuredAdapterHash)&&digest(p.integrationHash),"Invalid profile fingerprint");
 for(const auto& path:{p.engine,p.adapter,p.performance,p.integration})require(fs::path(path).is_absolute(),"Bound paths must be absolute");
 require(p.gpuUuid.size()==40&&p.gpuUuid.rfind("GPU-",0)==0&&p.gpuUuid.substr(4).find_first_not_of("0123456789abcdefABCDEF-")==std::string::npos,"Invalid GPU UUID");
 require(p.gpuName=="NVIDIA GeForce RTX 4070 SUPER","This preview is validated only for RTX 4070 SUPER");
 require(p.grid>0&&p.grid<=4096&&(p.grid&(p.grid-1))==0&&p.threads==256&&p.points==1024,"Invalid Vanity launch geometry");
 require(std::isfinite(p.median)&&std::isfinite(p.minimum)&&std::isfinite(p.maximum)&&std::isfinite(p.planning)&&p.minimum>0&&p.minimum<=p.median&&p.median<=p.maximum&&p.maximum<1000000&&p.samples==3,"Invalid measured profile statistics");
 require(std::abs(p.planning-p.median*.97)<=std::max(1e-8,p.median*1e-10),"Planning speed must retain the explicit 3 percent margin");return p;
}
void verify(const Profile& p){
 require(hashFile(p.engine)==p.engineHash&&hashFile(p.adapter)==p.adapterHash,"Experimental executable fingerprint changed; profile preserved");
 require(hashFile(p.performance)==p.performanceHash&&hashFile(p.integration)==p.integrationHash,"Prior evidence fingerprint changed; profile preserved");
 require(access(p.engine.c_str(),X_OK)==0&&access(p.adapter.c_str(),X_OK)==0,"Experimental executable is not executable");
 require(execute(shellQuote(p.engine)+" --identity")=="OpenPuzzle-VanityRange 0.1.0 CUDA synthetic protocol 1\n","Unsupported experimental engine identity");
}
void import(const fs::path& destination,const std::string& data){
 require(destination.is_absolute()&&fs::is_directory(destination.parent_path())&&!fs::is_symlink(fs::symlink_status(destination.parent_path())),"Explicit existing private profile directory required");
 require((fs::status(destination.parent_path()).permissions()&(fs::perms::group_all|fs::perms::others_all))==fs::perms::none,"Profile directory must be owner-only");
 if(fs::exists(destination)||fs::is_symlink(fs::symlink_status(destination))){require(read(destination)==data,"Existing different profile preserved; choose a new file");require((fs::status(destination).permissions()&(fs::perms::group_all|fs::perms::others_all))==fs::perms::none,"Existing non-private profile preserved");std::cout<<"Identical experimental profile preserved.\n";return;}
 int fd=open(destination.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);require(fd>=0,"Profile creation refused");
 bool ok=true;size_t n=0;while(n<data.size()){ssize_t k=write(fd,data.data()+n,data.size()-n);if(k<0&&errno==EINTR)continue;if(k<=0){ok=false;break;}n+=size_t(k);}if(fsync(fd))ok=false;close(fd);
 int dir=open(destination.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(dir<0||fsync(dir))ok=false;if(dir>=0)close(dir);
 require(ok,"Incomplete profile write preserved for review");std::cout<<"Experimental Vanity profile saved. BitCrack profiles unchanged.\n";
}
void show(const Profile& p){std::cout<<"Engine: vanity-preview\nBackend: cuda\nGPU: "<<p.gpuName<<"\nGPU UUID: "<<p.gpuUuid<<"\nGrid: "<<p.grid<<"\nThreads: "<<p.threads<<"\nPoints: "<<p.points<<"\nMeasured median MKey/s: "<<std::setprecision(12)<<p.median<<"\nPlanning MKey/s (3% margin): "<<p.planning<<"\nMeasurement pairs: "<<p.samples<<"\nMeasurement source: prior checkpoint comparison; final CLI calibration pending\nProduction assignments: disabled\n";}
}
int runVanityCommand(const std::vector<std::string>& args){
 try{
  if(args.empty()||args[0]=="help"){std::cout<<"Experimental Vanity commands:\n  vanity-preview profile-import --from FILE --profile FILE --enable-experimental-vanity-local\n  vanity-preview profile-show --profile FILE\n  vanity-preview verify --profile FILE\n  vanity-preview local-test --profile FILE --root DIR --controls DIR --keys FILE --expected FILE --start HEX --count N --grid N --scenario NAME --enable-experimental-vanity-local\n  vanity-preview http-test (explicit loopback/target/engine hashes; see VANITY_HTTP_PREVIEW.md)\nNo production assignments. This build does not enable run --engine vanity.\n";return 0;}
  if(args[0].rfind("measured-",0)==0){
   // Replace this process: Python owns its bounded local child and signal cleanup.
   std::vector<std::string> native={OPENPUZZLE_VANITY_PYTHON,OPENPUZZLE_VANITY_MEASURED_SCRIPT};
   native.insert(native.end(),args.begin(),args.end());std::vector<char*> argv;
   for(auto& a:native)argv.push_back(a.data());argv.push_back(nullptr);
   execv(native[0].c_str(),argv.data());throw std::runtime_error("Measured-profile helper could not start");
  }
  if(args[0]=="coordinator-test"){
   const std::set<std::string> fields={"--server","--root","--controls","--engine","--engine-sha256","--adapter","--adapter-sha256","--client-id","--target","--puzzle","--grid","--segment-keys","--scope-start","--scope-end","--gpu-uuid","--speed-mkeys","--receipt-protocol"};
   std::map<std::string,std::string> values;bool enabled=false,resume=false;
   for(size_t i=1;i<args.size();++i){const auto& k=args[i];
    if(k=="--enable-experimental-vanity-local"){require(!enabled,"Repeated coordinator opt-in");enabled=true;continue;}
    if(k=="--resume"){require(!resume,"Repeated coordinator resume");resume=true;continue;}
    require(fields.count(k)&&i+1<args.size(),"Unknown/missing coordinator option");auto value=args[++i];
    require(!value.empty()&&value.find_first_of("\r\n\0",0,3)==std::string::npos&&values.emplace(k,value).second,"Repeated/invalid coordinator option");
   }
   require(enabled&&values.size()==fields.size()-(values.count("--receipt-protocol")?0:1),"Explicit local coordinator binding and opt-in required");
   const auto& uuid=values.at("--gpu-uuid");require(uuid.size()==40&&uuid.rfind("GPU-",0)==0&&uuid.substr(4).find_first_not_of("0123456789abcdefABCDEF-")==std::string::npos,"Invalid coordinator GPU UUID");
   return vanityPreview::runCoordinatorSession(values,resume);
  }
  if(args[0]=="http-test"){
   const std::set<std::string> fields={"--engine","--engine-sha256","--adapter","--adapter-sha256","--root","--controls","--server","--start","--count","--grid","--target-kind","--target","--gpu-uuid"};
   std::map<std::string,std::string> o;bool enabled=false;
   for(size_t i=1;i<args.size();++i){auto k=args[i];if(k=="--enable-experimental-vanity-local"){require(!enabled,"Repeated opt-in");enabled=true;continue;}require(fields.count(k)&&i+1<args.size(),"Unknown/missing HTTP option");require(o.emplace(k,args[++i]).second,"Repeated HTTP option");}
   require(enabled&&o.size()==fields.size(),"All explicit HTTP binding options and opt-in are required");
   vanityPreview::LoopbackHttp transport(o.at("--server")); // Reject remote endpoints before filesystem/process work.
   for(const auto& k:{"--engine","--adapter","--root","--controls"})require(fs::path(o.at(k)).is_absolute(),"HTTP preview paths must be absolute");
   const auto& uuid=o.at("--gpu-uuid");const char* visible=getenv("CUDA_VISIBLE_DEVICES");
   require(uuid.size()==40&&uuid.rfind("GPU-",0)==0&&uuid.substr(4).find_first_not_of("0123456789abcdefABCDEF-")==std::string::npos&&visible&&uuid==visible,"Explicit selected GPU UUID required");
   require(digest(o.at("--engine-sha256"))&&hashFile(o.at("--engine"))==o.at("--engine-sha256")&&digest(o.at("--adapter-sha256"))&&hashFile(o.at("--adapter"))==o.at("--adapter-sha256"),"HTTP executable fingerprint mismatch");
   std::string bound;for(const auto& pair:o){require(pair.second.find_first_of("\r\n\0",0,3)==std::string::npos,"Control character in HTTP option");bound+=pair.first+"="+pair.second+"\n";}
   std::vector<std::string> native;for(const auto& k:{"--engine","--adapter","--root","--controls","--server","--start","--count","--grid","--target-kind","--target","--gpu-uuid"})native.push_back(o.at(k));native.push_back(hashText(bound));return runVanityHttpAssignment(native);
  }
  const std::string action=args[0];const std::set<std::string> actions={"profile-import","profile-show","verify","local-test"};require(actions.count(action),"Unknown experimental command");
  std::map<std::string,std::string> options;bool enabled=false;
  const std::set<std::string> local={"--profile","--root","--controls","--keys","--expected","--start","--count","--grid","--scenario"};
  for(size_t i=1;i<args.size();++i){auto key=args[i];if(key=="--enable-experimental-vanity-local"){require(!enabled,"Repeated opt-in");enabled=true;continue;}
   require((action=="local-test"?local.count(key):key=="--profile"||(action=="profile-import"&&key=="--from"))&&i+1<args.size(),"Unknown or missing experimental option");
   auto value=args[++i];require(!value.empty()&&value.find('\0')==std::string::npos&&options.emplace(key,value).second,"Empty or repeated experimental option");}
  auto get=[&](const char* key)->std::string{require(options.count(key),"Missing experimental option");return options.at(key);};
  auto profilePath=fs::path(get("--profile"));require(profilePath.is_absolute(),"Explicit absolute profile path required");
  if(action=="profile-import"){require(enabled,"Explicit local experimental opt-in required");auto data=read(get("--from"));auto p=parse(data);verify(p);import(profilePath,data);show(p);return 0;}
  auto p=parse(read(profilePath));if(action=="profile-show"){show(p);return 0;}
  verify(p);if(action=="verify"){show(p);std::cout<<"Executable identities and evidence hashes verified; no GPU kernel launched.\n";return 0;}
  require(enabled,"Explicit local experimental opt-in required");
  const char* visible=std::getenv("CUDA_VISIBLE_DEVICES");require(visible&&p.gpuUuid==visible,"CUDA_VISIBLE_DEVICES must exactly match the bound GPU UUID");
  const std::string start=get("--start"),count=get("--count"),grid=get("--grid"),keys=get("--keys"),scenario=get("--scenario");
  vanityPreview::Plan plan(start,vanityPreview::decimal(count),vanityPreview::decimal(grid));
  auto binding=[&](const std::string& first){return hashText(hashFile(profilePath)+"\n"+hashFile(keys)+"\n"+first+"\n"+count+"\n"+grid+"\n"+scenario);};
  std::vector<std::string> native={"local-client","vanity-preview",p.adapter,p.engine,get("--root"),get("--controls"),keys,get("--expected"),start,count,grid,binding(plan.start),binding(vanityPreview::add(plan.start,plan.count)),scenario,"--enable-experimental-vanity-local"};
  std::vector<char*> argv;for(auto& a:native)argv.push_back(a.data());return runVanityLocalAssignment(int(argv.size()),argv.data());
 }catch(const std::exception& e){std::cerr<<"Vanity preview refused: "<<e.what()<<'\n';return 2;}
}
}
