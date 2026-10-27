// MIT. Native durable operation intent and exact receipt lookup; loopback only.
#pragma once
#include "ReceiptJson.hpp"
#include "VanityPreviewEngine.hpp"
#include <set>
#include "LoopbackHttp.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <regex>
#include <sstream>
#include <vector>
#include <sys/random.h>
#include <sys/stat.h>
namespace openpuzzle::vanityPreview::receipts {
inline constexpr const char* protocol="openpuzzle_vanity_receipts_v1";
class Client {
 using fs=std::filesystem::path;
 fs path_;LoopbackHttp http_;std::string server_,client_,binding_,scopeHash_;Json scope_;
 std::function<std::string(const std::string&)> hash_;
 std::vector<Json> entries_;bool blocked_=false;
 static uint64_t decimal(const Json& value){
  auto s=value.text();require(std::regex_match(s,std::regex("0|[1-9][0-9]{0,19}")),"Invalid receipt counter");
  size_t used=0;auto n=std::stoull(s,&used);require(used==s.size(),"Invalid receipt count");return n;
 }
 static bool uuid(const std::string& s){return std::regex_match(s,std::regex("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"));}
 static std::string newId(){
  unsigned char data[16];size_t n=0;
  while(n<sizeof data){auto count=getrandom(data+n,sizeof data-n,0);if(count<0&&errno==EINTR)continue;require(count>0,"Operation UUID entropy unavailable");n+=size_t(count);}
  data[6]=(data[6]&15)|64;data[8]=(data[8]&63)|128;
  std::ostringstream out;out<<std::hex<<std::setfill('0');for(unsigned i=0;i<16;++i){if(i==4||i==6||i==8||i==10)out<<'-';out<<std::setw(2)<<unsigned(data[i]);}return out.str();
 }
 void privateFile() const {
  struct stat st{};require(lstat(path_.c_str(),&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==getuid()&&(st.st_mode&0077)==0&&st.st_size<=1024*1024,"Unsafe native receipt journal");
 }
 Json journal() const {
  Json rows=Json::obj({});for(size_t i=0;i<entries_.size();++i)rows.object.emplace(std::to_string(i),entries_[i]);
  return Json::obj({{"schema","openpuzzle_native_receipts_v1"},{"protocol",protocol},{"binding",binding_},
   {"server",server_},{"client_id",client_},{"scope",scope_},{"entries",rows}});
 }
 void save() const {
  auto data=journal().dump()+"\n";require(data.size()<=1024*1024,"Native receipt journal requires review: size limit");
  require(!std::filesystem::is_symlink(std::filesystem::symlink_status(path_)),"Symlink receipt journal preserved");
  require(writePrivateFileAtomically(path_,data),"Native receipt journal not published");
  int fd=open(path_.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);bool ok=fd>=0&&fsync(fd)==0;if(fd>=0)close(fd);require(ok,"Receipt directory sync failed");
 }
 void validateRequest(const Json& q) const {
  auto kind=q.at("kind").text();require(kind=="claim"||kind=="progress"||kind=="complete","Unknown receipt operation");
  if(kind=="claim")q.keys({"protocol","operation_id","client_id","kind","scope_sha256","puzzle"});
  else q.keys({"protocol","operation_id","client_id","kind","scope_sha256","assignment_id","lease_epoch","keys_checked"});
  require(q.at("protocol").text()==protocol&&q.at("client_id").text()==client_&&q.at("scope_sha256").text()==scopeHash_
   &&uuid(q.at("operation_id").text()),"Native receipt request binding differs");
  if(kind=="claim")require(q.at("puzzle").integer()==20,"Receipt puzzle differs");
  else require(uuid(q.at("assignment_id").text())&&q.at("lease_epoch").integer()>0&&decimal(q.at("keys_checked"))<=total(),"Native receipt assignment/count differs");
 }
 void validateReceipt(const Json& q,const Json& r) const {
  r.keys({"protocol","operation_id","request_sha256","client_id","kind","outcome","reason","assignment","production_enabled"});
  for(auto key:{"protocol","operation_id","client_id","kind"})require(r.at(key)==q.at(key),"Receipt request identity differs");
  require(r.at("request_sha256").text()==hash_(q.dump())&&r.at("production_enabled").isFalse(),"Receipt digest/production gate differs");
  auto outcome=r.at("outcome").text();require(outcome=="applied"||outcome=="refused","Unknown receipt outcome");
  if(outcome=="refused"){
   auto reason=r.at("reason").text();require(reason=="assignment_unavailable"||reason=="assignment_not_found"||reason=="lease_refused"||reason=="coverage_refused","Unknown receipt refusal");return;
  }
  require(r.at("reason").text()=="applied","Applied receipt reason differs");auto a=r.at("assignment");
  a.keys({"assignment_id","client_id","lease_epoch","lease_until","keys_checked","status","total","puzzle","scope"});
  require(a.at("client_id").text()==client_&&a.at("scope")==scope_&&a.at("puzzle").integer()==20
   &&uuid(a.at("assignment_id").text())&&a.at("lease_epoch").integer()>0&&a.at("lease_until").integer()>0
   &&decimal(a.at("total"))==total()&&decimal(a.at("keys_checked"))<=total(),"Receipt assignment scope differs");
  auto kind=q.at("kind").text();
  if(kind=="claim")require(a.at("status").text()=="assigned"&&decimal(a.at("keys_checked"))==0,"Claim receipt state differs");
  else {
   require(a.at("assignment_id")==q.at("assignment_id")&&a.at("lease_epoch")==q.at("lease_epoch")
    &&a.at("keys_checked")==q.at("keys_checked")&&a.at("status").text()==(kind=="complete"?"completed":"assigned"),"Receipt operation assignment/count differs");
   if(kind=="complete")require(decimal(a.at("keys_checked"))==total(),"Incomplete completion receipt");
  }
 }
 uint64_t total() const {return inclusiveCount(scope_.at("start").text(),scope_.at("end").text());}
 void validateHistory() const {
  bool claim=false,complete=false;uint64_t checked=0;Json assignment;
  std::set<std::string> ids;
  for(size_t i=0;i<entries_.size();++i){
   const auto& row=entries_[i];row.keys({"request","request_sha256","receipt"});auto q=row.at("request");validateRequest(q);
   require(ids.insert(q.at("operation_id").text()).second&&row.at("request_sha256").text()==hash_(q.dump()),"Changed/duplicate native operation intent");
   auto kind=q.at("kind").text();require(!complete,"Operation follows completion");
   if(kind=="claim"){require(i==0&&!claim,"Another receipt claim is forbidden");claim=true;}
   else {
    require(claim&&assignment.kind==Json::Object&&q.at("assignment_id")==assignment.at("assignment_id")
     &&q.at("lease_epoch")==assignment.at("lease_epoch"),"Receipt history assignment differs");
    auto n=decimal(q.at("keys_checked"));require(n>=checked,"Receipt history regressed");checked=n;
    if(kind=="complete")require(n==total(),"Incomplete completion intent");
   }
   const auto& r=row.at("receipt");
   if(r.kind==Json::Null)require(i+1==entries_.size(),"Operation follows unresolved intent");
   else {
    validateReceipt(q,r);
    if(r.at("outcome").text()=="refused")require(i+1==entries_.size(),"Operation follows refused receipt");
    else if(kind=="claim")assignment=r.at("assignment");
   }
   complete=kind=="complete";
  }
 }
 Json claimAssignment() const {
  require(!entries_.empty(),"Missing receipt claim");auto r=entries_.front().at("receipt");
  require(r.kind==Json::Object&&r.at("outcome").text()=="applied","Claim is not confirmed");return r.at("assignment");
 }
 void recover(){
  require(!entries_.empty(),"No durable operation to reconcile; review required");
  auto& last=entries_.back();auto q=last.at("request");
  auto response=http_.post("/api/vanity/v1/receipts/query",q.dump());require(response.status==200,"Receipt lookup unavailable; intent preserved");
  auto v=Json::parse(response.body);v.keys({"protocol","operation_id","request_sha256","client_id","state","receipt","production_enabled"});
  for(auto key:{"protocol","operation_id","client_id"})require(v.at(key)==q.at(key),"Receipt lookup identity differs");
  require(v.at("request_sha256").text()==hash_(q.dump())&&v.at("production_enabled").isFalse(),"Receipt lookup digest/gate differs");
  require(v.at("state").text()=="recorded","Receipt unknown: review required, no operation replay");
  auto r=v.at("receipt");validateReceipt(q,r);
  require(last.at("receipt").kind==Json::Null||last.at("receipt")==r,"Recorded receipt changed on lookup");
  last.object["receipt"]=r;save();validateHistory();
  require(r.at("outcome").text()=="applied","Receipt refused: no new operation or kernel authorized");
 }
 public:
 Client(const fs& root,const std::string& server,const std::string& client,Json scope,const std::string& binding,
        std::function<std::string(const std::string&)> hash,bool resume,bool create):
  path_(root/"receipt-client.json"),http_(server),server_(server),client_(client),binding_(binding),scope_(std::move(scope)),hash_(std::move(hash)){
  scope_.keys({"puzzle","range_id","start","end","target","required_backend"});
  require(scope_.at("puzzle").integer()==20&&scope_.at("range_id").integer()==37&&scope_.at("required_backend").text()=="cuda"&&total()>0&&total()<=(1ULL<<36),"Isolated receipt scope required");
  scopeHash_=hash_(scope_.dump());
  if(std::filesystem::exists(path_)||std::filesystem::is_symlink(std::filesystem::symlink_status(path_))){
   require(resume&&!create,"Explicit receipt resume required");privateFile();std::ifstream file(path_);std::string text((std::istreambuf_iterator<char>(file)),{});auto v=Json::parse(text);
   v.keys({"schema","protocol","binding","server","client_id","scope","entries"});
   require(v.at("schema").text()=="openpuzzle_native_receipts_v1"&&v.at("protocol").text()==protocol
    &&v.at("binding").text()==binding_&&v.at("server").text()==server_&&v.at("client_id").text()==client_&&v.at("scope")==scope_,"Native receipt journal binding differs");
   auto rows=v.at("entries");require(rows.kind==Json::Object&&rows.object.size()<=4096,"Invalid receipt journal entries");
   for(size_t i=0;i<rows.object.size();++i)entries_.push_back(rows.at(std::to_string(i)));
   validateHistory();recover();
  }else {require(create&&!resume,"Native receipt journal missing; review required");save();}
 }
 LocalHttpResponse operation(const std::string& kind,const std::string& assignment,uint64_t checked){
  require(!blocked_,"Unresolved receipt intent: explicit resume required");
  try {
   // Claim/complete can be adopted only from their exact recovered receipt.
   if(!entries_.empty()){
    const auto& last=entries_.back();auto old=last.at("request");
    if((kind=="claim"||kind=="complete")&&old.at("kind").text()==kind){
     require(last.at("receipt").kind==Json::Object,"Pending receipt requires explicit resume");
     if(kind=="complete")require(old.at("assignment_id").text()==assignment&&decimal(old.at("keys_checked"))==checked,"Recovered completion differs");
     return translate(old,last.at("receipt"));
    }
    require(last.at("receipt").kind==Json::Object&&last.at("receipt").at("outcome").text()=="applied"
     &&old.at("kind").text()!="complete","Unresolved/refused/complete receipt blocks new operation");
   }
   Json q=Json::obj({{"protocol",protocol},{"operation_id",newId()},{"client_id",client_},{"kind",kind},{"scope_sha256",scopeHash_}});
   if(kind=="claim"){require(entries_.empty(),"No second receipt claim allowed");q.object["puzzle"]=Json(uint64_t(20));}
   else {auto a=claimAssignment();require(a.at("assignment_id").text()==assignment,"Native assignment differs from claim receipt");
    q.object["assignment_id"]=assignment;q.object["lease_epoch"]=a.at("lease_epoch");q.object["keys_checked"]=std::to_string(checked);}
   validateRequest(q);require(entries_.size()<4096,"Receipt journal operation limit reached");
   entries_.push_back(Json::obj({{"request",q},{"request_sha256",hash_(q.dump())},{"receipt",Json()}}));
   validateHistory();save(); // Must complete before the first byte can be sent.
   auto response=http_.post("/api/vanity/v1/operations",q.dump());require(response.status==200,"Unconfirmed receipt response; intent preserved");
   auto receipt=Json::parse(response.body);validateReceipt(q,receipt);
   entries_.back().object["receipt"]=receipt;save();validateHistory();return translate(q,receipt);
  }catch(...){blocked_=true;throw;}
 }
 LocalHttpResponse translate(const Json& q,const Json& receipt) const {
  if(receipt.at("outcome").text()=="refused")return {409,"{\"error\":\"assignment_lease_expired\"}"};
  if(q.at("kind").text()!="claim")return {200,"{\"success\":true}"};
  auto a=receipt.at("assignment");return {200,Json::obj({{"assignment_id",a.at("assignment_id")},{"puzzle",Json(uint64_t(20))},
   {"range_id",scope_.at("range_id")},{"target",scope_.at("target")},{"search_mode","linear"},
   {"required_backend","cuda"},{"start",scope_.at("start")},{"end",scope_.at("end")}}).dump()};
 }
};
}
