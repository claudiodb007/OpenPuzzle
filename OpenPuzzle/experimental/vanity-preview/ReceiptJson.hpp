// MIT. Bounded typed JSON for the isolated receipt protocol, not a general parser.
#pragma once
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
namespace openpuzzle::vanityPreview::receipts {
inline void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Json {
 enum Kind {Null,Boolean,Number,String,Object} kind=Null;
 bool boolean=false;uint64_t number=0;std::string string;std::map<std::string,Json> object;
 Json()=default;Json(bool v):kind(Boolean),boolean(v){};Json(uint64_t v):kind(Number),number(v){}
 Json(const char* v):kind(String),string(v){};Json(std::string v):kind(String),string(std::move(v)){}
 Json(std::map<std::string,Json> v):kind(Object),object(std::move(v)){}
 static Json obj(std::initializer_list<std::pair<const std::string,Json>> v){return Json(std::map<std::string,Json>(v));}
 const Json& at(const std::string& key) const {require(kind==Object,"JSON object required");return object.at(key);}
 std::string text() const {require(kind==String,"JSON string required");return string;}
 uint64_t integer() const {require(kind==Number,"JSON unsigned integer required");return number;}
 bool isFalse() const {return kind==Boolean&&!boolean;}
 void keys(std::initializer_list<const char*> names) const {
  require(kind==Object&&object.size()==names.size(),"JSON fields differ");for(auto name:names)require(object.count(name),"JSON field missing");
 }
 bool operator==(const Json&) const =default;
 static std::string quote(const std::string& value){
  std::string out="\"";const char* hex="0123456789abcdef";
  for(unsigned char c:value){require(c<128,"Receipt strings must be ASCII");
   if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";
   else if(c=='\t')out+="\\t";else if(c=='\b')out+="\\b";else if(c=='\f')out+="\\f";
   else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);
  }return out+'"';
 }
 std::string dump() const {
  if(kind==Null)return "null";if(kind==Boolean)return boolean?"true":"false";if(kind==Number)return std::to_string(number);if(kind==String)return quote(string);
  std::string out="{";for(const auto& [k,v]:object){if(out.size()>1)out+=',';out+=quote(k)+':'+v.dump();}return out+'}';
 }
 static Json parse(const std::string& source){
  require(source.size()<=1024*1024,"Oversized receipt JSON");
  struct Parser {
   const std::string& s;size_t i=0;
   void ws(){while(i<s.size()&&(s[i]==' '||s[i]=='\r'||s[i]=='\n'||s[i]=='\t'))++i;}
   bool take(char c){ws();if(i<s.size()&&s[i]==c){++i;return true;}return false;}
   std::string str(){
    require(take('"'),"JSON quote required");std::string out;
    while(i<s.size()){
     unsigned char c=s[i++];if(c=='"')return out;require(c>=32&&c<128,"Invalid receipt string");
     if(c=='\\'){
      require(i<s.size(),"Truncated JSON escape");c=s[i++];
      if(c=='"'||c=='\\'||c=='/')out+=char(c);
      else if(c=='b')out+='\b';else if(c=='f')out+='\f';else if(c=='n')out+='\n';else if(c=='r')out+='\r';else if(c=='t')out+='\t';
      else if(c=='u'){
       require(s.size()-i>=4,"Truncated unicode escape");unsigned value=0;
       for(int n=0;n<4;++n){unsigned char h=s[i++];unsigned digit=h>='0'&&h<='9'?h-'0':h>='a'&&h<='f'?h-'a'+10:h>='A'&&h<='F'?h-'A'+10:99;require(digit<16,"Invalid unicode escape");value=value*16+digit;}
       require(value<128,"Non-ASCII receipt escape");out+=char(value);
      }else throw std::runtime_error("Unsupported JSON escape");
     }else out+=char(c);
    }throw std::runtime_error("Unterminated JSON string");
   }
   Json value(unsigned depth=0){
    require(depth<=16,"Receipt nesting exceeds bound");ws();require(i<s.size(),"Truncated JSON");
    if(s[i]=='"')return Json(str());
    if(take('{')){
     Json v=Json::obj({});if(take('}'))return v;
     do {auto key=str();require(take(':'),"JSON colon required");require(v.object.emplace(key,value(depth+1)).second,"Duplicate JSON key");}while(take(','));
     require(take('}'),"JSON object not closed");return v;
    }
    for(auto word:{std::string_view("null"),std::string_view("true"),std::string_view("false")}){
     if(s.compare(i,word.size(),word)==0){i+=word.size();if(word=="null")return {};return Json(word=="true");}
    }
    require(s[i]>='0'&&s[i]<='9',"Unsigned integer or object receipt value required");uint64_t n=0;size_t start=i;
    while(i<s.size()&&s[i]>='0'&&s[i]<='9'){
     unsigned digit=s[i++]-'0';require(n<=(std::numeric_limits<uint64_t>::max()-digit)/10,"JSON integer overflow");n=n*10+digit;
    }
    require(i-start==1||s[start]!='0',"Noncanonical JSON integer");return Json(n);
   }
  } p{source};auto v=p.value();p.ws();require(p.i==source.size(),"Trailing JSON data");return v;
 }
};
}
