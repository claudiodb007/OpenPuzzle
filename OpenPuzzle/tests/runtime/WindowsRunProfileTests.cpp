#include "openpuzzle/engines/WindowsRunProfile.hpp"
#include <iostream>
int main(){try{
 const auto make=[](std::string backend="cuda",std::string speed="1332.55") {return "{\"version\":1,\"backend\":\""+backend+"\",\"engine_sha256\":\""+std::string(64,'a')+"\",\"device\":0,\"device_identity\":\"fixture GPU\",\"selftest_passed\":true,\"threads\":256,\"blocks\":256,\"points\":256,\"samples\":12,\"duration_seconds\":30,\"speed_mkeys\":"+speed+"}";};
 auto check=[](bool ok){if(!ok)throw std::runtime_error("Measured profile contract failed");};
 auto p=openpuzzle::WindowsRunProfile::parse(make());auto bound=p.apply(openpuzzle::RunCommandOptions::parse({"run"}));
 check(bound.threadsSpecified&&bound.threads==256&&bound.backend=="cuda"&&p.speed==1332.55);
 auto reject=[&](const auto& value){bool failed=false;try{openpuzzle::WindowsRunProfile::parse(value);}catch(...){failed=true;}check(failed);};
 for(const auto& speed:{"0","-1","1e999","null","\"1junk\""})reject(make("cuda",speed));
 auto s=make();s.insert(1,"\"backend\":\"cpu\",");reject(s);
 s=make();s.replace(s.find("true"),4,"false");reject(s);
 s=make();s.replace(s.find("\"samples\":12"),12,"\"samples\":1");reject(s);
 for(const auto& args:{std::vector<std::string>{"run","--backend","cpu"},std::vector<std::string>{"run","--threads","128"},std::vector<std::string>{"run","--points","1024"},std::vector<std::string>{"run","--device","1"}}){bool failed=false;try{p.apply(openpuzzle::RunCommandOptions::parse(args));}catch(...){failed=true;}check(failed);}
 check(p.apply(openpuzzle::RunCommandOptions::parse({"run","--blocks","256","--threads","256","--points","256"})).points==256);
 std::cout<<"WindowsRunProfileTests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
