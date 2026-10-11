#include "openpuzzle/runtime/RunAssignmentPlan.hpp"
#include "openpuzzle/runtime/WindowsEngineRange.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include <algorithm>
#include <regex>
#include <cctype>
#include <stdexcept>
namespace openpuzzle {
namespace {
std::string hex(std::string value){
 if(value.empty()||value.size()>64)throw std::invalid_argument("Invalid Bitcoin range boundary");
 for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))throw std::invalid_argument("Range boundary is not hexadecimal");
 auto first=value.find_first_not_of('0');value=first==std::string::npos?"0":value.substr(first);
 std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});return value;
}
bool before(const std::string& a,const std::string& b){return a.size()<b.size()||(a.size()==b.size()&&a<b);}
}
bool RunAssignmentPlan::safeAssignmentId(const std::string& id){
 std::string upper=id;std::transform(upper.begin(),upper.end(),upper.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
 if(upper=="CON"||upper=="PRN"||upper=="AUX"||upper=="NUL"||
    (upper.size()==4&&(upper.starts_with("COM")||upper.starts_with("LPT"))&&upper[3]>='1'&&upper[3]<='9'))return false;
 return !id.empty()&&id.size()<=128&&std::all_of(id.begin(),id.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-';});
}
RunAssignmentPlan RunAssignmentPlan::build(const RunCommandOptions& o,const client::RangeAssignment& a,
    const std::string& clientId,const std::filesystem::path& workspace,const PuzzleExecutionMetadata* metadata) {
 if(o.command!="run"||o.puzzle<0||o.device<0||o.blocks<=0||o.threads<=0||o.points<=0||o.durationMinutes<1||o.durationMinutes>360)
   throw std::invalid_argument("Invalid run options");
 if(!a.valid()||!safeAssignmentId(a.assignmentId)||clientId.empty()||clientId.find('\0')!=std::string::npos||!workspace.is_absolute()||
    (o.puzzle>0&&o.puzzle!=a.puzzle))throw std::invalid_argument("Assignment identity does not match the run request");
 static const std::regex address("^1[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]{25,34}$");
 if(!std::regex_match(a.target,address))throw std::invalid_argument("Invalid assignment target address");
 const auto start=hex(a.start),end=hex(a.end);if(before(end,start))throw std::invalid_argument("Descending assignment range");
 if(a.searchMode!="linear"||o.engine=="kangaroo")throw std::invalid_argument("Windows Kangaroo planning/checkpoint recovery is not integrated in this stage");
 if((o.engine!="bitcrack"|| (o.backend!="cuda"&&o.backend!="opencl")) && (o.engine!="keyhunt"||o.backend!="cpu"))throw std::invalid_argument("Invalid engine/backend routing");
 if(o.backend=="cpu"&&(o.device!=0||o.gpuOptionsSpecified))throw std::invalid_argument("GPU options cannot be ignored by CPU planning");
 if(!a.requiredBackend.empty()&&a.requiredBackend!=o.backend)throw std::invalid_argument("Assignment backend differs from preflight");
 if(metadata){
   if(metadata->puzzle!=a.puzzle||metadata->searchMode!=a.searchMode||(!metadata->address.empty()&&metadata->address!=a.target))throw std::invalid_argument("Assignment differs from validated puzzle metadata");
   if(!metadata->requiredBackend.empty()&&metadata->requiredBackend!=o.backend)throw std::invalid_argument("Puzzle requires another backend");
   if(!metadata->keyspace.empty()){
     const auto sep=metadata->keyspace.find(':');if(sep==std::string::npos)throw std::invalid_argument("Invalid metadata keyspace");
     if(before(start,hex(metadata->keyspace.substr(0,sep)))||before(hex(metadata->keyspace.substr(sep+1)),end))throw std::invalid_argument("Assignment escapes puzzle keyspace");
   }
 }
 RunAssignmentPlan plan;auto& s=plan.state;s.active=true;s.assignmentId=a.assignmentId;s.clientId=clientId;s.puzzle=a.puzzle;s.rangeId=a.rangeId;
 s.target=a.target;s.start=start;s.end=end;s.engine=o.engine=="bitcrack"?"BitCrack":"KeyHunt";s.backend=o.backend;s.device=o.device;
 s.blocks=o.backend=="cpu"?0:o.blocks;s.threads=o.threads;s.points=o.backend=="cpu"?0:o.points;s.workspace=pathUtf8(workspace);
 if(o.engine=="bitcrack")plan.arguments={a.target,"--keyspace",start+":"+end,"--out",pathUtf8(workspace/"found.txt"),"-d",std::to_string(o.device),"-b",std::to_string(o.blocks),"-t",std::to_string(o.threads),"-p",std::to_string(o.points)};
 else {
#ifdef _WIN32
 const auto cpuEnd=WindowsEngineRange::keyhuntExclusiveEnd(start,end);
#else
 const auto& cpuEnd=end;
#endif
 plan.arguments={"-m","address","-f",pathUtf8(workspace/"targets.txt"),"-r",start+":"+cpuEnd,"-l","compress","-n","1024","-q","-s","1","-t",std::to_string(o.threads)};plan.targetFileContent=a.target+"\n";}
#ifdef _WIN32
 if(o.engine=="bitcrack")plan.arguments.push_back("-c");
#endif
 return plan;
}
}
