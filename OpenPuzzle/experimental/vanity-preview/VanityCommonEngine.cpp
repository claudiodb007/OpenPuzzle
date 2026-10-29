#include "VanityCommonEngine.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/PosixProcessRunner.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <tuple>
#include <sys/stat.h>
#include <unistd.h>

namespace openpuzzle {
namespace {
namespace fs=std::filesystem;
using namespace vanityPreview;
void need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::string lower(std::string value){for(char& c:value)c=std::tolower(static_cast<unsigned char>(c));return value;}
std::ifstream privateInput(const fs::path& path,std::uintmax_t maximum=1024*1024){
  struct stat st{};
  need(path.is_absolute()&&fs::canonical(path)==path&&lstat(path.c_str(),&st)==0&&S_ISREG(st.st_mode)
       &&st.st_uid==getuid()&&!(st.st_mode&0077)&&st.st_size>=0&&std::uintmax_t(st.st_size)<=maximum,"Unsafe/missing Vanity evidence");
  std::ifstream input(path);need(bool(input),"Cannot read Vanity evidence");return input;
}
std::string read(const fs::path& path,std::uintmax_t maximum=1024*1024){
  auto input=privateInput(path,maximum);
  return std::string((std::istreambuf_iterator<char>(input)),{});
}
std::string invoke(const std::string& command){
  PosixProcessRunner runner;std::string result;bool tooLarge=false;
  auto execution=runner.run(command,[&](const auto& line){result+=line+'\n';tooLarge=result.size()>65536;},20,[&]{return tooLarge;});
  need(execution.started&&execution.exitCode==0&&!tooLarge,"Vanity identity/hash verification failed");return result;
}
std::string digest(const std::string& value){return invoke("printf %s "+shellQuote(value)+" | sha256sum").substr(0,64);}
void executable(const std::string& path,const std::string& expected){
  need(std::regex_match(expected,std::regex("[0-9a-f]{64}"))&&fs::path(path).is_absolute()
       &&fs::canonical(path)==path&&fs::is_regular_file(fs::symlink_status(path))&&access(path.c_str(),X_OK)==0,"Invalid Vanity executable binding");
  need(invoke("sha256sum < "+shellQuote(path)).substr(0,64)==expected,"Vanity executable hash changed");
}
void immutable(const fs::path& path,const std::string& value){
  if(fs::exists(path)||fs::is_symlink(path))need(read(path)==value,"Existing Vanity assignment binding changed");
  else need(writePrivateFileAtomically(path,value),"Cannot publish Vanity binding");
}
struct Binding{std::string start,target,engine,engineHash,adapter,adapterHash,hash;uint64_t count=0,grid=0;};
Binding binding(const fs::path& workspace){
  auto text=read(workspace/"vanity-binding.txt");std::istringstream input(text);Binding b;std::string magic,extra;
  need(bool(input>>magic>>b.start>>b.count>>b.grid>>std::quoted(b.target)>>std::quoted(b.engine)>>b.engineHash
      >>std::quoted(b.adapter)>>b.adapterHash)&&!(input>>extra)&&magic=="OPV-COMMON-1","Invalid common Vanity binding");
  Plan plan(b.start,b.count,b.grid,AssignmentRangeLimit);need(b.start==plan.start,"Noncanonical Vanity start");b.hash=digest(text);
  need(read(workspace/"vanity-targets.txt")=="OPV-TARGETS-1\np2pkh "+b.target+'\n',"Vanity target binding changed");
  return b;
}
// Same exact batch/interval/terminal contract as the existing coordinator.
// This is native evidence; a shared log or server acknowledgement cannot replace it.
uint64_t coverage(const fs::path& root,const Binding& b,bool terminal){
  Plan plan(b.start,b.count,b.grid,AssignmentRangeLimit);
  auto cp=read(root/"checkpoint.txt",4096);std::istringstream input(cp);
  std::string magic,hash,start,count,grid,mode,done,status,safe,extra;
  need(bool(input>>magic>>hash>>start>>count>>grid>>mode>>done>>status>>safe)&&!(input>>extra)
       &&magic=="OPV-CHECKPOINT-1"&&hash==b.hash&&start==plan.start&&decimal(count)==plan.count
       &&decimal(grid)==plan.blocks&&mode=="stop"&&(safe=="0"||safe=="1"),"Vanity checkpoint binding differs");
  auto checked=decimal(done);need(checked<=plan.count,"Vanity checkpoint exceeds assignment");
  need(status=="running"||status=="interrupted"||status=="complete","Vanity checkpoint requires review");
  std::vector<std::tuple<uint64_t,uint64_t,bool>> spans;
  for(const auto& item:fs::directory_iterator(root)){
    if(!item.path().filename().string().starts_with("attempt-"))continue;
    need(fs::is_directory(item.symlink_status())&&fs::canonical(item.path())==item.path(),"Unsafe Vanity attempt");
    need(read(item.path()/"solutions.txt").empty(),"Vanity solution requires private export/review");
    std::istringstream log(read(item.path()/"engine.log"));std::string line;std::vector<std::string> records;
    while(std::getline(log,line))if(line.starts_with("OPV1 "))records.push_back(line);
    need(!records.empty(),"Missing Vanity native interval proof");
    std::istringstream begin(records.front());std::string kind,first,n,batch;
    need(bool(begin>>magic>>kind>>first>>n>>batch)&&!(begin>>extra)&&magic=="OPV1"&&kind=="BEGIN","Missing Vanity native BEGIN");
    Plan sub(first,decimal(n),b.grid);need(sub.start>=plan.start&&sub.end<=plan.end,"Vanity attempt escaped assignment");
    Protocol protocol(sub);for(const auto& record:records)protocol.consume(record);
    auto offset=sub.start==plan.start?0:inclusiveCount(plan.start,sub.start)-1;
    spans.emplace_back(offset,protocol.checked,protocol.complete);
  }
  std::sort(spans.begin(),spans.end());uint64_t sum=0;bool complete=false;
  for(const auto& [offset,n,finished]:spans){need(offset==sum,"Vanity attempt gap/overlap");sum+=n;complete=finished;}
  need(sum==checked,"Vanity checkpoint has no exact native coverage");
  if(status=="complete"||checked==plan.count||terminal)
    need(status=="complete"&&complete&&checked==plan.count,"Vanity completion lacks native terminal proof");
  return checked;
}
}

VanityCommonEngine::VanityCommonEngine(VanityEngineConfiguration configuration):configuration_(std::move(configuration)){}
EngineInfo VanityCommonEngine::info() const {
  EngineInfo result;result.name="Vanity";result.version="0.2.0";result.backend="CUDA";result.executable=configuration_.engine;
  result.available=fs::is_regular_file(configuration_.engine)&&fs::is_regular_file(configuration_.adapter);return result;
}
bool VanityCommonEngine::prepare(){try{
  executable(configuration_.engine,configuration_.engineSha256);executable(configuration_.adapter,configuration_.adapterSha256);
  return invoke(shellQuote(configuration_.engine)+" --identity")=="OpenPuzzle-VanityRange 0.2.0 CUDA target protocol 1\n";
}catch(...){return false;}}

std::string VanityCommonEngine::buildCommand(const EngineLaunchRequest& request) const {
  need(lower(request.engine)=="vanity"&&lower(request.backend)=="cuda"&&request.device==0
       &&request.threads==256&&request.points==1024&&request.targets.size()==1&&request.publicKey.empty(),"Unsupported common Vanity request");
  need(std::regex_match(request.targets[0],std::regex("1[1-9A-HJ-NP-Za-km-z]{25,34}")),"Invalid Vanity P2PKH target");
  Plan plan(request.startKey,inclusiveCount(request.startKey,request.endKey),request.blocks,AssignmentRangeLimit);
  const fs::path workspace=request.workspace;
  need(workspace.is_absolute()&&fs::canonical(workspace)==workspace&&fs::is_directory(fs::symlink_status(workspace)),"Private direct Vanity workspace required");
  struct stat st{};need(stat(workspace.c_str(),&st)==0&&st.st_uid==getuid()&&!(st.st_mode&0077),"Vanity workspace permissions differ");
  executable(configuration_.engine,configuration_.engineSha256);executable(configuration_.adapter,configuration_.adapterSha256);
  std::ostringstream raw;raw<<"OPV-COMMON-1\n"<<plan.start<<' '<<plan.count<<' '<<plan.blocks<<'\n'
      <<std::quoted(request.targets[0])<<' '<<std::quoted(configuration_.engine)<<' '<<configuration_.engineSha256<<' '
      <<std::quoted(configuration_.adapter)<<' '<<configuration_.adapterSha256<<'\n';
  const auto hash=digest(raw.str());auto native=workspace/"vanity-native";
  if(fs::exists(workspace/"vanity-binding.txt")){
    need(read(workspace/"vanity-binding.txt")==raw.str(),"Common Vanity assignment changed");
    auto b=binding(workspace);
    if(fs::exists(native/"checkpoint.txt"))coverage(native,b,false);
    else need(!fs::exists(native)||fs::is_empty(native),"Missing common Vanity checkpoint");
  }else need(!fs::exists(native)&&!fs::exists(workspace/"vanity-targets.txt"),"Unbound Vanity evidence preserved");
  immutable(workspace/"vanity-binding.txt",raw.str());
  immutable(workspace/"vanity-targets.txt","OPV-TARGETS-1\np2pkh "+request.targets[0]+'\n');
  const auto controls=workspace/"vanity-controls";
  if(!fs::exists(controls))fs::create_directory(controls);
  need(fs::is_directory(fs::symlink_status(controls))&&fs::canonical(controls)==controls,"Unsafe Vanity controls");
  fs::permissions(controls,fs::perms::owner_all,fs::perm_options::replace);
  AdapterConfig config;config.adapter=configuration_.adapter;config.engine=configuration_.engine;
  config.knownKeys=(workspace/"vanity-targets.txt").string();config.binding=hash;config.controls=controls.string();
  config.targetMode=true;config.mode="stop";config.commonRuntime=true;
  auto nativeRequest=request;nativeRequest.engine="vanity-preview";nativeRequest.backend="cuda";
  nativeRequest.workspace=native.string();nativeRequest.targetFile=config.knownKeys;
  return VanityPreviewEngine(config).buildCommand(nativeRequest);
}

std::optional<ExecutionProgress> VanityCommonProgressParser::parseLine(const std::string& line){
  if(!line.starts_with("OPV_CLIENT "))return std::nullopt;
  try{
    need(!invalid_&&!finished_,"Vanity protocol after invalid/terminal record");
    if(line.starts_with("OPV_CLIENT BEGIN ")){
      need(!parser_,"Repeated common Vanity BEGIN");std::istringstream input(line);std::string a,b,total,resumed,extra;
      need(bool(input>>a>>b>>total>>resumed)&&!(input>>extra)&&total.starts_with("total=")&&resumed.starts_with("resumed="),"Malformed common Vanity BEGIN");
      auto n=decimal(total.substr(6)),prior=decimal(resumed.substr(8));need(n&&n<=AssignmentRangeLimit&&prior<n,"Invalid common Vanity bounds");
      total_=std::to_string(n);parser_=std::make_unique<VanityPreviewProgressParser>(n,prior);return std::nullopt;
    }
    need(bool(parser_),"Common Vanity progress without BEGIN");
    auto value=parser_->parseLine(line);if(value&&value->finished)finished_=true;return value;
  }catch(...){invalid_=true;finished_=false;ExecutionProgress result;result.error=true;result.message="Invalid common Vanity protocol";return result;}
}
bool VanityCommonProgressParser::complete(const std::string& expected)const{
  return !invalid_&&finished_&&!expected.empty()&&total_==expected;
}
bool vanityCommonCompletionProof(const std::string& workspace,const std::string& expected){try{
  auto root=fs::path(workspace);auto b=binding(root);need(std::to_string(b.count)==expected,"Vanity total differs from assignment");
  VanityCommonProgressParser parser;auto log=privateInput(root/"vanity.log",512ULL*1024*1024);std::string line;
  while(std::getline(log,line))parser.parseLine(line);
  return parser.complete(expected)&&coverage(root/"vanity-native",b,true)==b.count;
}catch(...){return false;}}
bool vanityCommonProgressWithinCoverage(const std::string& workspace,const std::string& checked){try{
  auto root=fs::path(workspace);auto b=binding(root);
  return vanityPreview::decimal(checked)<=coverage(root/"vanity-native",b,false);
}catch(...){return false;}}
std::optional<std::uint64_t> vanityCommonCheckedCoverage(const std::string& workspace){try{
  auto root=fs::path(workspace);auto b=binding(root);
  if(!fs::exists(root/"vanity-native"))return 0;
  return coverage(root/"vanity-native",b,false);
}catch(...){return std::nullopt;}}
}
