#include "openpuzzle/runtime/RunCommandOptions.hpp"
#include <iostream>
#include <stdexcept>
using namespace openpuzzle;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 auto defaults=RunCommandOptions::parse({});require(defaults.puzzle==0&&defaults.command=="run","Automatic selection must remain puzzle zero");
 auto gpu=RunCommandOptions::parse({"run","71","--once","--backend","OpenCL","--device","2","--b","256","--t","128","--p","1024","--duration-minutes","5","--server","http://127.0.0.1:1234"});
 require(gpu.puzzle==71&&gpu.once&&gpu.backend=="opencl"&&gpu.device==2&&gpu.blocks==256&&gpu.threads==128&&gpu.points==1024&&gpu.durationMinutes==5&&gpu.gpuOptionsSpecified&&gpu.threadsSpecified,"Validated GPU arguments changed");
 auto cpu=RunCommandOptions::parse({"--backend","cpu","--threads","4","--dry-run"});require(cpu.engine=="keyhunt"&&cpu.backend=="cpu"&&cpu.threads==4&&cpu.dryRun,"CPU routing changed");
 require(RunCommandOptions::parse({"run","--engine","keyhunt"}).threads==1,"Unspecified CPU threads should be conservative");
 require(RunCommandOptions::parse({"71","--preflight-only"}).preflightOnly,"Direct puzzle routing failed");
 for(const auto& cmd:{"status","stop","safestop","help","version"})require(RunCommandOptions::parse({cmd}).command==cmd,"Local control routing failed");
 require(RunCommandOptions::parse({"--help"}).command=="help"&&RunCommandOptions::parse({"-V"}).command=="version","Local information flags failed");
 for(const auto& args:std::vector<std::vector<std::string>>{
   {""},{std::string("run\0x",5)},{"unsupported"},{"run","--unknown"},{"status","--server","http://127.0.0.1"},
   {"run","0"},{"run","-1"},{"run","71","72"},{"run","71","--puzzle","72"},
   {"run","--puzzle","71","--puzzle","72"},{"run","--device"},{"run","--device","-1"},{"run","--device","2x"},
   {"run","--device","2147483648"},{"run","--b","1","--blocks","2"},{"run","--threads","0"},
   {"run","--duration-minutes","361"},{"run","--duration-minutes","1.0"},{"run","--once","--once"},
   {"run","--engine","bitcrack","--backend","cpu"},{"run","--engine","keyhunt","--backend","cuda"},
   {"run","--engine","kangaroo","--backend","opencl"},{"run","--backend","cpu","--device","0"},
   {"run","--server","file:///x"},{"run","--devices","all"},{"run","--server","https://user@host/"},
   {"run","--engine","bitcrack","--engine","bitcrack"},{"run","--backend","cpu","--blocks","1"}}) {
   bool refused=false;try{(void)RunCommandOptions::parse(args);}catch(const std::exception&){refused=true;}require(refused,"Invalid/ambiguous argument was accepted");
 }
 std::cout<<"RunCommandOptionsTests passed: strict options, aliases, automatic puzzle and local command routing\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
