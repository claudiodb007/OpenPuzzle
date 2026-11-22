#include "openpuzzle/ui/WindowsDesktopApplication.hpp"
#include <windows.h>
#include <iostream>
int wmain(int argc,wchar_t** argv){try{
 std::vector<std::string> args;for(int i=1;i<argc;++i){const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);
  if(n<1)throw std::runtime_error("Invalid desktop argument");std::string value(n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,value.data(),n,nullptr,nullptr);value.pop_back();args.push_back(std::move(value));}
 return openpuzzle::ui::WindowsDesktopApplication().run(args,std::cout);
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
