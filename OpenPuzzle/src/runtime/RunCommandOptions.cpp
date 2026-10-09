#include "openpuzzle/runtime/RunCommandOptions.hpp"
#include "openpuzzle/core/CliInteger.hpp"
#include "openpuzzle/client/HttpRequestPolicy.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
namespace openpuzzle {
namespace {
std::string canonical(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return s;}
}
RunCommandOptions RunCommandOptions::parse(const std::vector<std::string>& input) {
  RunCommandOptions out;auto args=input;
  if(args.empty())return out;
  for(const auto& a:args)if(a.empty()||a.find('\0')!=std::string::npos)throw std::invalid_argument("Empty or NUL command argument");
  std::size_t first=0;
  if(args[0]=="run"||args[0]=="status"||args[0]=="stop"||args[0]=="safestop"||args[0]=="help"||args[0]=="version") {out.command=args[0];first=1;}
  else if(args[0]=="--help"||args[0]=="-h"){out.command="help";first=1;}
  else if(args[0]=="--version"||args[0]=="-V"){out.command="version";first=1;}
  else if(args[0].front()!='-'&&!std::isdigit(static_cast<unsigned char>(args[0].front())))throw std::invalid_argument("Unsupported Windows command");
  if(out.command!="run") {if(first!=args.size())throw std::invalid_argument("Local control command takes no run options");return out;}
  bool positional=false;std::set<std::string> seen;
  for(std::size_t i=first;i<args.size();++i) {
    const auto& a=args[i];if(a.empty()||a.find('\0')!=std::string::npos)throw std::invalid_argument("Empty or NUL command argument");
    if(a.front()!='-') {
      if(positional||seen.count("--puzzle"))throw std::invalid_argument("Repeated puzzle number");
      out.puzzle=cliIntegerArgument({"--puzzle",a},{"--puzzle"},0);if(out.puzzle<=0)throw std::invalid_argument("Explicit puzzle must be positive");positional=true;continue;
    }
    std::string name=a;
    if(name=="--b")name="--blocks";if(name=="--t")name="--threads";if(name=="--p")name="--points";
    if(name=="--d")name="--device";
    if(!seen.insert(name).second)throw std::invalid_argument("Repeated Windows run option: "+name);
    if(name=="--once"){out.once=true;continue;}
    if(name=="--dry-run"){out.dryRun=true;continue;}
    if(name=="--preflight-only"){out.preflightOnly=true;continue;}
    const bool number=name=="--puzzle"||name=="--device"||name=="--blocks"||name=="--threads"||name=="--points"||name=="--duration-minutes";
    if(!number&&name!="--engine"&&name!="--backend"&&name!="--server")throw std::invalid_argument("Unsupported Windows run option: "+name);
    if(i+1==args.size()||args[i+1].empty()||args[i+1].find('\0')!=std::string::npos)throw std::invalid_argument("Missing or invalid value for "+name);
    const auto value=args[++i];
    if(number) {
      const auto n=cliIntegerArgument({name,value},{name},0);
      if(name=="--device"){if(n<0)throw std::invalid_argument("Device must be non-negative");out.device=n;}
      else {if(n<=0)throw std::invalid_argument("Run integer must be positive");
        if(name=="--puzzle"){if(positional)throw std::invalid_argument("Repeated puzzle number");out.puzzle=n;}
        if(name=="--blocks")out.blocks=n;if(name=="--threads")out.threads=n;if(name=="--points")out.points=n;
        if(name=="--duration-minutes"){if(n>360)throw std::invalid_argument("Duration must be 1..360 minutes");out.durationMinutes=n;}
      }
    }else if(name=="--server"){if(!client::HttpRequestPolicy::endpoint(value))throw std::invalid_argument("Invalid coordination URL");out.server=value;}
    else if(name=="--engine"){out.engine=canonical(value);if(out.engine!="bitcrack"&&out.engine!="keyhunt"&&out.engine!="kangaroo")throw std::invalid_argument("Unsupported search engine");}
    else {out.backend=canonical(value);if(out.backend!="cuda"&&out.backend!="opencl"&&out.backend!="cpu")throw std::invalid_argument("Unsupported backend");}
  }
  out.gpuOptionsSpecified=seen.count("--device")||seen.count("--blocks")||seen.count("--points");
  out.threadsSpecified=seen.count("--threads")!=0;
  if(out.engine=="keyhunt"){if(!out.backend.empty()&&out.backend!="cpu")throw std::invalid_argument("KeyHunt requires CPU");out.backend="cpu";}
  if(out.backend=="cpu"){if(!out.engine.empty()&&out.engine!="keyhunt")throw std::invalid_argument("CPU requires KeyHunt");out.engine="keyhunt";}
  if(out.engine=="kangaroo"&&!out.backend.empty()&&out.backend!="cuda")throw std::invalid_argument("Kangaroo requires CUDA");
  if(out.backend=="cpu"&&out.gpuOptionsSpecified)throw std::invalid_argument("GPU launch options do not apply to CPU");
  if(out.backend=="cpu"&&!seen.count("--threads"))out.threads=1;
  return out;
}
}
