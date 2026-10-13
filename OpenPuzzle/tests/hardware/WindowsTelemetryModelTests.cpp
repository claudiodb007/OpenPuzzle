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
 std::cout<<"WindowsTelemetryModelTests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
