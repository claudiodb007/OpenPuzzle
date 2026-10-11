#include "openpuzzle/engines/KeyHuntSolutionRecord.hpp"
#include <iostream>
int main(){try{
 const std::string address="1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH";
 std::vector<std::string> record={"Private Key: 1","pubkey: 0279BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798","Address "+address,"rmd160 751e76e8199196d454941c45d1b3a323f1433bd6"};
 auto check=[](bool ok){if(!ok)throw std::runtime_error("KeyHunt solution contract failed");};
 check(openpuzzle::keyHuntSolutionKey(record,address,"1","1")==std::string(63,'0')+"1");
 auto reject=[&](auto r,const auto& begin,const auto& end){bool failed=false;try{(void)openpuzzle::keyHuntSolutionKey(r,address,begin,end);}catch(...){failed=true;}check(failed);};
 reject(record,"2","3");reject(record,"2","1");auto r=record;r[0]="Private Key: 0";reject(r,"0","1");
 r=record;r[2]="Address other";reject(r,"1","1");r=record;r[1][8]='0';r[1][9]='4';reject(r,"1","1");
 r=record;r[3]+="Z";reject(r,"1","1");r=record;r.push_back("Private Key: 2");reject(r,"1","2");
 std::cout<<"KeyHuntSolutionRecordTests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
