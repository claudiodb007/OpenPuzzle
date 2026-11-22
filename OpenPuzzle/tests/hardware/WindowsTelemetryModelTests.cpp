#include "openpuzzle/hardware/WindowsTelemetryModel.hpp"
#include <iostream>
using namespace openpuzzle;
void require(bool value){if(!value)throw std::runtime_error("Windows telemetry/thermal contract failed");}
WindowsTelemetryReport report(double temperature){client::ClientGpuTelemetry v;v.vendor="NVIDIA";v.deviceId="cuda-0";v.device=0;v.pciBusId="00000000:08:00.0";v.temperatureC=temperature;v.powerDrawW=123.456;v.powerLimitW=290;return {true,"fixture",{{"selected GPU",v}}};}
int main(){try{
 require(canonicalGpuPciBusId("0000:08:00.0")=="00000000:08:00.0");require(canonicalGpuPciBusId("ffffffff:ff:1f.7")=="FFFFFFFF:FF:1F.7");
 for(const auto* bad:{"0:08:20.0","0:08:01.8","-1:00:00.0","0:00:00.0junk","0:00:00.0.1"})require(canonicalGpuPciBusId(bad).empty());
 auto mapped=report(55);mapped.readings[0].values.deviceId="nvidia-0";
 mapped.bindCudaOrdinals({{"0000:09:00.0",0},{"0000:08:00.0",3}});
 require(mapped.readings[0].values.device==3&&mapped.readings[0].values.deviceId=="cuda-3");
 mapped=report(55);mapped.readings[0].values.deviceId="nvidia-0";
 mapped.bindCudaOrdinals({{"0000:08:00.0",0},{"00000000:08:00.0",1}});
 require(mapped.readings[0].values.deviceId=="nvidia-0");
 auto now=std::chrono::steady_clock::now();int reads=0;auto current=report(70);
 WindowsRuntimeThermalSettings settings;settings.policy={true,true,75,85};settings.deviceName="selected GPU";settings.reader=[&]{++reads;return current;};
 WindowsThermalMonitor monitor(settings);monitor.sample(now);require(reads==1&&!monitor.mustStop()&&monitor.status()==WindowsThermalStatus::Normal);
 current=report(88);monitor.sample(now+std::chrono::seconds(1));require(reads==1&&!monitor.mustStop());monitor.sample(now+std::chrono::seconds(5));require(monitor.mustStop());
 current=report(75);monitor.sample(now+std::chrono::seconds(10));require(monitor.status()==WindowsThermalStatus::Warning&&!monitor.mustStop());
 current=report(74);monitor.sample(now+std::chrono::seconds(15));require(monitor.status()==WindowsThermalStatus::Warning);
 current=report(72);monitor.sample(now+std::chrono::seconds(20));require(monitor.status()==WindowsThermalStatus::Normal);
 current.readings.push_back(current.readings.front());monitor.sample(now+std::chrono::seconds(25));require(monitor.mustStop()&&monitor.status()==WindowsThermalStatus::Unavailable);
 current=report(70);current.readings[0].values.temperatureC.reset();monitor.sample(now+std::chrono::seconds(30));require(monitor.mustStop());
 current=report(99);settings.policy.stopOnCritical=false;WindowsThermalMonitor warnings(settings);warnings.sample(now);require(!warnings.mustStop()&&warnings.status()==WindowsThermalStatus::Critical);
 settings.deviceName.clear();settings.policy.stopOnCritical=true;WindowsThermalMonitor cpu(settings);cpu.sample(now);require(!cpu.mustStop()&&cpu.status()==WindowsThermalStatus::Disabled);
 settings.deviceName="selected GPU";settings.reader=[]()->WindowsTelemetryReport{throw std::runtime_error("driver unavailable");};WindowsThermalMonitor lost(settings);lost.sample(now);require(lost.mustStop());
 client::ClientHeartbeat heartbeat;current=report(55);current.heartbeat(heartbeat);require(heartbeat.gpuTelemetryReported&&heartbeat.gpuTelemetry.size()==1&&heartbeat.gpuTelemetry[0].powerDrawW==123.456);
 current.complete=false;current.heartbeat(heartbeat);require(!heartbeat.gpuTelemetryReported&&heartbeat.gpuTelemetry.empty());
 current={true,"none",{}};current.heartbeat(heartbeat);require(heartbeat.gpuTelemetryReported&&heartbeat.gpuTelemetry.empty());

 // The OpenCL chip name can differ from ADLX's marketing name. Only the
 // persistent physical identity may connect that sensor to engine index 7.
 const std::string pnp="PCI\\VEN_1002&DEV_731F\\RIG-A";
 std::vector<WindowsAmdTelemetryDevice> amdDevices={{2,"AMD Radeon RX 5700 XT",pnp}};
 int enumerations=0,temperatureReads=0;
 WindowsAmdTelemetryApi amdApi;
 amdApi.devices=[&]{++enumerations;return amdDevices;};
 std::optional<double> temperature=64.25;
 amdApi.temperature=[&](const std::string& id){require(id==pnp);++temperatureReads;return temperature;};
 auto amd=queryWindowsAmdTelemetry(amdApi);
 require(amd.complete&&enumerations==2&&temperatureReads==1&&amd.readings.size()==1);
 require(amd.readings[0].values.temperatureC==64.25&&!amd.readings[0].values.powerDrawW&&!amd.readings[0].values.powerLimitW);
 require(amd.bindDevice("opencl",7,"","",pnp));
 amd.heartbeat(heartbeat);require(heartbeat.gpuTelemetryReported&&heartbeat.gpuTelemetry.size()==1&&
  heartbeat.gpuTelemetry[0].deviceId=="opencl-7"&&heartbeat.gpuTelemetry[0].device==7);
 require(!amd.bindDevice("opencl",0,"","","PCI\\VEN_1002&DEV_731F\\OTHER"));
 WindowsRuntimeThermalSettings amdSettings;amdSettings.policy={true,true,75,85};
 amdSettings.deviceName="gfx1010:xnack-";amdSettings.deviceWindowsInstanceId=pnp;amdSettings.reader=[&]{return amd;};
 WindowsThermalMonitor amdMonitor(amdSettings);amdMonitor.sample(now);require(amdMonitor.status()==WindowsThermalStatus::Normal&&!amdMonitor.mustStop());
 amd.readings[0].values.temperatureC=90;amdMonitor.sample(now+std::chrono::seconds(5));require(amdMonitor.mustStop());
 amd.readings.push_back(amd.readings[0]);require(!amd.bindDevice("opencl",7,"","",pnp));
 WindowsThermalMonitor duplicateAmd(amdSettings);duplicateAmd.sample(now);require(duplicateAmd.mustStop());
 for(const auto value:{std::optional<double>{},std::optional<double>{-1},std::optional<double>{201},std::optional<double>{std::nan("")}}){
  temperature=value;const auto unavailable=queryWindowsAmdTelemetry(amdApi);require(unavailable.complete&&unavailable.readings.empty());
 }
 // Measured power is independent of the optional temperature, and never
 // becomes a fabricated watt limit from the AMD tuning percentage.
 amdApi.power=[](const auto&){return std::optional<double>{142.75};};
 temperature=64.25;auto powered=queryWindowsAmdTelemetry(amdApi);
 require(powered.readings.size()==1&&powered.readings[0].values.powerDrawW==142.75&&!powered.readings[0].values.powerLimitW);
 temperature.reset();powered=queryWindowsAmdTelemetry(amdApi);
 require(powered.readings.size()==1&&!powered.readings[0].values.temperatureC&&powered.readings[0].values.powerDrawW==142.75);
 for(const auto value:{-1.0,10001.0,std::nan("")}){amdApi.power=[value](const auto&){return std::optional<double>{value};};require(queryWindowsAmdTelemetry(amdApi).readings.empty());}
 amdApi.power={};
 temperature=64.25;amdDevices.push_back(amdDevices.front());require(!queryWindowsAmdTelemetry(amdApi).complete);amdDevices.pop_back();
 amdApi.devices=[&]{auto result=amdDevices;if(++enumerations%2==0)result[0].windowsDeviceInstanceId="PCI\\VEN_1002&DEV_731F\\REPLACED";return result;};
 require(!queryWindowsAmdTelemetry(amdApi).complete);
 amdApi.devices=[&]{return amdDevices;};amdApi.temperature=[](const auto&)->std::optional<double>{throw std::runtime_error("driver failed");};
 require(!queryWindowsAmdTelemetry(amdApi).complete);
 amdApi.temperature=[&](const auto&){return temperature;};amd=queryWindowsAmdTelemetry(amdApi);
 const WindowsTelemetryReport absentNv{false,"NVML absent",{}};
 require(mergeWindowsTelemetryReports(absentNv,amd,false,true).complete);
 require(!mergeWindowsTelemetryReports(absentNv,amd,true,true).complete);
 require(mergeWindowsTelemetryReports(report(55),amd,true,true).readings.size()==2);
 require(mergeWindowsTelemetryReports(report(55),{},true,false).complete);
 std::cout<<"WindowsTelemetryModelTests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
