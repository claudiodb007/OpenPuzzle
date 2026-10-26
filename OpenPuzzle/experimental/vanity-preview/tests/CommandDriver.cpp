// Small harness for dependency-light development. CTest uses the real OpenPuzzle binary.
#include "VanityCommand.hpp"
#include <string>
#include <vector>
int main(int argc,char** argv){
 if(argc<2||std::string(argv[1])!="vanity-preview")return 2;
 std::vector<std::string> args;for(int i=2;i<argc;++i)args.emplace_back(argv[i]);
 return openpuzzle::runVanityCommand(args);
}
