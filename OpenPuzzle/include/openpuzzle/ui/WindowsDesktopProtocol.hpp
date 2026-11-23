#pragma once
#include "openpuzzle/runtime/ExecutionSlot.hpp"
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace openpuzzle::ui {
struct WindowsDesktopSlot {std::string slot,status;std::map<std::string,std::string> fields;};
inline std::vector<WindowsDesktopSlot> windowsDesktopSlots(const std::string& text,int code){
 if((code!=0&&code!=2)||text.empty()||text.size()>262144)throw std::runtime_error("Windows status unavailable; no idle fallback");
 std::vector<WindowsDesktopSlot> result;std::set<std::string> seen;std::istringstream in(text);std::string line;WindowsDesktopSlot current;
 auto finish=[&]{if(current.fields.empty())return;
  auto& f=current.fields;current.slot=f["Slot"];
  if(!ExecutionSlot::valid(current.slot)||!seen.insert(current.slot).second||!f.count("Runtime")||!f.count("Assignment")||!f.count("Pending claim"))throw std::runtime_error("Incomplete Windows status");
  if(f["Runtime"]!="idle"&&f["Runtime"]!="running"&&f["Runtime"]!="unknown")throw std::runtime_error("Unknown Windows runtime state");
  if(f["Assignment"].empty()||(f["Pending claim"]!="none"&&f["Pending claim"]!="preserved"))throw std::runtime_error("Invalid Windows assignment state");
  current.status=f["Runtime"]=="unknown"||f["Assignment"]=="identity unavailable"||f["Assignment"]=="unreadable state preserved"?"identity unavailable":
   f["Runtime"]=="running"?"running":f["Assignment"]!="none"||f["Pending claim"]!="none"?"stopped":"idle";
  result.push_back(std::move(current));current={};};
 while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;
  const auto at=line.find(": ");if(at==std::string::npos)throw std::runtime_error("Unrecognized Windows status line");
  auto key=line.substr(0,at),value=line.substr(at+2);if(key=="Slot"&&!current.fields.empty())finish();
  if(!current.fields.emplace(key,value).second)throw std::runtime_error("Duplicated Windows status field");
 }finish();if(result.empty()||result.size()>256)throw std::runtime_error("Empty Windows status");return result;
}
inline std::string windowsDesktopStatus(const std::vector<WindowsDesktopSlot>& slots,bool profileReady){
 std::ostringstream out;
 for(const auto& s:slots){out<<"Slot............... "<<s.slot<<"\nStatus............. "<<s.status<<'\n';
  const auto field=[&](const char* source,const char* target){const auto p=s.fields.find(source);if(p!=s.fields.end()&&!p->second.empty()&&p->second!="none"&&p->second!="unavailable")out<<target<<".......... "<<p->second<<'\n';};
  field("Assignment","Assignment");field("Engine","Engine");field("Backend","Backend");field("Puzzle","Puzzle");field("Range","Assignment number");
  field("Temperature","Temperature");field("Power","Power");
  field("Speed","Speed");field("Keys checked","Keys checked");field("Workspace","Workspace");field("Recovery","Recovery available");
  field("Device","Device");field("Checkpoint","Checkpoint");
  if(s.status=="stopped")out<<"Synchronization.... pending supervisor\n";out<<'\n';
 }out<<"Desktop profile.... "<<(profileReady?"ready":"missing")<<'\n';return out.str();
}
}
