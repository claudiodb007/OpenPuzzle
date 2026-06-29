#include "openpuzzle/models/KeySpace.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
namespace openpuzzle {
KeySpace::KeySpace(Int start, Int end) : start_(std::move(start)), end_(std::move(end)) {}
KeySpace KeySpace::fromHexPair(const std::string& startHex, const std::string& endHex){ return KeySpace(hexToInt(startHex), hexToInt(endHex)); }
KeySpace KeySpace::fromColonString(const std::string& text){ auto pos=text.find(':'); if(pos==std::string::npos) throw std::runtime_error("Invalid keyspace format"); return fromHexPair(text.substr(0,pos), text.substr(pos+1)); }
KeySpace::Int KeySpace::sizeInclusive() const { if(!valid()) return 0; return end_-start_+1; }
bool KeySpace::valid() const { return start_<=end_; }
KeySpace::Int KeySpace::hexToInt(const std::string& input){ std::string hex=input; if(hex.rfind("0x",0)==0||hex.rfind("0X",0)==0) hex=hex.substr(2); Int value=0; for(char c:hex){ if(std::isspace((unsigned char)c)) continue; int n=0; if(c>='0'&&c<='9') n=c-'0'; else if(c>='a'&&c<='f') n=10+c-'a'; else if(c>='A'&&c<='F') n=10+c-'A'; else throw std::runtime_error("Invalid hex"); value<<=4; value+=n;} return value; }
std::string KeySpace::intToHex(const Int& value,int minWidth){ if(value<0) throw std::runtime_error("negative hex"); if(value==0) return std::string(std::max(1,minWidth),'0'); Int t=value; std::string out; const char* d="0123456789ABCDEF"; while(t>0){ unsigned n=(unsigned)(t & 0xF); out.push_back(d[n]); t >>= 4;} std::reverse(out.begin(),out.end()); if((int)out.size()<minWidth) out.insert(out.begin(),minWidth-out.size(),'0'); return out; }
std::string KeySpace::startHex(int minWidth) const { return intToHex(start_, minWidth); }
std::string KeySpace::endHex(int minWidth) const { return intToHex(end_, minWidth); }
std::string KeySpace::toColonString(int minWidth) const { return startHex(minWidth)+":"+endHex(minWidth); }
}
