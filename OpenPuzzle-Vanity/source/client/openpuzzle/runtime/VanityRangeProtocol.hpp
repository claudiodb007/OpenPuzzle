// MIT License. Experimental protocol only; no production engine registration.
#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace openpuzzle { namespace vanityPreview {
inline uint64_t decimal(const std::string& text) {
  if(text.empty() || text.find_first_not_of("0123456789")!=std::string::npos)
    throw std::runtime_error("Invalid unsigned integer");
  uint64_t value=0;
  for(char c:text) {
    unsigned digit=unsigned(c-'0');
    if(value>(UINT64_MAX-digit)/10) throw std::runtime_error("Integer overflow");
    value=value*10+digit;
  }
  return value;
}
inline std::string canonical(std::string value) {
  if(value.empty() || value.size()>64 || value.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)
    throw std::runtime_error("Invalid hexadecimal scalar");
  for(char& c:value) if(c>='A'&&c<='F') c+=32;
  return std::string(64-value.size(),'0')+value;
}
inline std::string add(std::string value,uint64_t increment) {
  value=canonical(value);
  const char* digits="0123456789abcdef";
  for(int i=63;i>=0;--i) {
    unsigned n=value[i]<='9'?value[i]-'0':value[i]-'a'+10;
    uint64_t sum=n+(increment&15u);
    value[i]=digits[sum&15u]; increment=(increment>>4)+(sum>>4);
  }
  if(increment) throw std::runtime_error("Scalar overflow");
  return value;
}
struct Plan {
  std::string start,end;
  uint64_t count,batch,blocks;
  Plan(const std::string& first,uint64_t size,uint64_t grid):start(canonical(first)),count(size),blocks(grid) {
    if(!count || count>(uint64_t(1)<<40) || !blocks || blocks>4096 || (blocks&(blocks-1)))
      throw std::runtime_error("Experimental count/grid limit");
    end=add(start,count-1); batch=blocks*256*1024;
    if(start<canonical("10000000000000000") || end>=canonical("100000000000000000000"))
      throw std::runtime_error("Preview scalar range must lie within [2^64,2^80)");
  }
};
// A progress record acknowledges a completed contiguous batch, never a
// scattered sum of thread-local ranges. No completion without terminal+exit.
class Protocol {
public:
  explicit Protocol(Plan expected):plan(std::move(expected)) {}
  Plan plan;
  uint64_t checked=0;
  bool begun=false,terminal=false,complete=false,solution=false,stopped=false,traced=false;
  void consume(const std::string& line) {
    if(line.rfind("OPV1 ",0)!=0) return; // Diagnostic line, not protocol evidence.
    std::istringstream in(line);std::string magic,kind,a,b,c,extra;in>>magic>>kind;
    if(terminal) throw std::runtime_error("Protocol after terminal record");
    if(kind=="BEGIN") {
      if(begun || !(in>>a>>b>>c) || (in>>extra) || a!=plan.start || decimal(b)!=plan.count || decimal(c)!=plan.batch)
        throw std::runtime_error("Unexpected range identity");
      begun=true;return;
    }
    if(!begun || !(in>>a) || (in>>extra)) throw std::runtime_error("Malformed protocol");
    uint64_t n=decimal(a);
    if(kind=="PROGRESS") {
      if(checked==plan.count || n!=checked+std::min(plan.batch,plan.count-checked))
        throw std::runtime_error("Skipped, duplicate or regressing progress");
      checked=n;
    } else if(kind=="TRACE") {
      if(traced || n!=checked || checked!=plan.count || plan.count>65536)
        throw std::runtime_error("Invalid trace proof");
      traced=true;
    } else if(kind=="COMPLETE" || kind=="SOLUTION" || kind=="STOPPED") {
      if(n!=checked || (kind=="COMPLETE" && checked!=plan.count))
        throw std::runtime_error("Invalid terminal coverage");
      terminal=true;complete=kind=="COMPLETE";solution=kind=="SOLUTION";stopped=kind=="STOPPED";
    } else throw std::runtime_error("Unknown protocol record");
  }
  bool completed(int code) const { return code==0 && complete && checked==plan.count; }
};
} }
