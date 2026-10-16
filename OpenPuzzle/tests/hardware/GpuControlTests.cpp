#include "openpuzzle/ui/GpuControlUiModel.hpp"
#include <iostream>
using namespace openpuzzle;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F>void refuses(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Invalid control accepted");}
struct Fake:GpuControlBackend {
 std::vector<GpuControlDevice> devices;int writes=0;bool deny=false,badReadback=false;
 std::vector<GpuControlDevice> inspect()override{return devices;}
 void apply(const GpuControlDevice&,const GpuControlRequest& r)override{
  if(deny)throw std::runtime_error("permission denied");++writes;if(badReadback)return;
  auto& d=devices.front();if(r.action==GpuControlAction::Power)d.powerNow=r.value;
  if(r.action==GpuControlAction::PowerDefault)d.powerNow=d.powerDefault;
  if(r.action==GpuControlAction::FanAuto)d.fanMode="auto";
  if(r.action==GpuControlAction::FanManual){d.fanMode="manual";d.fanNow=r.value;}
 }
};
int main(){try{
 GpuControlDevice d;d.vendor="nvidia";d.pci="00000000:08:00.0";d.uuid="GPU-example";d.name="quoted \"GPU\" ç";d.power=d.fan=d.automatic=true;d.powerNow=333;d.powerMin=100;d.powerMax=351.5;d.powerDefault=333;d.powerStep=.001;d.fanNow=40;d.temperature=67;d.fanMode="auto";
 Fake f;f.devices={d};GpuControlRequest r{d.vendor,d.pci,d.uuid,GpuControlAction::Power,300};
 auto formatted=formatGpuControls(f.inspect());require(parseGpuControls(formatted).at(0).name==d.name,"Protocol Unicode/quoting failed");
 std::ostringstream out;gpuControlCommand({"list"},out,[&]{return f.inspect();},[&](auto request){applyGpuControlChecked(f,request);});require(f.writes==0,"Read-only command wrote settings");
 applyGpuControlChecked(f,r);require(f.writes==1&&f.devices[0].powerNow==300,"Power control failed");
 r.value=1000;refuses([&]{applyGpuControlChecked(f,r);});require(f.writes==1,"Invalid power wrote settings");
 r.value=300;r.uuid="GPU-other";refuses([&]{applyGpuControlChecked(f,r);});r.uuid=d.uuid;
 f.devices.push_back(d);refuses([&]{applyGpuControlChecked(f,r);});f.devices.pop_back();
 r.action=GpuControlAction::FanManual;r.value=29;refuses([&]{applyGpuControlChecked(f,r);});r.value=70;f.devices[0].temperature=80;refuses([&]{applyGpuControlChecked(f,r);});
 f.devices[0].temperature=-1;refuses([&]{applyGpuControlChecked(f,r);});f.devices[0].temperature=67;
 applyGpuControlChecked(f,r);require(f.devices[0].fanMode=="manual"&&f.devices[0].fanNow==70,"Manual fan failed");
 r.action=GpuControlAction::FanAuto;applyGpuControlChecked(f,r);require(f.devices[0].fanMode=="auto","Automatic fan failed");
 f.deny=true;refuses([&]{applyGpuControlChecked(f,r);});f.deny=false;
 f.badReadback=true;r.action=GpuControlAction::Power;r.value=200;refuses([&]{applyGpuControlChecked(f,r);});f.badReadback=false;
 auto args=openpuzzle::ui::gpuControlArguments(d,GpuControlAction::Power,300);require(args[0]=="gpu-control"&&args[5]==d.pci&&args[7]==d.uuid,"UI physical routing failed");
 const auto writer=[&](auto request){applyGpuControlChecked(f,request);};const auto reader=[&]{return f.inspect();};
 for(const auto bad:std::vector<std::vector<std::string>>{{"apply","--vendor","nvidia","--pci",d.pci,"--power","nan"},{"apply","--vendor","nvidia","--pci",d.pci,"--fan","auto","--power","300"},{"apply","--vendor","nvidia","--pci",d.pci,"--power","300","--power","250"},{"apply","--vendor","nvidia","--pci","0","--fan","auto"}})refuses([&]{gpuControlCommand(bad,out,reader,writer);});
 gpuControlCommand({"apply","--vendor",d.vendor,"--pci",d.pci,"--uuid",d.uuid,"--power-default"},out,reader,writer);require(f.devices[0].powerNow==333,"Default power failed");
 refuses([&]{parseGpuControls(formatted+formatted);});refuses([&]{parseGpuControls(formatted+formatted.substr(formatted.find('\n')+1));});
 std::cout<<"GpuControlTests passed: physical identity, zero-write reads, bounded values, permission failure, readback, auto/manual and literal UI commands\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
