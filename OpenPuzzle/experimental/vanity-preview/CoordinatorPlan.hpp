// MIT. Server-assigned linear intervals split for the unchanged measured adapter.
#pragma once
#include "openpuzzle/client/RangeAssignment.hpp"
#include "openpuzzle/runtime/VanityRangeProtocol.hpp"
#include <regex>
namespace openpuzzle::vanityPreview {
struct CoordinatorPlan {
 static constexpr uint64_t MaxSegment=uint64_t(1)<<40;
 std::string start,end;uint64_t total,limit,blocks;
 CoordinatorPlan(const client::RangeAssignment& a,int puzzle,const std::string& target,uint64_t grid,uint64_t cap=MaxSegment)
 :start(canonical(a.start)),end(canonical(a.end)),total(0),limit(cap),blocks(grid){
  if(!a.valid()||!std::regex_match(a.assignmentId,std::regex("[A-Za-z0-9-]{1,128}"))||a.puzzle!=puzzle||a.target!=target||a.searchMode!="linear"||!a.publicKey.empty()||a.requiredBackend!="cuda"||end<start)
   throw std::runtime_error("Coordinator assignment identity/target/mode differs");
  if(!grid||grid>4096||(grid&(grid-1))||!cap||cap>MaxSegment||cap%(grid*256*1024))throw std::runtime_error("Invalid coordinator segment/grid");
  if(start<canonical("10000000000000000")||end>=canonical("100000000000000000000"))throw std::runtime_error("Assignment outside validated scalar width");
  uint64_t diff=0;int borrow=0;
  auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};
  for(int i=63;i>=0;--i){int n=digit(end[i])-digit(start[i])-borrow;borrow=n<0;if(borrow)n+=16;if(i<48&&n)throw std::runtime_error("Assignment exceeds unsigned native count");if(i>=48)diff|=uint64_t(n)<<((63-i)*4);}
  if(diff==UINT64_MAX)throw std::runtime_error("Assignment count overflow");total=diff+1;
 }
 Plan segment(uint64_t offset)const{if(offset>=total||offset%limit)throw std::runtime_error("Invalid completed-segment offset");return Plan(add(start,offset),std::min(limit,total-offset),blocks);}
};
}
