#include "VanityPreviewEngine.hpp"
#include "openpuzzle/engines/bitcrack/BitCrackEngine.hpp"
#include <iostream>
#include <functional>
using namespace openpuzzle;using namespace openpuzzle::vanityPreview;
static void require(bool b){if(!b)throw std::runtime_error("Model assertion failed");}
static void refused(const std::function<void()>& f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}require(caught);}
int main(){try{
 require(inclusiveCount("123456789fffffffff","12345678a000000000")==2);
 refused([]{inclusiveCount("20","10");});refused([]{inclusiveCount("1","100000000000000000000");});
 EngineRegistry r;EngineFactory f;EngineDescriptor bit;bit.id="bitcrack";bit.name="BitCrack";r.registerEngine(bit);
 f.registerFactory("bitcrack",[](const auto& p){return std::make_unique<BitCrackEngine>(p);});AdapterConfig config;
 registerVanityPreview(r,f,config,false);require(r.count()==1);refused([&]{f.create("vanity-preview","");});
 registerVanityPreview(r,f,config,true);require(r.count()==2&&!r.find("vanity-preview")->capabilities.distributedReady);
 require(f.create("bitcrack","preserved")->info().executable=="preserved");
 EngineLaunchRequest request;refused([&]{f.create("vanity-preview","")->buildCommand(request);});
 VanityPreviewProgressParser parser(200);require(!parser.parseLine("diagnostic"));
 auto first=parser.parseLine("OPV_CLIENT PROGRESS 100 elapsed_us=50");require(first->keysChecked=="100"&&first->speedMKeys==2.0);
 refused([&]{parser.parseLine("OPV_CLIENT PROGRESS 100 elapsed_us=51");});refused([&]{parser.parseLine("OPV_CLIENT PROGRESS 201 elapsed_us=52");});
 parser.parseLine("OPV_CLIENT PROGRESS 200 elapsed_us=100");auto p=parser.parseLine("OPV_CLIENT RESULT complete checked=200 resumed=0 safe_stop=0 next_blocked=0");require(p->finished&&!p->keyFound&&p->privateKey.empty());
 refused([&]{parser.parseLine("OPV_CLIENT PROGRESS 200");});
 VanityPreviewProgressParser partial(200);partial.parseLine("OPV_CLIENT PROGRESS 100");refused([&]{partial.parseLine("OPV_CLIENT RESULT complete checked=100 resumed=0 safe_stop=0 next_blocked=0");});
 VanityPreviewProgressParser resumed(200,100);require(resumed.parseLine("OPV_CLIENT PROGRESS 200 elapsed_us=50")->speedMKeys==2.0);require(resumed.parseLine("OPV_CLIENT RESULT complete checked=200 resumed=100 safe_stop=0 next_blocked=0")->finished);
 VanityPreviewProgressParser legacy(200);require(legacy.parseLine("OPV_CLIENT PROGRESS 100")->speedMKeys==0.0);
 std::cout<<"Vanity integration model passed: explicit opt-in, existing registry preserved, range carry, strict progress and resume\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
