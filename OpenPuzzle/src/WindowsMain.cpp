#include "openpuzzle/core/WindowsCliApplication.hpp"
#include <windows.h>
#include <iostream>
#include <stdexcept>
namespace {
std::string utf8(const wchar_t* value){
 const int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,nullptr,0,nullptr,nullptr);
 if(count<=0)throw std::invalid_argument("Invalid Unicode command argument");
 std::string result(count,'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,result.data(),count,nullptr,nullptr))throw std::invalid_argument("Unicode conversion failed");
 result.pop_back();return result;
}
}
int wmain(int argc,wchar_t** argv){
 SetConsoleOutputCP(CP_UTF8);
 try{std::vector<std::string> args;for(int i=1;i<argc;++i)args.push_back(utf8(argv[i]));return openpuzzle::WindowsCliApplication().run(args,std::cout);}
 catch(const std::exception& error){std::cerr<<"Windows CLI refused: "<<error.what()<<'\n';return 1;}
}
