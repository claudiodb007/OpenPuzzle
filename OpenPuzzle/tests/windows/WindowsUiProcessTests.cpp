#include "openpuzzle/ui/WindowsUiProcess.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include <iostream>
#include <algorithm>
using namespace openpuzzle::ui;
void check(bool value){if(!value)throw std::runtime_error("UI process contract failed");}
UiProcessUpdate finish(WindowsUiProcess& p,std::string& output){
 const auto start=GetTickCount64();for(;;){auto r=p.poll();output+=r.output;if(r.finished)return r;if(GetTickCount64()-start>10000)throw std::runtime_error("Fixture timed out");Sleep(10);}
}
int wmain(int argc,wchar_t** argv){try{
 check(argc==2);const auto fixture=std::filesystem::absolute(argv[1]);WindowsUiProcess p;std::string output;
 wchar_t previous[128]{};const auto previousSize=GetEnvironmentVariableW(L"OPENPUZZLE_EXECUTION_SLOT",previous,128);check(previousSize<128);
 p.start(fixture,{"echo","spaces & %PATH% | literal","quote\" and trailing\\","Olá"});auto result=finish(p,output);
 output.erase(std::remove(output.begin(),output.end(),'\r'),output.end());
 check(result.exitCode==0&&output=="spaces & %PATH% | literal\nquote\" and trailing\\\nOlá\n");
 output.clear();p.start(fixture,{"fail"});result=finish(p,output);check(result.exitCode==9&&output.find("fixture-error")!=std::string::npos);
 output.clear();p.start(fixture,{"flood"});result=finish(p,output);check(result.exitCode==0&&output.find("TAIL")!=std::string::npos&&output.size()>300000);
 output.clear();p.start(fixture,{"slow"},false,100);result=finish(p,output);check(result.timedOut&&result.exitCode==124&&!p.busy());
 output.clear();p.start(fixture,{"fixture-run"},true,1);WindowsUiProcess control;control.start(fixture,{"stop","--slot","primary"});std::string reply;
 result=finish(control,reply);check(result.exitCode==0&&p.runtime());result=finish(p,output);check(result.exitCode==0&&!result.timedOut&&output.find("runtime-finished")!=std::string::npos);
 wchar_t current[128]{};const auto currentSize=GetEnvironmentVariableW(L"OPENPUZZLE_EXECUTION_SLOT",current,128);
 check(currentSize==previousSize&&std::wstring(current,currentSize)==std::wstring(previous,previousSize));
 std::cout<<"WindowsUiProcessTests passed: literal argv, streamed output, independent control, bounded commands and unbounded runtime\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
