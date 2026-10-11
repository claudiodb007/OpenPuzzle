#include "openpuzzle/core/LocalPaths.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
namespace fs=std::filesystem;
int wmain(int argc,wchar_t** argv){
 try{
  const auto name=fs::path(argv[0]).filename().wstring();const bool cpu=name==L"keyhunt.exe",cuda=name==L"cuBitCrack.exe";
  char modeBuffer[128]{};GetEnvironmentVariableA("OPENPUZZLE_BINDING_FIXTURE_MODE",modeBuffer,sizeof(modeBuffer));const std::string mode=modeBuffer;
  if(argc==2&&std::wstring(argv[1])==L"--openpuzzle-engine-version"){
   if(mode=="wrong-protocol"){std::cout<<"unverified executable"<<std::endl;return 0;}
   std::cout<<(cpu?"OpenPuzzle-KeyHunt CPU windows-range protocol 1":cuda?"OpenPuzzle-BitCrack 1.0.0 CUDA protocol 2":"OpenPuzzle-BitCrack 1.0.0 OpenCL protocol 2")<<std::endl;return 0;
  }
  if(argc==2&&std::wstring(argv[1])==L"--openpuzzle-device-list"){
   if(mode=="bad-json"){std::cout<<"not-json";return 0;}
   std::cout<<"[{\"engine_index\":0,\"platform_index\":0,\"native_index\":2,\"gpu\":true,\"name\":\"fixture GPU\",\"memory_bytes\":10737418240}]";return 0;
  }
  std::wstring range;fs::path targets,result=fs::current_path()/(cpu?"KEYFOUNDKEYFOUND.txt":"found.txt");
  for(int i=1;i<argc-1;++i){const std::wstring option=argv[i];if(option==L"-r"||option==L"--keyspace")range=argv[i+1];
   if((option==L"-f"&&cpu)||option==L"-i")targets=argv[i+1];if(option==L"-o")result=argv[i+1];}
  if(mode=="search-failure")return 9;
  if(range.starts_with(L"10000000:")){
   if(mode=="early-exit"){std::cout<<"[+] Total 1 keys in 1 seconds: ~1 Mkeys/s (1000000 keys/s)"<<std::endl;return 0;}
   for(int i=1;i<=20;++i){Sleep(1000);if(cpu)std::cout<<"[+] Total "<<i*1000000<<" keys in "<<i<<" seconds: ~1 Mkeys/s (1000000 keys/s)"<<std::endl;
    else std::cout<<"fixture 123.50 MKey/s (1234 total) [00:00:"<<(i<10?"0":"")<<i<<"]"<<std::endl;}
   return 0;
  }
  std::ifstream input(targets);std::string target;std::getline(input,target);
  const bool key2=range==L"2:3"||range==L"2:2";const bool key1=range==L"1:2"||range==L"1:1";
  if((key1&&target=="1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH")||(key2&&target=="1cMh228HTCiwS8ZsaakH8A8wze1JR5ZsP")||mode=="overshoot"){
   std::ofstream found(result);if(cpu)found<<"Private Key: "<<(key2?"2":"1")<<"\naddress: "<<target<<'\n';
   else found<<"OPENPUZZLE_SOLUTION_V1\naddress="<<target<<"\ncompression=compressed\nprivate_key_wif="<<(mode=="wrong-wif"?"invalid":key2?"KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rFU74NMTptX4":"KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rFU73sVHnoWn")<<'\n';
  }
  std::cout<<(cpu?"OPENPUZZLE_RANGE_DONE keys_checked=1":"Reached end of keyspace")<<std::endl;return 0;
 }catch(...){return 10;}
}
