#include "openpuzzle/ui/WindowsUiWindow.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include <fstream>
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/ui/WindowsUiLanguageStore.hpp"
#include "openpuzzle/ui/WindowsUiProfileStore.hpp"
#include <windows.h>
#include <iostream>
#include <optional>
struct Environment {
 const wchar_t* name;std::optional<std::wstring> previous;
 explicit Environment(const wchar_t* value):name(value){const auto size=GetEnvironmentVariableW(name,nullptr,0);if(size){std::wstring text(size,L'\0');const auto n=GetEnvironmentVariableW(name,text.data(),size);if(!n||n>=size)throw std::runtime_error("Environment changed");text.resize(n);previous=text;}}
 ~Environment(){SetEnvironmentVariableW(name,previous?previous->c_str():nullptr);}
};
struct RegistryFixture {
 std::wstring key=L"Software\\OpenPuzzle\\Tests\\UiLanguage-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
 ~RegistryFixture(){RegDeleteTreeW(HKEY_CURRENT_USER,key.c_str());}
};
int wmain(int argc,wchar_t** argv){try{
 if(argc!=2)return 1;Environment data(L"LOCALAPPDATA"),server(L"OPENPUZZLE_SERVER_URL");
 RegistryFixture registry;using namespace openpuzzle::ui;WindowsUiLanguageStore preferences(registry.key);
 if(preferences.load()!=UiLanguage::English)throw std::runtime_error("Missing language preference fallback failed");
 HKEY probe=nullptr;if(RegOpenKeyExW(HKEY_CURRENT_USER,registry.key.c_str(),0,KEY_READ,&probe)!=ERROR_FILE_NOT_FOUND){if(probe)RegCloseKey(probe);throw std::runtime_error("Reading language created a registry key");}
 for(unsigned i=0;i<4;++i){const auto language=static_cast<UiLanguage>(i);if(!preferences.save(language)||WindowsUiLanguageStore(registry.key).load()!=language)throw std::runtime_error("Saved language not restored");}
 if(preferences.save(static_cast<UiLanguage>(99))||preferences.load()!=UiLanguage::Spanish)throw std::runtime_error("Invalid preference changed stored language");
 if(RegOpenKeyExW(HKEY_CURRENT_USER,registry.key.c_str(),0,KEY_SET_VALUE,&probe)!=ERROR_SUCCESS)throw std::runtime_error("Cannot prepare malformed preference");
 const wchar_t invalid[]{L'p',L't',0,L'f',L'r',0};const auto written=RegSetValueExW(probe,L"Language",0,REG_SZ,reinterpret_cast<const BYTE*>(invalid),sizeof(invalid));RegCloseKey(probe);
 if(written!=ERROR_SUCCESS||preferences.load()!=UiLanguage::English)throw std::runtime_error("Embedded NUL language accepted");
 if(!preferences.save(UiLanguage::Portuguese))throw std::runtime_error("Cannot prepare persisted UI language");
 const auto root=std::filesystem::temp_directory_path()/openpuzzle::pathFromUtf8("op-ui-window-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));std::filesystem::create_directory(root);
 if(!SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str())||!SetEnvironmentVariableW(L"OPENPUZZLE_SERVER_URL",L"invalid-url"))throw std::runtime_error("Window fixture environment failed");
 const auto legacy=openpuzzle::localPaths::configDirectory()/"windows-ui.json";openpuzzle::WorkspaceSecurity::prepare(legacy.parent_path());
 const std::string old="legacy-preferences-preserved";if(!openpuzzle::writePrivateFileAtomically(legacy,old))throw std::runtime_error("Legacy fixture publication failed");
 const auto fixtureProfile=root/"original-profile.json";
 const auto measured="{\"version\":1,\"backend\":\"cpu\",\"engine_sha256\":\""+std::string(64,'a')+"\",\"device\":0,\"device_identity\":\"CPU fixture\",\"selftest_passed\":true,\"threads\":1,\"blocks\":256,\"points\":256,\"speed_mkeys\":1.0,\"samples\":3,\"duration_seconds\":30}\n";
 if(!openpuzzle::writePrivateFileAtomically(fixtureProfile,measured))throw std::runtime_error("Fixture profile preparation failed");
 // First run starts without any profile. The second run must discover the
 // saved profile without measuring again. Both use the synthetic CLI fixture.
 const auto failedMarker=openpuzzle::localPaths::dataDirectory()/"ui-benchmark-fail";
 openpuzzle::WorkspaceSecurity::prepare(failedMarker.parent_path());
 if(!openpuzzle::writePrivateFileAtomically(failedMarker,"expected failure"))throw std::runtime_error("Cannot prepare failure fixture");
 if(openpuzzle::ui::runWindowsUi(std::filesystem::absolute(argv[1]),true,registry.key,{})==0||std::filesystem::exists(openpuzzle::localPaths::dataDirectory()/"ui-profiles-fixture"/"profiles"/"cpu"/"result.json"))throw std::runtime_error("Failed measurement produced a selected/saved profile");
 std::filesystem::remove(failedMarker);
 for(int pass=0;pass<2;++pass)if(openpuzzle::ui::runWindowsUi(std::filesystem::absolute(argv[1]),true,registry.key,{})!=0)throw std::runtime_error("First-profile create/reopen fixture failed");
 const auto saved=openpuzzle::localPaths::dataDirectory()/"ui-profiles-fixture"/"profiles"/"cpu"/"result.json";
 bool absent=false;const auto savedBefore=openpuzzle::windowsFile::readPrivate(saved,absent,16384);
 if(!savedBefore||openpuzzle::WindowsRunProfile::parse(*savedBefore).speed!=2.0)throw std::runtime_error("First profile was not persisted");
 // Valid or damaged existing profiles are preserved byte-identically.
 if(openpuzzle::ui::storeFirstWindowsUiProfile(saved.parent_path().parent_path().parent_path(),fixtureProfile)!=fixtureProfile)throw std::runtime_error("Existing profile was replaced");
 if(!openpuzzle::writePrivateFileAtomically(saved,"damaged-existing-profile"))throw std::runtime_error("Cannot create damaged destination fixture");
 if(openpuzzle::ui::storeFirstWindowsUiProfile(saved.parent_path().parent_path().parent_path(),fixtureProfile)!=fixtureProfile)throw std::runtime_error("Damaged profile was replaced");
 const auto damaged=openpuzzle::windowsFile::readPrivate(saved,absent,16384);
 if(!damaged||*damaged!="damaged-existing-profile")throw std::runtime_error("Existing damaged profile changed");
 if(!openpuzzle::writePrivateFileAtomically(saved,*savedBefore))throw std::runtime_error("Cannot restore fixture");
 // GPU persistence requires a real measured record with physical binding.
 auto gpuJson=measured;gpuJson.replace(gpuJson.find("\"backend\":\"cpu\""),15,"\"backend\":\"cuda\"");
 const auto threadsAt=gpuJson.find("\"threads\":1");gpuJson.replace(threadsAt,11,"\"threads\":256");
 const auto gpuSource=root/"gpu-measurement.json";
 if(!openpuzzle::writePrivateFileAtomically(gpuSource,gpuJson))throw std::runtime_error("Cannot prepare unbound GPU result");
 const auto gpuStore=root/"gpu-profile-storage";bool unboundRejected=false;
 try{openpuzzle::ui::storeFirstWindowsUiProfile(gpuStore,gpuSource);}catch(...){unboundRejected=true;}
 if(!unboundRejected||std::filesystem::exists(gpuStore/"profiles"/"cuda"/"result.json"))throw std::runtime_error("Unbound GPU result was persisted");
 const auto end=gpuJson.rfind('}');gpuJson.insert(end,",\"gpu_uuid\":\"1f78bee1-b3b1-e9d5-78a7-00cdaa490619\",\"pci_bus_id\":\"00000000:08:00.0\"");
 if(!openpuzzle::writePrivateFileAtomically(gpuSource,gpuJson))throw std::runtime_error("Cannot prepare bound GPU result");
 const auto gpuSaved=openpuzzle::ui::storeFirstWindowsUiProfile(gpuStore,gpuSource);
 const auto gpuContents=openpuzzle::windowsFile::readPrivate(gpuSaved,absent,16384);
 if(!gpuContents||*gpuContents!=gpuJson||openpuzzle::WindowsRunProfile::parse(*gpuContents).gpuUuid.empty())throw std::runtime_error("Bound GPU profile was not persisted byte-identically");
 // Complete GPU UI route: automatic arguments, finalist provenance, physical
 // binding, versioned persistence, reopening and failed-run preservation.
 for(const auto backend:{"cuda","opencl"}){
  const auto store=openpuzzle::localPaths::dataDirectory()/openpuzzle::pathFromUtf8(std::string("ui-profiles-fixture-")+backend);
  for(const auto marker:{"ui-auto-finalist-fail","ui-auto-unvalidated"}){
   const auto path=openpuzzle::localPaths::dataDirectory()/marker;
   if(!openpuzzle::writePrivateFileAtomically(path,"expected failure"))throw std::runtime_error("Cannot prepare automatic failure fixture");
   if(openpuzzle::ui::runWindowsUi(std::filesystem::absolute(argv[1]),true,registry.key,{},backend)==0||openpuzzle::ui::loadWindowsUiAutomaticProfile(store,backend))throw std::runtime_error("Failed/unvalidated automatic result was persisted");
   std::filesystem::remove(path);
  }
  for(int pass=0;pass<2;++pass)if(openpuzzle::ui::runWindowsUi(std::filesystem::absolute(argv[1]),true,registry.key,{},backend)!=0)throw std::runtime_error("Automatic GPU create/reopen fixture failed");
  const auto first=openpuzzle::ui::loadWindowsUiAutomaticProfile(store,backend);if(!first)throw std::runtime_error("Automatic selection missing");
  const auto bytes=openpuzzle::windowsFile::readPrivate(*first,absent,16384);if(!bytes)throw std::runtime_error("Cannot read automatic selection");
  const auto second=openpuzzle::ui::storeWindowsUiAutomaticProfile(store,*first);
  if(second==*first||openpuzzle::windowsFile::readPrivate(*first,absent,16384)!=bytes||openpuzzle::ui::loadWindowsUiAutomaticProfile(store,backend)!=second)throw std::runtime_error("Automatic remeasurement overwrote the previous profile");
  const auto pointer=store/"profiles"/backend/"automatic-selected.txt";
  if(!openpuzzle::writePrivateFileAtomically(pointer,"unknown-existing-content"))throw std::runtime_error("Cannot prepare damaged automatic selection");
  bool blocked=false;try{openpuzzle::ui::storeWindowsUiAutomaticProfile(store,second);}catch(...){blocked=true;}
  if(!blocked||openpuzzle::windowsFile::readPrivate(pointer,absent,256)!=std::optional<std::string>("unknown-existing-content"))throw std::runtime_error("Damaged automatic selection was replaced");
 }
 const auto result=openpuzzle::ui::runWindowsUi(std::filesystem::absolute(argv[1]),true,registry.key,fixtureProfile);
 std::ifstream profileInput(fixtureProfile);const std::string profileAfter{std::istreambuf_iterator<char>(profileInput),{}};if(profileAfter!=measured)return 1;
 std::ifstream input(legacy);const std::string after{std::istreambuf_iterator<char>(input),{}};
 if(result||after!=old||preferences.load()!=UiLanguage::Portuguese||std::filesystem::exists(openpuzzle::localPaths::configDirectory()/"client.id"))return 1;
 std::cout<<"WindowsUiWindowTests passed: first profile without prerequisites, persistence/reopen, existing/damaged profile preservation, automatic CUDA/OpenCL finalist/binding/reopen, failed-run preservation, four languages, scrolling and owned synthetic benchmark\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
