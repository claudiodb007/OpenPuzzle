#pragma once
#include "openpuzzle/runtime/WindowsEngineRange.hpp"
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
namespace openpuzzle {
// Native address-mode output only. Preserve the original engine file; never
// infer completion from a solution or accept another target/range.
inline std::string keyHuntSolutionKey(const std::vector<std::string>& lines,
    const std::string& address,const std::string& start,const std::string& end) {
  if(lines.size()!=4 || !lines[0].starts_with("Private Key: ") ||
     !lines[1].starts_with("pubkey: ") || lines[2]!="Address "+address ||
     !lines[3].starts_with("rmd160 ")) throw std::invalid_argument("Unsupported native KeyHunt solution");
  const auto key=WindowsEngineRange::normalized(lines[0].substr(13));
  const auto begin=WindowsEngineRange::normalized(start),finish=WindowsEngineRange::normalized(end);
  const auto less=[](const auto& a,const auto& b){return a.size()<b.size()||(a.size()==b.size()&&a<b);};
  // The scalar helper also rejects zero and values outside secp256k1.
  (void)WindowsEngineRange::keyhuntExclusiveEnd(key,key);
  if(less(key,begin)||less(finish,key)||less(finish,begin))throw std::invalid_argument("KeyHunt solution is outside its assignment");
  const auto pub=lines[1].substr(8),hash=lines[3].substr(7);
  if(pub.size()!=66 || (pub.substr(0,2)!="02"&&pub.substr(0,2)!="03") || hash.size()!=40)
    throw std::invalid_argument("KeyHunt compressed public key/hash format is invalid");
  const auto hex=[](const auto& value){return std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||(c>='A'&&c<='F')||(c>='a'&&c<='f');});};
  if(!hex(pub)||!hex(hash))throw std::invalid_argument("KeyHunt public key/hash is not hexadecimal");
  return std::string(64-key.size(),'0')+key;
}
}
