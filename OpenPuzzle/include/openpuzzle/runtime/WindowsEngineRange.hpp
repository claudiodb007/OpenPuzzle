#pragma once
#include <algorithm>
#include <stdexcept>
#include <string>
namespace openpuzzle {
// The Windows KeyHunt protocol counts private scalars and takes an exclusive
// end. Server assignment boundaries remain inclusive and are never rewritten.
struct WindowsEngineRange {
  static std::string normalized(std::string value) {
    if(value.empty()||value.size()>64)throw std::invalid_argument("Invalid scalar boundary");
    for(char& c:value){if(c>='a'&&c<='f')c=static_cast<char>(c-'a'+'A');
      if(!((c>='0'&&c<='9')||(c>='A'&&c<='F')))throw std::invalid_argument("Non-hexadecimal scalar boundary");}
    const auto first=value.find_first_not_of('0');return first==std::string::npos?"0":value.substr(first);
  }
  static bool before(const std::string& a,const std::string& b){return a.size()<b.size()||(a.size()==b.size()&&a<b);}
  static std::string keyhuntExclusiveEnd(const std::string& first,const std::string& last) {
    const auto start=normalized(first);auto end=normalized(last);
    const std::string order="FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141";
    if(start=="0"||before(end,start)||!before(end,order))throw std::invalid_argument("Range is outside the private scalar domain");
    for(std::size_t i=end.size();i>0;--i){char& c=end[i-1];if(c=='F'){c='0';continue;}c=c=='9'?'A':static_cast<char>(c+1);return end;}
    return "1"+end;
  }
};
}
