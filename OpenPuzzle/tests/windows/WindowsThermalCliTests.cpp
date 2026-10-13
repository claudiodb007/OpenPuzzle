#include "openpuzzle/core/WindowsCliApplication.hpp"
#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
using namespace openpuzzle;namespace fs=std::filesystem;
void require(bool b,const char* message="Thermal CLI contract failed"){if(!b)throw std::runtime_error(message);}
std::string read(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(){try{
 const auto root=fs::temp_directory_path()/pathFromUtf8("op-thermal-cli-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));fs::create_directory(root);require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()));
 WindowsCliApplication app({},[]{WindowsGpuInventory i;i.nativeCpuAvailable=true;return i;},{},[]{client::ClientGpuTelemetry v;v.vendor="NVIDIA";v.device=0;v.deviceId="cuda-0";v.temperatureC=64;v.powerDrawW=123;v.powerLimitW=290;return WindowsTelemetryReport{true,"fixture",{{"fixture GPU",v}}};});
 std::ostringstream out;require(app.run({"thermal"},out)==0,"Read-only thermal command failed");
 require(!fs::exists(localPaths::root()),"Read-only thermal command wrote a profile");
 require(app.run({"telemetry"},out)==0&&out.str().find("Temperature C: 64")!=std::string::npos,"Read-only telemetry command failed");
 require(!fs::exists(localPaths::root()),"Read-only telemetry command wrote a profile");
 for(const auto& bad:std::vector<std::vector<std::string>>{{"thermal","--enable","--disable"},{"thermal","--warning","NaN"},{"thermal","--warning","85","--critical","85"},{"thermal","--warning","75junk"},{"thermal","--stop-on-critical","--no-stop-on-critical"},{"thermal","--slot","cuda"},{"telemetry","extra"}})require(app.run(bad,out)==1,"Invalid thermal options accepted");
 require(!fs::exists(localPaths::root()),"Invalid controls wrote profile data");
 require(app.run({"thermal","--enable","--stop-on-critical"},out)==0,"Thermal save failed");auto config=ConfigurationManager::loadChecked();require(config&&config->gpu.thermal.enabled&&config->gpu.thermal.stopOnCritical,"Thermal policy not persisted");
 config->assignment.durationMinutes=47;config->gpu.device=3;require(ConfigurationManager::save(*config),"Configuration fixture save failed");
 int a=1,b=1;std::thread first([&]{std::ostringstream o;a=app.run({"thermal","--warning","80"},o);});std::thread second([&]{std::ostringstream o;b=app.run({"thermal","--critical","90"},o);});first.join();second.join();
 config=ConfigurationManager::loadChecked();require(a==0&&b==0&&config&&config->gpu.thermal.warningC==80&&config->gpu.thermal.criticalC==90&&config->assignment.durationMinutes==47&&config->gpu.device==3,"Concurrent thermal updates lost settings");
 const auto path=pathFromUtf8(ConfigurationManager::configPath());require(writePrivateFileAtomically(path,"invalid; preserved"),"Invalid config fixture failed");const auto old=read(path);require(app.run({"thermal","--disable"},out)==1&&read(path)==old,"Invalid configuration overwritten");
 require(!fs::exists(localPaths::configDirectory()/"client.id"),"Thermal control created a client identity");
 std::cout<<"WindowsThermalCliTests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
