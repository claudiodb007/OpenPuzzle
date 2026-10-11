#include "openpuzzle/runtime/WindowsEngineRange.hpp"
#include <iostream>
#include <stdexcept>
using openpuzzle::WindowsEngineRange;
void check(bool value){if(!value)throw std::runtime_error("Range boundary assertion failed");}
int main(){try{
 check(WindowsEngineRange::keyhuntExclusiveEnd("1","1")=="2");
 check(WindowsEngineRange::keyhuntExclusiveEnd("f","000f")=="10");
 check(WindowsEngineRange::keyhuntExclusiveEnd("1","FFF")=="1000");
 check(WindowsEngineRange::keyhuntExclusiveEnd("68720BC60B6B059560","68720BC76BBF5B601F")=="68720BC76BBF5B6020");
 check(WindowsEngineRange::keyhuntExclusiveEnd("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364140","FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364140")=="FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141");
 for(const auto& pair: {std::pair<std::string,std::string>{"0","1"},{"2","1"},{"1","xyz"},{"1","FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141"},{"1",std::string(65,'F')},{"","1"}}){
  bool rejected=false;try{WindowsEngineRange::keyhuntExclusiveEnd(pair.first,pair.second);}catch(const std::exception&){rejected=true;}check(rejected);
 }
 std::cout<<"WindowsEngineRangeTests passed: inclusive one-key, carries, real recycled interval and scalar-order boundary\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
