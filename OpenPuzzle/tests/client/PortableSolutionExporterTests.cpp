#include "openpuzzle/client/SolutionExporter.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#else
#include <unistd.h>
#endif
using namespace openpuzzle;
namespace fs=std::filesystem;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
int main(){fs::path root;try{
#ifdef _WIN32
  const auto id=GetCurrentProcessId();
  const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);std::wstring old(n,L'\0');
  if(n){GetEnvironmentVariableW(L"LOCALAPPDATA",old.data(),n);old.resize(n-1);}
#else
  const auto id=getpid();const auto old=std::getenv("HOME")?std::string(std::getenv("HOME")):std::string{};
#endif
  root=fs::temp_directory_path()/pathFromUtf8("OpenPuzzle-export-ç-中文-"+std::to_string(id));
  require(!fs::exists(root),"Export fixture path already exists");fs::create_directory(root);
#ifdef _WIN32
  require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()),"Fixture environment failed");
#else
  require(setenv("HOME",root.c_str(),1)==0,"Fixture environment failed");
#endif
  WorkspaceSecurity::prepare(root/"result");
  client::ClientExecutionState state;state.assignmentId="fixture-local-result";state.puzzle=20;
  state.target="fixture-address";state.engine="BitCrack";state.start="0";state.end="F";
  const auto input=root/"result"/"found.txt";
  // Synthetic, syntax-only fixture, never a real wallet or production result.
  const auto synthetic=std::string("K")+std::string(51,'1');
  const auto record="OPENPUZZLE_SOLUTION_V1\r\naddress=fixture-address\r\nprivate_key_wif="+synthetic+"\r\ncompression=compressed\r\n";
  require(writePrivateFileAtomically(input,record),"Result fixture publication failed");
  auto exported=client::SolutionExporter::exportSolution(state,pathUtf8(input));
  require(exported.success&&!exported.walletPath.empty()&&!exported.noticePath.empty(),"Private export failed");
  const auto wallet=pathFromUtf8(exported.walletPath),notice=pathFromUtf8(exported.noticePath);
  const auto saved=read(wallet),noticeSaved=read(notice);
  require(saved.find(synthetic)!=std::string::npos&&noticeSaved.find(synthetic)==std::string::npos,"Notice disclosed fixture material");
#ifdef _WIN32
  require(pathUtf8(wallet).find("OpenPuzzle")!=std::string::npos,"Windows private solution location missing");
  windowsFile::Security security;windowsFile::Handle handle(CreateFileW(wallet.c_str(),READ_CONTROL,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr));
  require(handle.valid(),"Wallet inspection failed");PSECURITY_DESCRIPTOR descriptor=nullptr;PACL acl=nullptr;
  require(GetSecurityInfo(handle.get(),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&acl,nullptr,&descriptor)==ERROR_SUCCESS,"Wallet ACL unavailable");
  SECURITY_DESCRIPTOR_CONTROL control{};DWORD revision=0;
  require(GetSecurityDescriptorControl(descriptor,&control,&revision)&& (control&SE_DACL_PROTECTED)&&acl&&acl->AceCount==1,"Wallet permissions are not private");
  void* ace=nullptr;require(GetAce(acl,0,&ace)&&EqualSid(&reinterpret_cast<ACCESS_ALLOWED_ACE*>(ace)->SidStart,security.sid()),"Wallet ACL belongs to another identity");
  LocalFree(descriptor);handle.close();
#else
  require((fs::status(wallet).permissions()&fs::perms::group_all)==fs::perms::none&&
          (fs::status(wallet).permissions()&fs::perms::others_all)==fs::perms::none,"Wallet permissions are not private");
#endif
  require(writePrivateFileAtomically(input,"OPENPUZZLE_SOLUTION_V1\naddress=fixture-address\nprivate_key_wif=L"+std::string(51,'2')+"\ncompression=compressed\n"),"Replacement fixture failed");
  exported=client::SolutionExporter::exportSolution(state,pathUtf8(input));
  require(exported.success&&read(wallet)==saved&&read(notice)==noticeSaved,"Previously exported files were replaced");
  state.assignmentId="../escape";require(!client::SolutionExporter::exportSolution(state,pathUtf8(input)).success,"Unsafe assignment accepted");
  state.assignmentId="fixture-bad";state.target="other-address";require(!client::SolutionExporter::exportSolution(state,pathUtf8(input)).success,"Wrong address accepted");
  state.target="fixture-address";require(writePrivateFileAtomically(input,std::string(4097,'x')),"Oversize fixture failed");require(!client::SolutionExporter::exportSolution(state,pathUtf8(input)).success,"Oversized result accepted");
#ifdef _WIN32
  require(writePrivateFileAtomically(input,record),"Restored result fixture failed");
  const auto hard=root/"result"/"hardlink.txt";require(CreateHardLinkW(hard.c_str(),input.c_str(),nullptr),"Hardlink fixture failed");
  require(!client::SolutionExporter::exportSolution(state,pathUtf8(hard)).success,"Hardlinked engine result accepted");fs::remove(hard);
  const auto outside=root/"outside.txt";require(writePrivateFileAtomically(outside,"preserved-outside"),"Outside fixture failed");
  fs::remove(wallet);require(CreateHardLinkW(wallet.c_str(),outside.c_str(),nullptr),"Wallet hardlink fixture failed");
  state.assignmentId="fixture-local-result";require(!client::SolutionExporter::exportSolution(state,pathUtf8(input)).success,"Hardlinked wallet accepted");
  require(read(input)==record&&read(outside)=="preserved-outside","Unsafe export modified input or linked destination");
  require(SetEnvironmentVariableW(L"LOCALAPPDATA",L"relative\\invalid"),"Invalid environment fixture failed");
  require(!client::SolutionExporter::exportSolution(state,pathUtf8(input)).success,"Relative data root accepted");
  SetEnvironmentVariableW(L"LOCALAPPDATA",old.empty()?nullptr:old.c_str());
#else
  setenv("HOME",old.c_str(),1);
#endif
  fs::remove_all(root);std::cout<<"PortableSolutionExporterTests passed: private Unicode paths, bounded input and existing exports preserved\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
