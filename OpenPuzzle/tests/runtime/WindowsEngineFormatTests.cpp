#include "openpuzzle/core/WindowsCommandLine.hpp"
#include "openpuzzle/runtime/WindowsEngineRecord.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace openpuzzle;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
 require(WindowsCommandLine::quote("")=="\"\"","Empty argument lost");
 require(WindowsCommandLine::quote("a b")=="\"a b\"","Spaces lost");
 require(WindowsCommandLine::quote("a\"b")=="\"a\\\"b\"","Quote not escaped");
 require(WindowsCommandLine::quote("a\\")=="\"a\\\\\"","Trailing slash not doubled");
 require(WindowsCommandLine::quote("a\\\"b")=="\"a\\\\\\\"b\"","Slash before quote not doubled");
 require(WindowsCommandLine::quote("%PATH% & > x")=="\"%PATH% & > x\"","Shell syntax transformed");
 require(WindowsCommandLine::quote("ç 中文")=="\"ç 中文\"","Unicode bytes altered");
 bool refused=false;try{WindowsCommandLine::quote(std::string("a\0b",3));}catch(const std::invalid_argument&){refused=true;}require(refused,"NUL accepted");
 refused=false;try{WindowsCommandLine::build("bad\"exe",{});}catch(const std::invalid_argument&){refused=true;}require(refused,"Invalid executable accepted");
 WindowsEngineRecord record{{123,456},"123-456-0"};
 require(WindowsEngineRecord::parse(record.encode())==record,"Engine identity round trip failed");
 require(WindowsEngineRecord::completionCode(record.completion(259),record)==259,"STILL_ACTIVE numeric exit lost");
 require(WindowsEngineRecord::completionCode(record.completion(0),record)==0,"Zero exit lost");
 require(WindowsEngineRecord::completionCode(record.completion((std::numeric_limits<std::uint32_t>::max)()),record)==(std::numeric_limits<std::uint32_t>::max)(),"DWORD exit truncated");
 auto other=record;other.process.creationTime++;
 require(!WindowsEngineRecord::completionCode(other.completion(0),record),"Foreign completion authorized success");
 for(auto text:{record.encode()+"extra",record.process.encode(),std::string("linux\n123\n456\nx\n"),std::string("windows-engine-filetime-v1\n123\n456\n../escape\n"),std::string("windows-engine-filetime-v1\n0\n456\nx\n"),std::string("windows-engine-filetime-v1\n123\n0\nx\n"),std::string("windows-engine-filetime-v1\n123\n456\nx/escape\n"),std::string("windows-engine-filetime-v1\n123\n456\nx:stream\n")})require(!WindowsEngineRecord::parse(text),"Malformed/foreign identity accepted");
 for(auto text:{record.encode()+"-1\n",record.encode()+"+1\n",record.encode()+"4294967296\n",record.encode()+"1extra\n",record.completion(0)+"extra"})require(!WindowsEngineRecord::completionCode(text,record),"Invalid completion accepted");
 std::cout<<"WindowsEngineFormatTests passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
