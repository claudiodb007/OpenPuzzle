#pragma once
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
namespace openpuzzle::ui {
struct WindowsUiProgress {
 std::optional<double> speedMKeys;
 std::string keysChecked;
 static bool valid(double speed,const std::string& keys){
  return std::isfinite(speed)&&speed>0&&speed<=1e15&&!keys.empty()&&keys.size()<=128&&
   std::all_of(keys.begin(),keys.end(),[](char c){return c>='0'&&c<='9';});
 }
 static WindowsUiProgress measured(double speed,const std::string& keys){
  return valid(speed,keys)?WindowsUiProgress{speed,keys}:WindowsUiProgress{};
 }
 static WindowsUiProgress parse(const std::string& speed,const std::string& keys){
  double value=0;auto parsed=std::from_chars(speed.data(),speed.data()+speed.size(),value);
  return parsed.ec==std::errc{}&&parsed.ptr==speed.data()+speed.size()?measured(value,keys):WindowsUiProgress{};
 }
 std::string display()const{
  if(!speedMKeys)return "—";std::ostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(2)<<*speedMKeys<<" MKey/s";return out.str();
 }
 std::string protocol()const{
  std::ostringstream out;out.imbue(std::locale::classic());out<<"Speed MKey/s: ";
  if(speedMKeys)out<<std::setprecision(12)<<*speedMKeys;else out<<"unavailable";
  out<<"\nKeys checked: "<<(speedMKeys?keysChecked:"unavailable")<<'\n';return out.str();
 }
};
}
