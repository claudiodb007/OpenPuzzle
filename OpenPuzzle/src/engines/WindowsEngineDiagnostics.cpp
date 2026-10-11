#include "openpuzzle/engines/WindowsEngineDiagnostics.hpp"
#include "openpuzzle/runtime/WindowsEngineRange.hpp"
#include "openpuzzle/runtime/WindowsEngineProcess.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/CliInteger.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include "openpuzzle/core/JsonString.hpp"
#include <wincrypt.h>
#include <boost/property_tree/json_parser.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
namespace openpuzzle {
namespace fs=std::filesystem;
namespace {
const char* address1="1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH";
const char* address2="1cMh228HTCiwS8ZsaakH8A8wze1JR5ZsP";
const char* wif1="KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rFU73sVHnoWn";
const char* wif2="KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rFU74NMTptX4";
void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
int cliInteger(const std::string& value,int lower,int upper,const char* label){
 const auto result=cliIntegerArgument({"--value",value},{"--value"},0);
 require(result>=lower&&result<=upper,std::string("Out-of-range ")+label);return result;
}
std::string filename(const std::string& backend){return backend=="cpu"?"keyhunt.exe":backend=="cuda"?"cuBitCrack.exe":"clBitCrack.exe";}
std::string text(const fs::path& file,std::size_t limit=1024*1024){
 bool missing=false;auto value=windowsFile::readPrivate(file,missing,limit);
 require(value.has_value(),"Diagnostic output is missing, oversized or unreadable: "+pathUtf8(file));return *value;
}
void write(const fs::path& file,const std::string& value){require(writePrivateFileAtomically(file,value),"Private diagnostic file cannot be published");}
struct Result {std::uint32_t exit=0;bool timedOut=false;double elapsed=0;std::string log;};
Result launch(const fs::path& exe,const std::vector<std::string>& args,const fs::path& dir,int seconds){
 auto child=WindowsEngineProcess::start(exe,args,dir);
 const auto begin=std::chrono::steady_clock::now();auto exit=child->wait(static_cast<std::uint32_t>(seconds*1000));
 Result r;r.elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();r.timedOut=!exit;
 r.exit=exit?*exit:child->stop(124);r.log=text(child->logPath());return r;
}
std::string jsonString(const std::string& value){return "\""+escapeJsonString(value)+"\"";}
fs::path subdirectory(const fs::path& root,const std::string& name){const auto path=root/name;WorkspaceSecurity::prepare(path);return path;}
std::vector<std::string> searchArguments(const WindowsEngineCheckOptions& o,const fs::path& dir,
    const std::string& start,const std::string& end,bool benchmark=false){
 const auto targets=pathUtf8(dir/"targets.txt");
 if(o.backend=="cpu")return {"-m","address","-l","compress","-f",targets,"-r",start+":"+WindowsEngineRange::keyhuntExclusiveEnd(start,end),
    "-n",benchmark?"1048576":"1024","-t",benchmark?std::to_string(o.threads):"1","-s","1","-q"};
 return {"-d",std::to_string(o.device),"-b",benchmark?std::to_string(o.blocks):"1","-t",benchmark?std::to_string(o.threads):"32", "-p",benchmark?std::to_string(o.points):"1","-c","--keyspace",start+":"+end,"-i",targets,"-o",pathUtf8(dir/"found.txt"),"-f"};
}
std::string deviceIdentity(const fs::path& exe,const WindowsEngineCheckOptions& o,const fs::path& dir){
 const auto identity=launch(exe,{"--openpuzzle-engine-version"},subdirectory(dir,"identity"),30);
 const auto expected=o.backend=="cpu"?"OpenPuzzle-KeyHunt CPU windows-range protocol 1":
   o.backend=="cuda"?"OpenPuzzle-BitCrack 1.0.0 CUDA protocol 2":"OpenPuzzle-BitCrack 1.0.0 OpenCL protocol 2";
 require(identity.exit==0&&!identity.timedOut&&identity.log.find(expected)!=std::string::npos,"Engine identity/protocol does not match the selected backend");
 if(o.backend=="cpu"){SYSTEM_INFO system{};GetNativeSystemInfo(&system);const auto processors=GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  require(system.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64&&processors&&o.device==0&&o.threads<=static_cast<int>(processors),"CPU threads/device exceed the native x64 inventory");
  return "CPU x64; logical processors="+std::to_string(processors);}
 const auto inventory=launch(exe,{"--openpuzzle-device-list"},subdirectory(dir,"inventory"),30);
 require(inventory.exit==0&&!inventory.timedOut,"Engine GPU enumeration failed");
 boost::property_tree::ptree parsed;std::istringstream input(inventory.log);boost::property_tree::read_json(input,parsed);

 std::set<int> indexes;std::optional<std::string> selected;
 for(const auto& item:parsed){
  require(item.first.empty(),"Engine inventory devices is not an array");const auto& d=item.second;const auto index=d.get<int>("engine_index");
  require(index>=0&&indexes.insert(index).second,"Invalid or duplicate engine device index");
  if(!d.get<bool>("gpu"))continue;
  const auto platform=d.get<int>("platform_index"),native=d.get<int>("native_index");const auto name=d.get<std::string>("name");
  require(!name.empty()&&name.size()<=512&&native>=0&&platform>=0,"Invalid native engine mapping");
  if(index==o.device)selected=name+"; platform="+std::to_string(platform)+"; native="+std::to_string(native)+"; memory="+d.get<std::string>("memory_bytes");
 }
 require(selected.has_value(),"Requested device is absent from the engine GPU mapping");return *selected;
}
void selftest(const fs::path& exe,const WindowsEngineCheckOptions& o,const fs::path& dir){
 for(int key=1;key<=2;++key){const auto work=subdirectory(dir,"key-"+std::to_string(key));write(work/"targets.txt",std::string(key==1?address1:address2)+"\n");
  const auto result=launch(exe,searchArguments(o,work,std::to_string(key),std::to_string(key)),work,120);
  require(!result.timedOut&&result.exit==0,"Bounded synthetic engine search failed");
  const auto found=text(work/(o.backend=="cpu"?"KEYFOUNDKEYFOUND.txt":"found.txt"),4096);
  if(o.backend=="cpu"){
    std::smatch match;require(std::regex_search(found,match,std::regex("Private Key: ([0-9a-fA-F]+)"))&&
      WindowsEngineRange::normalized(match[1].str())==std::to_string(key)&&found.find(key==1?address1:address2)!=std::string::npos&&
      result.log.find("OPENPUZZLE_RANGE_DONE keys_checked=1")!=std::string::npos,"CPU synthetic solution/coverage mismatch");
  }else require(found.find("OPENPUZZLE_SOLUTION_V1")!=std::string::npos&&found.find(std::string("address=")+(key==1?address1:address2))!=std::string::npos&&
      found.find("compression=compressed")!=std::string::npos&&found.find(std::string("private_key_wif=")+(key==1?wif1:wif2))!=std::string::npos,"GPU synthetic solution/WIF mismatch");
 }
 const auto work=subdirectory(dir,"outside-range");write(work/"targets.txt",std::string(address2)+"\n");
 const auto result=launch(exe,searchArguments(o,work,"1","1"),work,120);
 const auto found=work/(o.backend=="cpu"?"KEYFOUNDKEYFOUND.txt":"found.txt");
 require(!result.timedOut&&result.exit==0&&!fs::exists(found)&&
  result.log.find(o.backend=="cpu"?"OPENPUZZLE_RANGE_DONE keys_checked=1":"Reached end of keyspace")!=std::string::npos,"Engine searched outside the inclusive one-key interval");
}
struct Measurement {double speed=0;int samples=0,lastSecond=0;};
Measurement measure(const std::string& log,const std::string& backend){
 const std::regex cpu(R"(Total\s+[0-9]+\s+keys\s+in\s+([0-9]+)\s+seconds.*\(([0-9]+)\s+keys/s\))");
 const std::regex gpu(R"(([0-9]+(?:\.[0-9]+)?)\s+MKey/s.*\[([0-9]+):([0-9]+):([0-9]+)\])");
 std::vector<double> samples;Measurement result;std::string separated=log;std::replace(separated.begin(),separated.end(),'\r','\n');std::istringstream lines(separated);std::string line;
 while(std::getline(lines,line)){std::smatch m;int second=0;double speed=0;
  if(backend=="cpu"&&std::regex_search(line,m,cpu)){second=std::stoi(m[1]);speed=std::stod(m[2])/1000000.;}
  else if(backend!="cpu"&&std::regex_search(line,m,gpu)){second=std::stoi(m[2])*3600+std::stoi(m[3])*60+std::stoi(m[4]);speed=std::stod(m[1]);}
  else continue;
  require(std::isfinite(speed)&&speed>0&&speed<=1e9&&second>=result.lastSecond,"Invalid or regressing benchmark sample");result.lastSecond=second;
  if(second>=5)samples.push_back(speed);
 }
 require(result.lastSecond>=8&&samples.size()>=3,"Benchmark has insufficient sustained samples after warm-up");
 std::sort(samples.begin(),samples.end());result.samples=static_cast<int>(samples.size());const auto middle=samples.size()/2;
 result.speed=samples.size()%2?samples[middle]:(samples[middle-1]+samples[middle])/2.;return result;
}
}
WindowsEngineCheckOptions WindowsEngineCheckOptions::parse(const std::vector<std::string>& a){
 WindowsEngineCheckOptions o;require(!a.empty()&&(a[0]=="selftest"||a[0]=="benchmark"),"Unknown engine diagnostic command");o.command=a[0];
 std::set<std::string> seen;
 for(std::size_t i=1;i<a.size();++i){const auto& flag=a[i];require(seen.insert(flag).second,"Duplicate engine option: "+flag);
  if(flag=="--real"){require(o.command=="benchmark","--real belongs to benchmark");o.real=true;continue;}
  require(i+1<a.size(),"Missing engine option value: "+flag);const auto value=a[++i];
  if(flag=="--backend"){require(value=="cpu"||value=="cuda"||value=="opencl","Select cpu, cuda or opencl");o.backend=value;}
  else if(flag=="--device")o.device=cliInteger(value,0,255,"device");
  else if(flag=="--threads"){o.threads=cliInteger(value,1,1024,"threads");o.threadsSpecified=true;}
  else if(flag=="--blocks"){o.blocks=cliInteger(value,1,65535,"blocks");o.gpuParametersSpecified=true;}
  else if(flag=="--points"){o.points=cliInteger(value,1,65536,"points");o.gpuParametersSpecified=true;}
  else if(flag=="--seconds"){require(o.command=="benchmark","--seconds belongs to benchmark");o.seconds=cliInteger(value,10,60,"seconds");}
  else throw std::invalid_argument("Unknown engine option: "+flag);
 }
 require(!o.backend.empty(),"Specify --backend cpu, cuda or opencl");
 if(o.backend=="cpu"){require(o.device==0&&!o.gpuParametersSpecified,"GPU options do not apply to CPU");
  if(!o.threadsSpecified)o.threads=static_cast<int>((std::min)(256UL,GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));
  require(o.threads>=1&&o.threads<=256,"CPU threads must be 1..256");}
 else {if(!o.threadsSpecified)o.threads=256;require(o.threads>=32&&o.threads%32==0,"GPU threads must be a multiple of 32 in 32..1024");}
 require(o.command!="selftest"||(!o.gpuParametersSpecified&&!o.threadsSpecified),"Self-test uses fixed one-key launch parameters");return o;
}
fs::path WindowsEngineDiagnostics::defaultFolder(){std::vector<wchar_t> name(32768);const auto n=GetModuleFileNameW(nullptr,name.data(),static_cast<DWORD>(name.size()));
 require(n>0&&n<name.size(),"Application directory unavailable");return fs::path(std::wstring(name.data(),n)).parent_path()/"engines";}
WindowsEngineDiagnostics::WindowsEngineDiagnostics(fs::path folder):folder_(folder.empty()?defaultFolder():std::move(folder)){}
std::string WindowsEngineDiagnostics::executableSha256(const fs::path& file){
 require(windowsFile::safePath(file),"Unsafe engine executable path");windowsFile::Handle input(CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
 BY_HANDLE_FILE_INFORMATION info{};require(input.valid()&&GetFileInformationByHandle(input.get(),&info)&&!(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)),"Engine executable is unavailable");
 HCRYPTPROV provider=0;HCRYPTHASH hash=0;
 require(CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)!=0,"SHA-256 provider unavailable");
 try {require(CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=0,"SHA-256 initialization failed");std::array<unsigned char,65536> buffer{};
  for(;;){DWORD n=0;require(ReadFile(input.get(),buffer.data(),static_cast<DWORD>(buffer.size()),&n,nullptr)!=0,"Engine executable read failed");if(!n)break;require(CryptHashData(hash,buffer.data(),n,0)!=0,"Engine hashing failed");}
  std::array<unsigned char,32> digest{};DWORD size=32;require(CryptGetHashParam(hash,HP_HASHVAL,digest.data(),&size,0)!=0&&size==32,"Engine fingerprint unavailable");
  CryptDestroyHash(hash);hash=0;CryptReleaseContext(provider,0);provider=0;std::ostringstream out;for(auto byte:digest)out<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(byte);return out.str();
 }catch(...){if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);throw;}
}
void WindowsEngineDiagnostics::describe(std::ostream& out)const{
 out<<"Engine directory: "<<pathUtf8(folder_)<<'\n';
 for(const auto backend:{"cpu","cuda","opencl"}){const auto path=folder_/filename(backend);std::error_code error;
  out<<backend<<": "<<(windowsFile::safePath(path)&&fs::is_regular_file(path,error)&&!error?"PRESENT (self-test required)":"MISSING")<<'\n';}
}
int WindowsEngineDiagnostics::run(const std::vector<std::string>& arguments,std::ostream& out)const {
 fs::path report;
 try{const auto o=WindowsEngineCheckOptions::parse(arguments);const auto exe=folder_/filename(o.backend);
  const auto fingerprint=executableSha256(exe);
  out<<"Backend: "<<o.backend<<"\nExecutable: "<<pathUtf8(exe)<<"\nDevice: "<<o.device<<"\n";
  if(o.command=="benchmark"){out<<"Threads: "<<o.threads<<"\nDuration: "<<o.seconds<<" seconds\n";
    if(o.backend!="cpu")out<<"Blocks: "<<o.blocks<<"\nPoints: "<<o.points<<"\n";
    if(!o.real){out<<"Use --real to run the local synthetic benchmark\n";return 0;}}
  for(const auto& slot:ClientRuntimeControl::executionSlots())require(ClientRuntimeControl::runtimeIdentityStatus(slot)==ClientRuntimeControl::RuntimeIdentityStatus::Inactive,"Stop existing or uncertain runtimes before engine diagnostics");
  static std::atomic<unsigned long long> sequence{0};
  report=localPaths::dataDirectory()/"engine-diagnostics"/pathFromUtf8(o.backend+'-'+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64())+'-'+std::to_string(sequence++));
  require(!fs::exists(report),"Diagnostic report already exists");WorkspaceSecurity::prepare(report);
  out<<"Report: "<<pathUtf8(report)<<'\n';
  const auto device=deviceIdentity(exe,o,report);out<<"Engine device: "<<device<<'\n';
  selftest(exe,o,report);out<<"Synthetic self-test: PASSED (keys 1, 2 and outside-range rejection)\n";
  std::optional<Measurement> measured;
  if(o.command=="benchmark"){
    const auto work=subdirectory(report,"benchmark");write(work/"targets.txt",std::string(address1)+"\n");
    const auto result=launch(exe,searchArguments(o,work,"10000000","FFFFFFFFFFFFFFFF",true),work,o.seconds);
    require((result.timedOut&&result.exit==124)||(!result.timedOut&&result.exit==0),"Benchmark engine failed");
    require(result.elapsed>=9.5&&!fs::exists(work/(o.backend=="cpu"?"KEYFOUNDKEYFOUND.txt":"found.txt")),"Benchmark ended too early or found an out-of-range target");
    measured=measure(result.log,o.backend);out<<std::fixed<<std::setprecision(3)<<"Median speed: "<<measured->speed<<" MKey/s\nSamples after warm-up: "<<measured->samples<<'\n';
  }
  require(executableSha256(exe)==fingerprint,"Engine changed during validation; result not published");
  std::ostringstream value;value<<"{\"version\":1,\"backend\":"<<jsonString(o.backend)<<",\"engine_sha256\":"<<jsonString(fingerprint)<<",\"device\":"<<o.device<<",\"device_identity\":"<<jsonString(device)<<",\"selftest_passed\":true,\"threads\":"<<o.threads<<",\"blocks\":"<<o.blocks<<",\"points\":"<<o.points;
  if(measured)value<<std::setprecision(17)<<",\"speed_mkeys\":"<<measured->speed<<",\"samples\":"<<measured->samples<<",\"duration_seconds\":"<<o.seconds;
  value<<"}\n";write(report/"result.json",value.str());
  out<<"Engine SHA-256: "<<fingerprint<<"\nLocal validation: PASSED\nNo assignment requested\n";return 0;
 }catch(const std::exception& e){out<<"Engine diagnostics refused: "<<e.what()<<'\n';if(!report.empty())out<<"Report preserved: "<<pathUtf8(report)<<'\n';return 1;}
}
}
