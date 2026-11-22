#include "openpuzzle/ui/WindowsDesktopProtocol.hpp"
#include <iostream>
using namespace openpuzzle::ui;
void check(bool ok){if(!ok)throw std::runtime_error("Desktop status contract failed");}
int main(){try{
 const std::string idle="Slot: primary\nRuntime: idle\nAssignment: none\nPending claim: none\n";
 check(windowsDesktopSlots(idle,0).front().status=="idle");
 auto pending=idle;pending.replace(pending.find("Assignment: none"),16,"Assignment: preserved-range");
 check(windowsDesktopSlots(pending,0).front().status=="stopped");
 auto unknown=idle;unknown.replace(unknown.find("Runtime: idle"),13,"Runtime: unknown");
 check(windowsDesktopSlots(unknown,2).front().status=="identity unavailable");
 const auto output=windowsDesktopStatus(windowsDesktopSlots(pending,0),true);
 check(output.starts_with("Slot.")&&output.find("pending supervisor")!=std::string::npos&&output.ends_with("Desktop profile.... ready\n"));
 auto sensed=windowsDesktopSlots(pending,0);sensed[0].fields["Temperature"]="64.0 °C (GPU)";sensed[0].fields["Power"]="142.8 W / —";
 const auto metrics=windowsDesktopStatus(sensed,true);
 check(metrics.find("Temperature.......... 64.0 °C (GPU)")!=std::string::npos&&metrics.find("Power.......... 142.8 W / —")!=std::string::npos);
 for(const auto& bad:{std::string(),idle+idle,std::string("Slot: cuda-0\nRuntime: idle\n"),std::string("Slot: ../x\nRuntime: idle\nAssignment: none\nPending claim: none\n"),std::string("Runtime: idle\n")}){
  bool failed=false;try{(void)windowsDesktopSlots(bad,0);}catch(...){failed=true;}check(failed);
 }
 bool failed=false;try{(void)windowsDesktopSlots(idle,1);}catch(...){failed=true;}check(failed);
 std::cout<<"Desktop protocol passed: idle, preserved, uncertain and malformed states\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
