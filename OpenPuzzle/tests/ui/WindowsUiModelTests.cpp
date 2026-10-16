#include "openpuzzle/ui/WindowsUiModel.hpp"
#include "openpuzzle/core/WindowsCommandLine.hpp"
#include "openpuzzle/services/PuzzleMetadataCatalog.hpp"
#include <iostream>
#include <fstream>
using namespace openpuzzle;using namespace openpuzzle::ui;
void check(bool b){if(!b)throw std::runtime_error("Windows UI model contract failed");}
int main(int argc,char** argv){try{
 check(std::string(windowsUiServer)=="https://claudiodb.com");
 WindowsUiSelection s;s.server="http://127.0.0.1:1";s.profilePath="C:\\Perfis com espaços & dados\\result.json";
 WindowsRunProfile p;p.backend="cuda";p.device=3;p.threads=256;p.blocks=256;p.points=1024;p.gpuUuid="1f78bee1-b3b1-e9d5-78a7-00cdaa490619";s.profile=p;s.slot="cuda-3";
 for(const auto& backend:{"cpu","cuda","opencl"}){
  auto measured=p;measured.backend=backend;measured.device=measured.backend=="cpu"?0:3;measured.threads=measured.backend=="cpu"?1:256;s.profile=measured;
  const auto preview=windowsUiCommand(UiAction::BenchmarkPreview,s),real=windowsUiCommand(UiAction::Benchmark,s);
  check(preview[0]=="benchmark"&&preview[2]==backend&&real.back()=="--real"&&real.size()==preview.size()+1);
  for(const auto& flag:{"--slot","--profile","--server","--real"})check(std::find(preview.begin(),preview.end(),flag)==preview.end());
  check((std::find(preview.begin(),preview.end(),"--blocks")!=preview.end())==(measured.backend!="cpu"));
 }
 s.profile=p;
 check(windowsUiBenchmarkReport("Report: C:\\Private\\run-1\r\nLocal validation: PASSED\r\n")=="C:\\Private\\run-1");
 for(const auto& bad:{"no report", "Report: one\nReport: two\n"}){bool rejected=false;try{windowsUiBenchmarkReport(bad);}catch(...){rejected=true;}check(rejected);}
 auto fixed=windowsUiStartCommand(s);const auto serverFlag=std::find(fixed.begin(),fixed.end(),"--server");check(serverFlag!=fixed.end()&&*(serverFlag+1)==windowsUiServer);
 auto args=windowsUiCommand(UiAction::Start,s);std::vector<std::string> runArgs;
 for(std::size_t i=0;i<args.size();++i){if(args[i]=="--slot"){++i;continue;}runArgs.push_back(args[i]);}
 auto parsed=RunCommandOptions::parse(runArgs);check(parsed.profile==s.profilePath&&parsed.server==s.server&&parsed.puzzle==71&&!parsed.dryRun&&!parsed.once);
 auto preflight=windowsUiCommand(UiAction::Preflight,s);check(preflight.back()=="--dry-run"&&preflight[1]=="--profile");
 runArgs.clear();for(std::size_t i=0;i<preflight.size();++i){if(preflight[i]=="--slot"){++i;continue;}runArgs.push_back(preflight[i]);}
 parsed=RunCommandOptions::parse(runArgs);check(parsed.puzzle==0&&parsed.dryRun&&parsed.profile==s.profilePath);
 const auto originalServer=s.server;for(const auto& server:{"", "invalid", "https://production.example"}){
  s.server=server;preflight=windowsUiCommand(UiAction::Preflight,s);check(preflight.back()=="--dry-run");
  const auto at=std::find(preflight.begin(),preflight.end(),"--server");check(at!=preflight.end()&&*(at+1)=="http://127.0.0.1:1");
 }s.server=originalServer;
 s.puzzle=0;s.duration=0;check(windowsUiCommand(UiAction::Preflight,s).back()=="--dry-run");s.puzzle=71;s.duration=60;
 check(argc==2);std::ifstream metadataFile(argv[1]);check(metadataFile.good());
 const auto metadata=PuzzleMetadataCatalog::parse(71,std::string(std::istreambuf_iterator<char>(metadataFile),{}));
 check(metadata&&metadata->puzzle==71&&metadata->searchMode=="linear"&&metadata->requiredBackend.empty());
 check(metadata->address=="1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU"&&metadata->keyspace=="400000000000000000:7FFFFFFFFFFFFFFFFF");
 check(windowsUiCommand(UiAction::Stop,s)==std::vector<std::string>({"stop","--slot","cuda-3"}));
 check(windowsUiCommand(UiAction::SafeStop,s)[0]=="safestop");check(windowsUiCommand(UiAction::Recover,s)[0]=="recover");
 check(windowsUiCommand(UiAction::Status,s)==std::vector<std::string>({"status","--slot","cuda-3"}));
 s.profile->backend="opencl";s.slot="opencl-3";check(profileSlot(*s.profile)==s.slot);check(windowsUiCommand(UiAction::Start,s).back()==s.slot);
 const auto reject=[&]{bool failed=false;try{windowsUiCommand(UiAction::Start,s);}catch(...){failed=true;}check(failed);};
 s.slot="primary";reject();s.slot="opencl-3";s.profile->gpuUuid.clear();reject();s.profile->pciBusId="00000000:08:00.0";
 s.server="https://server.test/#bad";reject();s.server="http://127.0.0.1:1";s.duration=361;reject();s.duration=60;s.profile.reset();reject();
 s.profile=p;s.slot="cuda-3";s.profilePath=std::string("bad\0path",8);reject();
 auto status=WindowsUiStatus::parse(0,"Slot: cuda-3\r\nRuntime: idle\r\nAssignment: none\r\nPending claim: none\r\n","cuda-3");check(status.canStart());
 check(!WindowsUiStatus::parse(0,"Slot: other\nRuntime: idle\nAssignment: none\nPending claim: none\n","cuda-3").known);
 check(!WindowsUiStatus::parse(0,"Slot: cuda-3\nRuntime: unknown\nAssignment: none\nPending claim: none\n","cuda-3").canStart());
 check(!WindowsUiStatus::parse(1,"Slot: cuda-3\nRuntime: idle\nAssignment: none\nPending claim: none\n","cuda-3").known);
 check(!WindowsUiStatus::parse(0,"Slot: cuda-3\nRuntime: idle\nRuntime: running\nAssignment: none\nPending claim: none\n","cuda-3").known);
 check(WindowsUiStatus::parse(2,"Slot: cuda-3\nRuntime: idle\nAssignment: job\nPending claim: preserved\n","cuda-3").preserved);
 check(WindowsUiStatus::parse(0,"Slot: cuda-3\nRuntime: running\nAssignment: job\nPending claim: none\n","cuda-3").running);
 s.thermal={true,true,75,85};check(windowsUiCommand(UiAction::ThermalSave,s)[1]=="--enable");s.thermal.criticalC=74;
 check(windowsUiThermalThreshold("75,5")==75.5&&windowsUiThermalThreshold("85.5")==85.5);
 bool rejected=false;try{windowsUiCommand(UiAction::ThermalSave,s);}catch(...){rejected=true;}check(rejected);
 check(WindowsCommandLine::build("C:\\Program Files\\OpenPuzzle\\openpuzzle.exe",{"--profile","C:\\Olá & literal %PATH%\\x.json"}).find("%PATH%")!=std::string::npos);
 std::cout<<"WindowsUiModelTests passed: profile routing, local preflight, preserved/unknown states, thermal limits\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
