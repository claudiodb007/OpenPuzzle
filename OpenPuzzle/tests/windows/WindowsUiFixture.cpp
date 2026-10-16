#include <windows.h>
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include <iostream>
#include <string>
#include <vector>
int wmain(int argc,wchar_t** argv){
 const std::wstring command=argc>1?argv[1]:L"";
 if(command==L"echo"){for(int i=2;i<argc;++i)std::cout<<openpuzzle::pathUtf8(std::filesystem::path(argv[i]))<<'\n';return 0;}
 if(command==L"slow"||command==L"fixture-run"){std::cout<<"runtime-started\n"<<std::flush;Sleep(command==L"slow"?5000:800);std::cout<<"runtime-finished\n";return 0;}
 if(command==L"flood"){std::cout<<std::string(300000,'x')<<"\nTAIL\n";return 0;}
 if(command==L"fail"){std::cout<<"fixture-error\n";return 9;}
 if(command==L"run"){
  bool dry=false;for(int i=2;i<argc;++i)if(std::wstring(argv[i])==L"--dry-run")dry=true;
  if(!dry)return 8;std::cout<<"benchmark-profile-preflight-fixture\n";return 0;
 }
 if(command==L"benchmark"){
  const std::vector<std::wstring> expected{L"benchmark",L"--backend",L"cpu",L"--device",L"0",L"--threads",L"1",L"--seconds",L"30",L"--real"};
  if(argc!=static_cast<int>(expected.size()+1))return 8;for(int i=1;i<argc;++i)if(argv[i]!=expected[i-1])return 8;
  const auto report=openpuzzle::localPaths::dataDirectory()/"engine-diagnostics"/"ui-benchmark-fixture";openpuzzle::WorkspaceSecurity::prepare(report);
  const auto json="{\"version\":1,\"backend\":\"cpu\",\"engine_sha256\":\""+std::string(64,'a')+"\",\"device\":0,\"device_identity\":\"CPU fixture\",\"selftest_passed\":true,\"threads\":1,\"blocks\":256,\"points\":256,\"speed_mkeys\":2.0,\"samples\":3,\"duration_seconds\":30}\n";
  Sleep(300);if(!openpuzzle::writePrivateFileAtomically(report/"result.json",json))return 8;
  std::cout<<"Report: "<<openpuzzle::pathUtf8(report)<<"\nbenchmark-fixture-finished\n";return 0;
 }
 if(command==L"status"){std::cout<<"Slot: primary\nRuntime: idle\nAssignment: none\nPending claim: none\n";return 0;}
 if(command==L"telemetry"){std::cout<<"GPU telemetry: AVAILABLE\nDevice: synthetic\nTemperature C: 31\nPower W: 10\nPower limit W: 300\n";return 0;}
 if(command==L"thermal"){std::cout<<"Thermal mode: enabled\nWarning C: 75\nCritical C: 85\nStop on critical: yes\n";return 0;}
 if(command==L"stop"||command==L"safestop"){std::cout<<"Cooperative request recorded; completion is not yet confirmed\n";return 0;}
 return 8;
}
