#include "openpuzzle/ui/WindowsUiWindow.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include <fstream>
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/ui/WindowsUiLanguageStore.hpp"
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
 const auto result=openpuzzle::ui::runWindowsUi(std::filesystem::absolute(argv[1]),true,registry.key,fixtureProfile);
 std::ifstream profileInput(fixtureProfile);const std::string profileAfter{std::istreambuf_iterator<char>(profileInput),{}};if(profileAfter!=measured)return 1;
 std::ifstream input(legacy);const std::string after{std::istreambuf_iterator<char>(input),{}};
 if(result||after!=old||preferences.load()!=UiLanguage::Portuguese||std::filesystem::exists(openpuzzle::localPaths::configDirectory()/"client.id"))return 1;
 std::cout<<"WindowsUiWindowTests passed: four languages, Safe Stop captions, repeated scrolling, owned synthetic benchmark/import and preserved original profile\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
