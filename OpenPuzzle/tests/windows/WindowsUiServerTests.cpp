#include "openpuzzle/ui/WindowsUiModel.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include <windows.h>
#include <fstream>
#include <iostream>
using namespace openpuzzle;using namespace openpuzzle::ui;namespace fs=std::filesystem;
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int wmain(){try{
 const auto root=fs::temp_directory_path()/pathFromUtf8("op-ui-server-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));fs::create_directory(root);
 require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()),"Server fixture environment failed");
 const auto legacy=localPaths::configDirectory()/"windows-ui.json";WorkspaceSecurity::prepare(legacy.parent_path());
 const std::string contents="{\"version\":1,\"server\":\"http://127.0.0.1:9\"}\n";require(writePrivateFileAtomically(legacy,contents),"Legacy fixture setup failed");
 WindowsUiSelection selection;WindowsRunProfile profile;profile.backend="cpu";selection.profile=profile;selection.profilePath="C:\\profiles\\result.json";
 for(const wchar_t* overrideValue:{L"http://127.0.0.1:1",L"https://other.test",L"invalid-url",L""}){
  require(SetEnvironmentVariableW(L"OPENPUZZLE_SERVER_URL",overrideValue),"Override fixture failed");selection.server="https://other.test";
  const auto args=windowsUiStartCommand(selection);const auto at=std::find(args.begin(),args.end(),"--server");
  require(at!=args.end()&&*(at+1)=="https://claudiodb.com","UI coordinator was overridden");
  std::vector<std::string> plain;for(std::size_t i=0;i<args.size();++i){if(args[i]=="--slot"){++i;continue;}plain.push_back(args[i]);}
  require(RunCommandOptions::parse(plain).server==windowsUiServer,"CLI would receive a different coordinator");
  const auto preflight=windowsUiCommand(UiAction::Preflight,selection);const auto local=std::find(preflight.begin(),preflight.end(),"--server");
  require(local!=preflight.end()&&*(local+1)=="http://127.0.0.1:1"&&preflight.back()=="--dry-run","Preflight was converted to a production request");
 }
 std::ifstream input(legacy);const std::string after{std::istreambuf_iterator<char>(input),{}};
 require(after==contents&&!fs::exists(localPaths::configDirectory()/"client.id")&&!fs::exists(localPaths::dataDirectory()),"Server policy changed preferences, identity or runtime state");
 std::cout<<"WindowsUiServerTests passed: fixed official UI coordinator, ignored environment/legacy overrides, literal CLI server and local-only preflight\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
