#include "CoordinatorPlan.hpp"
#include <cassert>
#include <functional>
#include <iostream>
using namespace openpuzzle::vanityPreview;
void refused(const std::function<void()>& f){bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
int main(){
 openpuzzle::client::RangeAssignment a;a.assignmentId="assigned-1";a.puzzle=71;a.rangeId=1;a.target="1FixturePublicAddress";a.requiredBackend="cuda";a.start="400000000000000000";
 const uint64_t count=(uint64_t(1)<<44)+17;a.end=add(a.start,count-1);
 CoordinatorPlan p(a,71,a.target,1024);assert(p.total==count);uint64_t offset=0;unsigned chunks=0;
 while(offset<p.total){auto s=p.segment(offset);assert(s.count<=p.MaxSegment&&s.start==add(a.start,offset));offset+=s.count;assert(s.end==add(a.start,offset-1));++chunks;}
 assert(offset==count&&chunks==17&&p.segment(uint64_t(1)<<44).count==17);
 refused([&]{p.segment(1);});refused([&]{p.segment(count);});
 auto bad=a;bad.assignmentId="../escape";refused([&]{CoordinatorPlan q(bad,71,a.target,1024);});
 bad=a;bad.target="other";refused([&]{CoordinatorPlan q(bad,71,a.target,1024);});
 bad=a;bad.searchMode="kangaroo";refused([&]{CoordinatorPlan q(bad,71,a.target,1024);});
 bad=a;bad.requiredBackend="opencl";refused([&]{CoordinatorPlan q(bad,71,a.target,1024);});
 refused([&]{CoordinatorPlan q(a,20,a.target,1024);});refused([&]{CoordinatorPlan q(a,71,a.target,1024,17);});
 bad=a;bad.start="10000000000000000";bad.end=add(bad.start,UINT64_MAX);refused([&]{CoordinatorPlan q(bad,71,a.target,1024);});
 bad=a;bad.end=bad.start;CoordinatorPlan one(bad,71,a.target,1024);assert(one.total==1&&one.segment(0).count==1);
 std::cout<<"VanityCoordinatorPlanTests passed: normal 60-minute sized assignment, exact contiguous segments, tail, overflow and metadata rejection.\n";
}
