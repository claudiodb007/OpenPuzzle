#include "ReceiptJson.hpp"
#include <cassert>
#include <iostream>
using namespace openpuzzle::vanityPreview::receipts;
int main(){
 auto value=Json::parse(" {\"z\":false,\"a\":{\"count\":\"18446744073709551615\",\"n\":18446744073709551615,\"v\":null}} ");
 assert(value.dump()=="{\"a\":{\"count\":\"18446744073709551615\",\"n\":18446744073709551615,\"v\":null},\"z\":false}");
 assert(value==Json::parse(value.dump()));assert(value.at("z").isFalse());
 assert(Json::parse("\"\\u0061\\n\"").text()=="a\n");
 for(auto bad:{"{\"a\":1,\"a\":2}","{\"a\":1,\"\\u0061\":2}","{\"a\":true,}","{\"a\":-1}","{\"a\":1.0}","{\"a\":1e2}","{\"a\":01}","{\"a\":18446744073709551616}","[]","{}{}","tru", "\"\\u00e9\"", "\"\\uD800\"", "\"\\x20\""}){
  bool failed=false;try{Json::parse(bad);}catch(...){failed=true;}assert(failed);
 }
 for(auto bad:{Json("false"),Json(uint64_t(0)),Json()})assert(!bad.isFalse());
 bool failed=false;try{Json(true).integer();}catch(...){failed=true;}assert(failed);
 std::string deep;for(int i=0;i<18;++i)deep+="{\"a\":";deep+="null";deep+=std::string(18,'}');
 failed=false;try{Json::parse(deep);}catch(...){failed=true;}assert(failed);
 failed=false;try{Json::parse(std::string(1024*1024+1,' '));}catch(...){failed=true;}assert(failed);
 std::cout<<"ReceiptJsonTests passed: canonical typed objects, limits and strict ambiguous-input rejection.\n";
}
