// GPL-3.0-only. Public compressed P2PKH/HASH160 input; no private-key input.
#pragma once
#include <algorithm>
#include <cstdint>
#include "Base58.h"
#include "hash/sha256.h"
#include <array>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
namespace opvTargets {
struct Target { std::array<unsigned char,20> hash; std::string address; };
inline std::string hex(const unsigned char* p,size_t n) {
 const char* digits="0123456789abcdef";std::string s;
 for(size_t i=0;i<n;++i){s+=digits[p[i]>>4];s+=digits[p[i]&15];}return s;
}
inline std::string address(const unsigned char* h) {
 unsigned char bytes[25]={0};std::copy(h,h+20,bytes+1);
 sha256_checksum(bytes,21,bytes+21);return EncodeBase58(bytes,bytes+25);
}
inline Target parse(const std::string& kind,const std::string& value) {
 Target t;
 if(kind=="hash160") {
  if(value.size()!=40||value.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)throw std::runtime_error("Invalid HASH160");
  for(size_t i=0;i<20;++i)t.hash[i]=static_cast<unsigned char>(std::stoul(value.substr(i*2,2),nullptr,16));
 }else if(kind=="p2pkh") {
  if(value.size()<26||value.size()>35)throw std::runtime_error("Invalid P2PKH length");
  std::vector<unsigned char> bytes;
  if(!DecodeBase58(value,bytes)||bytes.size()!=25||bytes[0]!=0)throw std::runtime_error("Only mainnet P2PKH targets are supported");
  unsigned char checksum[4];sha256_checksum(bytes.data(),21,checksum);
  if(!std::equal(checksum,checksum+4,bytes.begin()+21))throw std::runtime_error("Invalid P2PKH checksum");
  std::copy(bytes.begin()+1,bytes.begin()+21,t.hash.begin());
 }else throw std::runtime_error("Target must be p2pkh or hash160");
 t.address=address(t.hash.data());
 if(kind=="p2pkh"&&t.address!=value)throw std::runtime_error("Noncanonical P2PKH target");
 return t;
}
inline std::map<std::string,Target> load(const std::string& path) {
 struct stat st;
 if(lstat(path.c_str(),&st)||!S_ISREG(st.st_mode)||st.st_size>8192)throw std::runtime_error("Missing/unsafe/oversized target file");
 std::ifstream f(path);std::string line;if(!std::getline(f,line)||line!="OPV-TARGETS-1")throw std::runtime_error("Invalid target file header");
 std::map<std::string,Target> targets;
 while(std::getline(f,line)) {
  std::istringstream in(line);std::string kind,value,extra;
  if(!(in>>kind>>value)||(in>>extra))throw std::runtime_error("Invalid target record");
  auto t=parse(kind,value);
  if(!targets.emplace(hex(t.hash.data(),20),t).second)throw std::runtime_error("Duplicate target");
  if(targets.size()>64)throw std::runtime_error("Too many targets");
 }
 if(targets.empty()||f.bad())throw std::runtime_error("Empty/unreadable target file");return targets;
}
}
