#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/WindowsCommandLine.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/runtime/WindowsProcessIdentity.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
using namespace openpuzzle;
std::string hex(const std::string& text){const char* digits="0123456789abcdef";std::string out;for(unsigned char c:text){out+=digits[c>>4];out+=digits[c&15];}return out;}
int wmain(int argc,wchar_t** argv){try{
 if(argc<2)return 2;
 const std::wstring mode=argv[1];
 if(mode==L"echo"){
   for(int i=2;i<argc;++i)std::cout<<i-2<<':'<<hex(pathUtf8(fs::path(argv[i])))<<std::endl;
   std::cerr<<"stderr"<<std::endl;
   char byte=0;DWORD n=1;if(!ReadFile(GetStdHandle(STD_INPUT_HANDLE),&byte,1,&n,nullptr)||n!=0)return 3;
   return 0;
 }
 if(mode==L"exit259")return 259;
 if(mode==L"cwd"){std::cout<<hex(pathUtf8(fs::current_path()))<<std::endl;return 0;}
 if(mode==L"probe"&&argc==3){auto raw=std::stoull(argv[2]);return SetEvent(reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(raw)))?4:0;}
 if(mode==L"sleep"){Sleep(15000);return 0;}
 if(mode==L"touch"&&argc==3){return writePrivateFileAtomically(fs::path(argv[2]),"ran")?0:5;}
 if(mode==L"gate"&&argc==4){
   if(!writePrivateFileAtomically(fs::path(argv[3]),"ready"))return 6;
   const auto end=GetTickCount64()+15000;
   while(GetTickCount64()<end){if(fs::exists(argv[2]))return 0;Sleep(10);}return 7;
 }
 if(mode==L"tree"||mode==L"leave-tree"){
   auto exe=fs::absolute(argv[0]);auto command=pathFromUtf8(WindowsCommandLine::build(pathUtf8(exe),{"sleep"})).native();
   STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
   if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child))return 8;
   auto identity=WindowsProcessIdentity::creationTime(child.dwProcessId);
   std::cout<<"child="<<child.dwProcessId<<':'<<identity.value_or(0)<<std::endl;
   CloseHandle(child.hThread);CloseHandle(child.hProcess);
   if(mode==L"tree")Sleep(15000);
   return 0;
 }
 return 9;
 }catch(...){return 10;}}
