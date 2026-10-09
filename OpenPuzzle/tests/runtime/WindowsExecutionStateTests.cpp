#include "openpuzzle/runtime/WindowsExecutionState.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace openpuzzle;
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
int main() {
  namespace fs=std::filesystem;
  fs::path root;
  try {
    root=fs::temp_directory_path()/pathFromUtf8("OpenPuzzle-state-ç-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
#ifdef _WIN32
    require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()),"Fixture environment failed");
#else
    require(setenv("HOME",root.c_str(),1)==0,"Fixture environment failed");
#endif
    client::ClientExecutionState state;
    state.active=true;state.assignmentId="fixture assignment ç\nvalue";state.clientId="fixture-client";
    state.puzzle=20;state.rangeId=1;state.workspace=pathUtf8(root/"workspace");state.bootId="legacy-linux";
    const WindowsEngineRecord record{{123,134000000000000000ULL},"generation-1"};
    const WindowsRuntimeRecord owner{456,134000000000000001ULL};
    require(bindWindowsExecutionState(state,record,owner)&&state.valid(),"Binding rejected valid metadata");
    require(state.bootId.empty()&&windowsExecutionRecord(state)==record,"Windows discriminator lost");
    require(client::ClientStateStore::save(state,"cuda-5"),"State publication failed");
    auto loaded=client::ClientStateStore::load("cuda-5");
    require(loaded&&windowsExecutionRecord(*loaded)==record&&loaded->ownerPid==owner.pid&&
            loaded->ownerStartTime==owner.creationTime&&loaded->assignmentId==state.assignmentId&&loaded->workspace==state.workspace,"Identity or UTF-8 state roundtrip failed");
    auto invalid=state;invalid.processGeneration="../old";require(!windowsExecutionRecord(invalid),"Generation path traversal accepted");
    invalid=state;invalid.processPlatform="linux-proc-starttime-v1";require(!windowsExecutionRecord(invalid),"Foreign process platform accepted");
    invalid=state;invalid.bootId="linux-boot";require(!windowsExecutionRecord(invalid),"Mixed boot identity accepted");
    invalid=state;invalid.ownerStartTime=0;require(!windowsExecutionRecord(invalid),"Missing supervisor identity accepted");
    invalid=state;require(!bindWindowsExecutionState(invalid,{{0,1},"bad"},owner)&&invalid.processGeneration==state.processGeneration,"Failed binding mutated prior state");
    auto legacy=state;legacy.processPlatform.clear();legacy.processGeneration.clear();legacy.ownerPid=0;legacy.ownerStartTime=0;legacy.bootId="legacy-linux";
    require(client::ClientStateStore::save(legacy,"primary"),"Legacy state save failed");
    loaded=client::ClientStateStore::load("primary");require(loaded&&loaded->bootId=="legacy-linux"&&!windowsExecutionRecord(*loaded),"Legacy state silently became Windows");
    fs::remove_all(root);
    std::cout<<"WindowsExecutionStateTests passed: explicit engine/supervisor identities, private roundtrip and rejected mixed generations\n";
    return 0;
  } catch(const std::exception& e) {
    if(!root.empty()){std::error_code ec;fs::remove_all(root,ec);}
    std::cerr<<e.what()<<'\n';return 1;
  }
}
