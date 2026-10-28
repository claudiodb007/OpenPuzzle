#include "VanityCommonEngine.hpp"
#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/engines/EngineParserFactory.hpp"
#include "openpuzzle/runtime/ExecutionRequestBuilder.hpp"
#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include "openpuzzle/client/SolutionExporter.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <map>

using namespace openpuzzle;
static void need(bool ok){if(!ok)throw std::runtime_error("Common runtime test assertion failed");}
int main(int argc,char** argv){try{
  if(argc==2&&std::string(argv[1])=="parser"){
    EngineManager manager;need(!manager.registry().find("vanity"));
    need(manager.registry().find("bitcrack")&&manager.registry().find("keyhunt")&&manager.registry().find("kangaroo"));
    for(const auto& name:{"bitcrack","keyhunt","kangaroo"})need(bool(EngineParserFactory::create(name)));
    auto parser=EngineParserFactory::create("VaNiTy");need(bool(parser));
    need(!parser->parseLine("private engine diagnostic"));
    need(!parser->parseLine("OPV_CLIENT BEGIN total=300 resumed=100"));
    auto first=parser->parseLine("OPV_CLIENT PROGRESS 200 elapsed_us=100");need(first->keysChecked=="200"&&first->speedMKeys==1.0);
    auto second=parser->parseLine("OPV_CLIENT PROGRESS 300 elapsed_us=200");need(second->keysChecked=="300"&&second->speedMKeys==1.0);
    need(parser->parseLine("OPV_CLIENT RESULT complete checked=300 resumed=100 safe_stop=0 next_blocked=0")->finished);
    need(parser->parseLine("OPV_CLIENT PROGRESS 300")->error);
    for(const auto& bad:{"OPV_CLIENT PROGRESS 1","OPV_CLIENT BEGIN total=0 resumed=0","OPV_CLIENT BEGIN total=4 resumed=4","OPV_CLIENT BEGIN total=18446744073709551616 resumed=0"}){
      auto invalid=EngineParserFactory::create("vanity");need(invalid->parseLine(bad)->error);
      need(invalid->parseLine("OPV_CLIENT BEGIN total=4 resumed=0")->error);
    }
    auto partial=EngineParserFactory::create("vanity");partial->parseLine("OPV_CLIENT BEGIN total=20 resumed=0");
    partial->parseLine("OPV_CLIENT PROGRESS 10");
    need(partial->parseLine("OPV_CLIENT RESULT complete checked=10 resumed=0 safe_stop=0 next_blocked=0")->error);
    std::cout<<"Common engine/parser contracts passed\n";return 0;
  }
  need(argc>=4);std::string mode=argv[1];std::filesystem::path workspace=argv[2];std::string expected=argv[3];
  if(mode=="inspect"){
    auto p=client::ExecutionSyncService::latestProgress(workspace.string(),"Vanity");
    std::cout<<"{\"proof\":"<<(client::ExecutionSyncService::hasCompletionProof(workspace.string(),"Vanity",expected)?"true":"false")
      <<",\"progress\":"<<(p?"true":"false")<<",\"checked\":\""<<(p?p->keysChecked:"")<<"\"}\n";return 0;
  }
  if(mode=="sync"){
    need(argc==5);auto value=client::ExecutionSyncService().tick(argv[4]);
    std::cout<<"{\"running\":"<<(value.running?"true":"false")<<",\"uploaded\":"<<(value.completionUploaded?"true":"false")
      <<",\"removed\":"<<(value.stateRemoved?"true":"false")<<",\"solution\":"<<(value.solutionFound?"true":"false")
      <<",\"progress\":"<<(value.hasProgress?"true":"false")<<"}\n";return 0;
  }
  if(mode=="export"){
    auto state=client::ClientStateStore::load();need(bool(state));
    auto value=client::SolutionExporter::exportSolution(*state,(workspace/"found.txt").string());
    std::cout<<"{\"success\":"<<(value.success?"true":"false")<<"}\n";return value.success?0:1;
  }
  need(argc==13);VanityEngineConfiguration config{argv[4],argv[5],argv[6],argv[7]};
  EngineManager manager(config);need(manager.registry().find("vanity")&&manager.registry().count()==4);
  need(manager.resolveExecutable("Vanity","CUDA")==config.engine);need(!manager.resolveExecutable("Vanity","cpu"));
  const std::string start=argv[8],end=argv[9],target=argv[10],server=argv[11],clientId=argv[12];
  client::RangeAssignment a;a.assignmentId="8e9621a2-228d-4447-8126-e8aa000f9b2e";a.puzzle=65;a.rangeId=37;
  a.start=start;a.end=end;a.target=target;a.requiredBackend="cuda";
  if(mode=="claim-start"){
    client::HttpRangeClient http(server);auto claimed=http.claimResult(clientId,65,1,2500,"cuda","linear");
    need(claimed.assignment.has_value());a=*claimed.assignment;
    need(a.start==start&&a.end==end&&a.target==target&&a.searchMode=="linear");
  }
  PuzzleRecord puzzle;puzzle.id=a.puzzle;puzzle.number=a.puzzle;puzzle.address=a.target;
  RangeRecord range;range.id=a.rangeId;range.startKey=a.start;range.endKey=a.end;
  JobRecord job;job.id=37;
  WorkerEngineCapability capability;capability.engine="Vanity";capability.backend="CUDA";capability.available=true;
  capability.device=0;capability.blocks=1024;capability.threads=256;capability.points=1024;
  ExecutionRequestBuilder builder(manager);auto request=builder.build(puzzle,range,job,capability,config.engine,workspace.string());
  if(mode=="build"){std::cout<<request.command<<'\n';return 0;}
  need(mode=="start"||mode=="claim-start");auto handle=BackgroundExecutionLauncher().start(request);
  client::ClientExecutionState state;state.active=true;state.assignmentId=a.assignmentId;state.clientId=clientId;
  state.puzzle=a.puzzle;state.rangeId=a.rangeId;state.pid=handle.pid;state.bootId=client::ClientStateStore::currentBootId();
  state.processStartTime=LinuxProcessIdentity::startTime(handle.pid).value_or(0);need(state.processStartTime>0);
  state.workspace=workspace.string();state.engine="Vanity";state.backend="cuda";state.start=start;state.end=end;state.target=target;
  state.blocks=1024;state.threads=256;state.points=1024;state.command=request.command;
  need(client::ClientStateStore::save(state));std::cout<<"{\"pid\":"<<handle.pid<<"}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}}
