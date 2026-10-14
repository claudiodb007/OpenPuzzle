#include <windows.h>
#include "openpuzzle/core/LocalPaths.hpp"
#include <iostream>
#include <string>
int wmain(int argc,wchar_t** argv){
 const std::wstring command=argc>1?argv[1]:L"";
 if(command==L"echo"){for(int i=2;i<argc;++i)std::cout<<openpuzzle::pathUtf8(std::filesystem::path(argv[i]))<<'\n';return 0;}
 if(command==L"slow"||command==L"fixture-run"){std::cout<<"runtime-started\n"<<std::flush;Sleep(command==L"slow"?5000:800);std::cout<<"runtime-finished\n";return 0;}
 if(command==L"flood"){std::cout<<std::string(300000,'x')<<"\nTAIL\n";return 0;}
 if(command==L"fail"){std::cout<<"fixture-error\n";return 9;}
 if(command==L"status"){std::cout<<"Slot: primary\nRuntime: idle\nAssignment: none\nPending claim: none\n";return 0;}
 if(command==L"telemetry"){std::cout<<"GPU telemetry: AVAILABLE\nDevice: synthetic\nTemperature C: 31\nPower W: 10\nPower limit W: 300\n";return 0;}
 if(command==L"thermal"){std::cout<<"Thermal mode: enabled\nWarning C: 75\nCritical C: 85\nStop on critical: yes\n";return 0;}
 if(command==L"stop"||command==L"safestop"){std::cout<<"Cooperative request recorded; completion is not yet confirmed\n";return 0;}
 return 8;
}
