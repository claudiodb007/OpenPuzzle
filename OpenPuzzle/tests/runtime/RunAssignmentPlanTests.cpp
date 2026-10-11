#include "openpuzzle/runtime/RunAssignmentPlan.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/WindowsCommandLine.hpp"
#include <iostream>
#include <algorithm>
#include <stdexcept>
using namespace openpuzzle;
void require(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
int main(){try{
 const std::string target="1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";
 client::RangeAssignment a;a.assignmentId="fixture-assignment-1";a.puzzle=71;a.rangeId=123;a.target=target;a.start="000a";a.end="F";
 auto o=RunCommandOptions::parse({"run","71","--engine","bitcrack","--backend","cuda","--device","2"});
 const auto work=std::filesystem::temp_directory_path()/pathFromUtf8("OpenPuzzle-plan ç 中文 & literal")/a.assignmentId;
 require(!std::filesystem::exists(work),"Fixture workspace already exists");
 PuzzleExecutionMetadata m;m.puzzle=71;m.address=target;m.keyspace="0:FF";
 const auto p=RunAssignmentPlan::build(o,a,"fixture-client",work,&m);
 require(p.state.start=="A"&&p.state.end=="F"&&p.state.assignmentId==a.assignmentId&&p.state.device==2,"Exact assignment metadata changed");
 auto expected=std::vector<std::string>{target,"--keyspace","A:F","--out",pathUtf8(work/"found.txt"),"-d","2","-b","256","-t","256","-p","256"};
#ifdef _WIN32
 expected.push_back("-c");
#endif
 require(p.arguments==expected,"Direct BitCrack argument contract changed");
 require(p.targetFileContent.empty()&&!std::filesystem::exists(work),"Planning had filesystem side effects");
 o=RunCommandOptions::parse({"run","--engine","keyhunt","--threads","4"});a.start="F";a.end="F";
 const auto cpu=RunAssignmentPlan::build(o,a,"fixture-client",work);
 require(cpu.state.blocks==0&&cpu.state.points==0&&cpu.state.threads==4&&cpu.targetFileContent==target+"\n","CPU plan lost target/thread metadata");
#ifdef _WIN32
 require(std::find(cpu.arguments.begin(),cpu.arguments.end(),"F:10")!=cpu.arguments.end(),"Windows KeyHunt lost the inclusive last key");
#else
 require(std::find(cpu.arguments.begin(),cpu.arguments.end(),"F:F")!=cpu.arguments.end(),"Legacy Linux one-key interval changed");
#endif
 require(WindowsCommandLine::build("C:\\engine with spaces\\keyhunt.exe",cpu.arguments).find("&&")==std::string::npos,"Native arguments contain shell preparation");
 auto reject=[&](auto bad,auto options,const PuzzleExecutionMetadata* meta=nullptr){bool failed=false;try{RunAssignmentPlan::build(options,bad,"fixture-client",work,meta);}catch(const std::exception&){failed=true;}require(failed,"Unsafe assignment plan accepted");};
 auto bad=a;bad.assignmentId="../../outside";reject(bad,o);bad=a;bad.assignmentId="CON";
 require(!RunAssignmentPlan::safeAssignmentId("CON")&&!RunAssignmentPlan::safeAssignmentId("LPT1")&&!RunAssignmentPlan::safeAssignmentId("..")&&!RunAssignmentPlan::safeAssignmentId("slot:name")&&!RunAssignmentPlan::safeAssignmentId("a/b"),"Unsafe directory component accepted");
 bad=a;bad.start="10";bad.end="F";reject(bad,o);bad=a;bad.end=std::string(65,'F');reject(bad,o);
 bad=a;bad.start="xyz";reject(bad,o);bad=a;bad.target="address\nextra";reject(bad,o);
 bad=a;bad.searchMode="kangaroo";reject(bad,o);bad=a;bad.requiredBackend="cuda";reject(bad,o);
 auto explicitPuzzle=o;explicitPuzzle.puzzle=72;reject(a,explicitPuzzle);
 m.keyspace="10:FF";reject(a,o,&m);m.keyspace="0:A";reject(a,o,&m);m.keyspace="0:FF";m.address="other";reject(a,o,&m);
 require(!std::filesystem::exists(work),"Rejected planning created a workspace");
 std::cout<<"RunAssignmentPlanTests passed: direct argv, inclusive ranges, metadata agreement, Unicode paths and no side effects\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
