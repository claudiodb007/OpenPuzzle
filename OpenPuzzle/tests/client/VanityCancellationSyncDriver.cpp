#include "openpuzzle/client/ExecutionSyncService.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace openpuzzle::client;
int main(int argc,char** argv) {
  try {
    if(argc!=4)throw std::runtime_error("action endpoint workspace");
    auto state=ClientStateStore::load();
    if(!state) {
      ClientExecutionState value;value.active=true;
      value.assignmentId="11111111-1111-4111-8111-111111111111";
      value.clientId="22222222-2222-4222-8222-222222222222";
      value.puzzle=69;value.rangeId=1;value.pid=1;value.processStartTime=1;
      value.bootId="00000000-0000-0000-0000-000000000000";
      value.engine="Vanity";value.backend="CUDA";value.command="fixture";
      value.start="123456789000000000";value.end="12345678900000ffff";
      value.target="1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH";value.workspace=argv[3];
      std::filesystem::create_directories(value.workspace);
      std::ofstream(std::filesystem::path(value.workspace)/"exit.code")<<"130\n";
      if(!ClientStateStore::save(value))throw std::runtime_error("save failed");state=value;
    }
    if(std::string(argv[1])=="request" && !ExecutionSyncService::requestCancellation(*state))throw std::runtime_error("intent failed");
    const auto result=ExecutionSyncService().tick(argv[2]);
    std::cout<<"{\"uploaded\":"<<result.completionUploaded<<",\"removed\":"<<result.stateRemoved
      <<",\"preserved\":"<<result.checkpointPreserved<<",\"exit_code\":"<<result.exitCode<<"}\n";
    return 0;
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
