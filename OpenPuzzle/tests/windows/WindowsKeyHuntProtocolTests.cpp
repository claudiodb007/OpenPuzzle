#include "openpuzzle/adapters/keyhunt/KeyHuntProgressParser.hpp"
#include <iostream>
#include <stdexcept>
using namespace openpuzzle;
void check(bool value){if(!value)throw std::runtime_error("Windows KeyHunt scalar protocol assertion failed");}
int main(){try{
 keyhunt::KeyHuntProgressParser parser(keyhunt::KeyHuntProgressParser::CountProtocol::PrivateScalars);
 auto progress=parser.parseLine("[+] Total 16777216 keys in 1 seconds: ~16 Mkeys/s (16777216 keys/s)");
 check(progress&&progress->keysChecked=="16777216"&&progress->speedMKeys==16.777216&&!progress->finished);
 check(!parser.parseLine("End"));
 auto done=parser.parseLine("OPENPUZZLE_RANGE_DONE keys_checked=1");check(done&&done->finished&&done->keysChecked=="1");
 done=parser.parseLine("OPENPUZZLE_RANGE_DONE keys_checked=18446744073709551615");check(done&&done->keysChecked=="18446744073709551615");
 for(const auto& bad:{"OPENPUZZLE_RANGE_DONE keys_checked=-1","OPENPUZZLE_RANGE_DONE keys_checked=18446744073709551616","OPENPUZZLE_RANGE_DONE keys_checked=1 trailing"})check(!parser.parseLine(bad));
 keyhunt::KeyHuntProgressParser legacy(keyhunt::KeyHuntProgressParser::CountProtocol::LegacyCompressedHashes);
 progress=legacy.parseLine("[+] Total 2000000 keys in 1 seconds: ~2 Mkeys/s (2000000 keys/s)");
 check(progress&&progress->keysChecked=="1000000"&&progress->speedMKeys==1.0);
 check(legacy.parseLine("End")->finished);
 std::cout<<"WindowsKeyHuntProtocolTests passed: scalar counts/speed retained, exact terminal proof and overflow rejection\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
