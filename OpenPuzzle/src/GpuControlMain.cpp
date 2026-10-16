#include "openpuzzle/hardware/GpuControl.hpp"
#include <iostream>
#ifdef _WIN32
#include "openpuzzle/core/LocalPaths.hpp"
int wmain(int argc,wchar_t** argv){
 std::vector<std::string> args;for(int i=1;i<argc;++i)args.push_back(openpuzzle::pathUtf8(std::filesystem::path(argv[i])));
#else
int main(int argc,char** argv){std::vector<std::string> args(argv+1,argv+argc);
#endif
 try{return openpuzzle::gpuControlCommand(args,std::cout);}
 catch(const openpuzzle::GpuControlFailure& e){std::cerr<<e.what()<<'\n';return e.exitCode;}
 catch(const std::invalid_argument& e){std::cerr<<e.what()<<'\n';return 64;}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
