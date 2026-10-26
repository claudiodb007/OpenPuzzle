#include "openpuzzle/runtime/VanityRangeProtocol.hpp"
#include <functional>
#include <iostream>
using namespace openpuzzle::vanityPreview;
static void require(bool v){if(!v)throw std::runtime_error("assertion failed");}
static void refused(std::function<void()> fn){bool yes=false;try{fn();}catch(...){yes=true;}require(yes);}
int main(){
  require(add("fffffffffffffffff",1)==canonical("100000000000000000"));
  require(add("1234567890ffffffff",1)==canonical("123456789100000000"));
  refused([]{decimal("18446744073709551616");});refused([]{canonical("0x12");});
  refused([]{Plan("1",1,1);});refused([]{Plan("10000000000000000",0,1);});
  refused([]{Plan("ffffffffffffffffffff",2,1);});
  for(uint64_t count:{1ull,2ull,1023ull,1024ull,1025ull,262143ull,262144ull,262145ull,524291ull}){
    Plan p("1234567890fffffff0",count,1);Protocol protocol(p);
    protocol.consume("OPV1 BEGIN "+p.start+" "+std::to_string(count)+" "+std::to_string(p.batch));
    uint64_t checked=0;
    while(checked<count){checked+=std::min(p.batch,count-checked);protocol.consume("OPV1 PROGRESS "+std::to_string(checked));}
    require(!protocol.completed(0));protocol.consume("OPV1 COMPLETE "+std::to_string(count));
    require(protocol.completed(0));require(!protocol.completed(1));
    refused([&]{protocol.consume("OPV1 PROGRESS "+std::to_string(count));});
  }
  for(const std::string& line:{"OPV1 PROGRESS 1","OPV1 COMPLETE 262145","OPV1 PROGRESS 999999999","OPV1 PROGRESS -1","OPV1 PROGRESS 262144 junk"}){
    Plan p("123456789000000000",262145,1);Protocol q(p);
    q.consume("OPV1 BEGIN "+p.start+" 262145 262144");refused([&]{q.consume(line);});
  }
  Plan p("123456789000000000",262145,1);Protocol q(p);
  q.consume("OPV1 BEGIN "+p.start+" 262145 262144");q.consume("OPV1 PROGRESS 262144");
  refused([&]{q.consume("OPV1 PROGRESS 262144");});q.consume("OPV1 STOPPED 262144");require(!q.completed(0));
  std::cout<<"VanityRangeProtocolTests passed: carry, limits, exact batches, tail, forged/duplicate progress and terminal/exit binding\n";
}
