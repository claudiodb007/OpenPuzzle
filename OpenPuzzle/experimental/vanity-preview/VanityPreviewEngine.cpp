#include "VanityPreviewEngine.hpp"
#include <sstream>
#include <map>
#include <unistd.h>
namespace fs=std::filesystem;
namespace openpuzzle::vanityPreview {
std::string shellQuote(const std::string& value){
 if(value.find('\0')!=std::string::npos)throw std::runtime_error("NUL in command argument");
 std::string out="'";for(char c:value)out+=c=='\''?"'\\''":std::string(1,c);return out+"'";
}
uint64_t inclusiveCount(const std::string& first,const std::string& last){
 auto a=canonical(first),b=canonical(last);if(a>b)throw std::runtime_error("Descending range");
 uint64_t diff=0;int borrow=0;
 for(int i=63;i>=0;--i){
  auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};
  int n=digit(b[i])-digit(a[i])-borrow;borrow=n<0;if(borrow)n+=16;
  if(i<48&&n)throw std::runtime_error("Range count exceeds native scalar limit");
  if(i>=48)diff|=uint64_t(n)<<((63-i)*4);
 }
 if(diff>=((uint64_t(1)<<40)))throw std::runtime_error("Experimental range too large");
 return diff+1;
}
EngineInfo VanityPreviewEngine::info()const{
 EngineInfo i;i.name="VanitySearch (local experimental)";i.version="0.4.0";i.backend="CUDA";i.executable=config_.engine;
 i.available=fs::is_regular_file(config_.engine)&&fs::is_regular_file(config_.adapter);return i;
}
bool VanityPreviewEngine::prepare(){return info().available&&access(config_.engine.c_str(),X_OK)==0&&access(config_.adapter.c_str(),X_OK)==0;}
std::string VanityPreviewEngine::buildCommand(const EngineLaunchRequest& r)const{
 if(r.engine!="vanity-preview"||r.backend!="cuda"||r.device!=0||r.threads!=256||r.points!=1024||r.blocks<1)
  throw std::runtime_error("Unsupported experimental engine/backend/device/geometry");
 Plan plan(r.startKey,inclusiveCount(r.startKey,r.endKey),r.blocks);
 if(r.targetFile!=config_.knownKeys||!fs::is_regular_file(r.targetFile)||!fs::is_directory(config_.controls))throw std::runtime_error("Synthetic fixture input missing");
 if(config_.binding.size()!=64||config_.binding.find_first_not_of("0123456789abcdef")!=std::string::npos||(config_.mode!="exhaust"&&config_.mode!="stop"))throw std::runtime_error("Invalid experimental binding/mode");
 if(r.workspace.empty()||!fs::path(r.workspace).is_absolute())throw std::runtime_error("Absolute private workspace required");
 std::string cmd="exec "+shellQuote(config_.adapter);
 for(const auto& arg:std::vector<std::string>{config_.engine,r.workspace,plan.start,std::to_string(plan.count),std::to_string(plan.blocks),config_.knownKeys,config_.mode,config_.binding,config_.controls,config_.targetMode?"--targets-only":"--synthetic-only","--owner",std::to_string(getpid())})cmd+=" "+shellQuote(arg);
 return cmd;
}
void registerVanityPreview(EngineRegistry& registry,EngineFactory& factory,const AdapterConfig& config,bool enabled){
 if(!enabled)return;
 EngineDescriptor d;d.id="vanity-preview";d.name="VanitySearch (local experimental)";d.version="0.4.0";d.backend="cuda";
 d.runtime.executable=config.engine;d.runtime.installed=fs::is_regular_file(config.engine);d.runtime.available=d.runtime.installed&&fs::is_regular_file(config.adapter);
 d.capabilities.cuda=true;d.capabilities.supportsUncompressed=false;d.capabilities.supportsMultipleTargets=true;
 d.capabilities.supportsLinearSearch=true;d.capabilities.supportsBenchmark=false;d.capabilities.distributedReady=false;
 d.capabilities.supportsResume=true;d.capabilities.supportsCheckpoint=true;
 d.capabilities.defaultBlocks=1024;d.capabilities.defaultThreads=256;d.capabilities.defaultPoints=1024;
 registry.registerEngine(d);factory.registerFactory(d.id,[config](const auto& executable){if(executable!=config.engine)throw std::runtime_error("Engine identity/path mismatch");return std::make_unique<VanityPreviewEngine>(config);});
}
VanityPreviewProgressParser::VanityPreviewProgressParser(uint64_t total,uint64_t resumed):total_(total),checked_(resumed),resumed_(resumed){if(!total||resumed>total)throw std::runtime_error("Invalid progress bounds");}
std::optional<ExecutionProgress> VanityPreviewProgressParser::parseLine(const std::string& line){
 if(line.rfind("OPV_CLIENT ",0)!=0)return std::nullopt;
 if(terminal_)throw std::runtime_error("Progress after terminal result");
 std::istringstream in(line);std::string magic,kind,a,b;in>>magic>>kind;
 ExecutionProgress p;
 if(kind=="PROGRESS"){
  if(!(in>>a)||(in>>b))throw std::runtime_error("Malformed progress");
  auto n=decimal(a);if(n<=checked_||n>total_)throw std::runtime_error("Invalid, duplicate or regressing progress");checked_=n;
 }else if(kind=="RESULT"){
  std::string status;if(!(in>>status))throw std::runtime_error("Missing terminal state");
  std::map<std::string,std::string> values;while(in>>a){auto pos=a.find('=');if(pos==std::string::npos||!values.emplace(a.substr(0,pos),a.substr(pos+1)).second)throw std::runtime_error("Malformed terminal fields");}
  if(values.size()!=4||!values.count("checked")||!values.count("resumed")||!values.count("safe_stop")||!values.count("next_blocked"))throw std::runtime_error("Missing terminal fields");
  if(decimal(values.at("checked"))!=checked_||decimal(values.at("resumed"))!=resumed_||decimal(values.at("safe_stop"))>1||decimal(values.at("next_blocked"))>1)throw std::runtime_error("Terminal progress mismatch");
  if(status=="complete"){if(checked_!=total_)throw std::runtime_error("Partial completion");p.finished=true;}
  else if(status=="solution"){p.keyFound=true;}
  else if(status=="interrupted"||status=="faulted"||status=="review"){p.error=true;p.message=status;}
  else throw std::runtime_error("Unknown terminal state");
  terminal_=true;
 }else if(kind=="BLOCKED"){p.error=true;p.message="Preserved native state blocks launch";terminal_=true;return p;}
 else throw std::runtime_error("Unknown native adapter record");
 p.keysChecked=std::to_string(checked_);
 double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start_).count();
 if(seconds>0)p.speedMKeys=(checked_-resumed_)/seconds/1e6;
 return p;
}
}
