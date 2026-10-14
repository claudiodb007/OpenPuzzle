#include "WindowsRunRequestFixture.hpp"
#include "openpuzzle/core/WindowsCliApplication.hpp"
#include "openpuzzle/runtime/WindowsAssignmentRecovery.hpp"
#include "openpuzzle/engines/WindowsEngineDiagnostics.hpp"
#include "openpuzzle/engines/WindowsRunProfile.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/ui/WindowsUiModel.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <fstream>
#include <iostream>
using namespace openpuzzle;namespace fs=std::filesystem;
std::string read(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
bool endpoint(const std::string& r,const char* p){return r.starts_with(std::string("POST ")+p+' ');}
int wmain(int argc,wchar_t** argv){try{
 require(argc==2||argc==3||((argc==4||argc==5)&&std::wstring(argv[3])==L"--thermal"&&(argc!=5||std::wstring(argv[4])==L"--ui")),"Supply a fixture executable or real engine/profile directories and optional --thermal");Winsock sockets;
 const bool real=argc>=3,ui=argc==5;const auto root=fs::temp_directory_path()/pathFromUtf8("op-native-run-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));
 require(!fs::exists(root),"Fixture directory already exists");fs::create_directory(root);
 require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str())&&SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT","primary"),"Run environment failed");
 if(argc>=4){Configuration config;config.gpu.thermal={true,true,75,85};require(ConfigurationManager::save(config),"Isolated thermal configuration failed");std::cout<<"Physical thermal protection enabled only in the synthetic fixture profile\n";}
 const auto folder=root/"engines";fs::create_directory(folder);for(const auto n:{"keyhunt.exe","cuBitCrack.exe","clBitCrack.exe"}){const auto source=real?fs::absolute(argv[1])/n:fs::absolute(argv[1]);fs::copy_file(source,folder/n);require(WindowsEngineDiagnostics::executableSha256(source)==WindowsEngineDiagnostics::executableSha256(folder/n),"Synthetic engine copy changed bytes");}
 const std::string target="1cMh228HTCiwS8ZsaakH8A8wze1JR5ZsP";
 std::ostringstream output;
 for(const auto backend:{"cpu","cuda","opencl"}){
  const auto engine=folder/(std::string(backend)=="cpu"?"keyhunt.exe":std::string(backend)=="cuda"?"cuBitCrack.exe":"clBitCrack.exe");
  fs::path profile;
  if(real){for(const auto& entry:fs::recursive_directory_iterator(fs::absolute(argv[2])))if(entry.path().filename()=="result.json"){
    std::optional<WindowsRunProfile> parsed;try{parsed=WindowsRunProfile::parse(read(entry.path()));}catch(const std::exception&){continue;}
    if(parsed->backend==backend&&parsed->sha256==WindowsEngineDiagnostics::executableSha256(engine)){require(profile.empty(),"Multiple measured profiles for the same backend; select the prior diagnostics directory");profile=entry.path();}
   }require(!profile.empty(),"Missing sustained native benchmark profile");
  }else{
   profile=root/(std::string(backend)+".json");const auto device=WindowsEngineDiagnostics::inspectDevice(engine,backend,0,1);
   const auto record="{\"version\":1,\"backend\":\""+std::string(backend)+"\",\"engine_sha256\":\""+WindowsEngineDiagnostics::executableSha256(engine)+"\",\"device\":0,\"device_identity\":\""+device+"\",\"selftest_passed\":true,\"threads\":"+(std::string(backend)=="cpu"?"1":"32")+",\"blocks\":1,\"points\":1,\"samples\":3,\"duration_seconds\":10,\"speed_mkeys\":1}";
   require(writePrivateFileAtomically(profile,record),"Profile fixture write failed");
  }
  WindowsCliApplication app({},WindowsGpuDiscovery::inspect,folder);
  openpuzzle::ui::WindowsUiSelection selection;selection.profilePath=pathUtf8(profile);selection.profile=WindowsRunProfile::parse(read(profile));selection.slot=ui?openpuzzle::ui::profileSlot(*selection.profile):"primary";selection.puzzle=20;selection.once=true;
  const auto slot=selection.slot;
  auto runArguments=[&](const std::string& url,bool preflight=false){if(ui){selection.server=url;return openpuzzle::ui::windowsUiCommand(preflight?openpuzzle::ui::UiAction::Preflight:openpuzzle::ui::UiAction::Start,selection);}return preflight?std::vector<std::string>{"run","--profile",pathUtf8(profile),"--server",url,"--dry-run"}:std::vector<std::string>{"run","--profile",pathUtf8(profile),"--server",url,"--once"};};
  auto recover=[&](const std::string& url,int expected){
   std::ostringstream recoveryOutput;int code;
   if(ui){
    require(client::ClientStateStore::executionSlot()=="primary","Run did not restore the parent execution slot");
    selection.server=url;code=app.run(openpuzzle::ui::windowsUiCommand(openpuzzle::ui::UiAction::Recover,selection),recoveryOutput);
    require(client::ClientStateStore::executionSlot()=="primary","UI recovery did not restore the parent execution slot");
   }else code=recoverWindowsAssignment(slot,url,recoveryOutput);
   if(code!=expected)std::cerr<<"Recovery slot: "<<slot<<'\n'<<recoveryOutput.str();
   return code;
  };
  const auto identityBefore=read(localPaths::configDirectory()/"client.id");
  require(app.run(runArguments(ui?"":"http://127.0.0.1:1",true),output)==0,"Measured profile preflight failed");
  require(read(localPaths::configDirectory()/"client.id")==identityBefore,"Preflight changed the client identity");
  if(ui){
   const auto metadataFile=folder.parent_path()/"puzzles"/"20.json";
   if(!fs::exists(metadataFile)){
    require(app.run(runArguments("http://127.0.0.1:1"),output)==1&&read(localPaths::configDirectory()/"client.id")==identityBefore&&!fs::exists(WindowsRunSession::pendingClaimPath(slot)),"UI missing metadata did not fail before identity/claim");
    output.str("");output.clear();std::cout<<backend<<": UI missing metadata blocked before identity/claim PASSED\n";
    WorkspaceSecurity::prepare(metadataFile.parent_path());
    require(writePrivateFileAtomically(metadataFile,"{\"number\":20,\"search_mode\":\"linear\",\"address\":\""+target+"\",\"keyspace\":\"1:2\"}"),"Synthetic UI metadata publication failed");
   }
  }
  for(const bool solution:{false,true}){
   int claims=0,completed=0,reported=0;const auto id=std::string(backend)+(solution?"-solution":"-complete");
   RunServer server([&](const auto& request){
    require(request.find("private_key")==std::string::npos&&request.find("KwDiBf")==std::string::npos&&request.find("Private Key:")==std::string::npos,"Native run leaked local solution data");
    if(endpoint(request,"/api/range/claim")){++claims;return "{\"assignment_id\":\""+id+"\",\"puzzle\":20,\"range_id\":1,\"target\":\""+target+"\",\"start\":\""+(solution?"2":"1")+"\",\"end\":\""+(solution?"2":"1")+"\"}";}
    if(endpoint(request,"/api/range/complete")){++completed;require(request.find("\"keys_checked\":\"1\"")!=std::string::npos&&request.find("\"status\":\"completed\"")!=std::string::npos,"Exact native coverage was not uploaded");}
    else if(endpoint(request,"/api/puzzle/report-solution"))++reported;
    else require(endpoint(request,"/api/client/register")||endpoint(request,"/api/client/heartbeat")||endpoint(request,"/api/range/progress"),"Unexpected local integration endpoint");
    return std::string("{}");
   });
   const auto code=app.run(runArguments(server.url()),output);
   if(code!=0)std::cerr<<output.str();
   require(code==0,"Native assignment run failed; see captured session output");
   if(solution)require(recover(server.url(),0)==0,"Native solution recovery failed");
   server.finish();require(claims==1&&completed==(solution?0:1)&&reported==(solution?2:0),"Native run duplicated a claim or confused solution/completion");
   const auto state=client::ClientStateStore::load(slot);const auto pending=WindowsRunSession::pendingClaimPath(slot);
   if(solution){
    require(state&&fs::exists(pending)&&fs::exists(localPaths::root()/"solutions"/"Puzzle-20"/id/"wallet-import.txt"),"Native solution was not exported and preserved");
    const auto receipt=read(pending);require(app.run(runArguments(server.url()),output)==1&&read(pending)==receipt,"Preserved solution allowed a new claim");
    // Existing files remain until explicit fixture cleanup. Recovery repeats
    // only the notification; original private solution is not uploaded.
    int notified=0;RunServer retry([&](const auto& request){require(endpoint(request,"/api/puzzle/report-solution"),"Solution recovery requested other work");++notified;return std::string("{}");});
    require(recover(retry.url(),1)==1,"Recovery accepted a different server");retry.finish();require(notified==0&&read(pending)==receipt,"Server mismatch changed local data");
    require(client::ClientStateStore::remove(slot),"Fixture solution state cleanup failed");fs::remove(pending);
   }else require(!state&&!fs::exists(pending),"Acknowledged completion retained pending state");
   std::cout<<backend<<": "<<(solution?"private solution export and claim blocking":"exact one-key completion")<<" PASSED\n";
  }
 }
 if(ui)std::cout<<"UI command route PASSED: empty-server local preflight, explicit puzzle metadata, exact one-key launch, selected-slot solution recovery and preserved-work blocking\n";
 std::cout<<"WindowsNativeAssignmentTests passed: all three engines, exact coverage, protected exports and one claim per run\n";
 // Keep fixture logs and known-key synthetic exports for review. They contain
 // only the public test scalar 2; no production target or wallet is involved.
 std::cout<<"Synthetic report: "<<pathUtf8(root)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"Native assignment validation failed: "<<e.what()<<'\n';return 1;}}
