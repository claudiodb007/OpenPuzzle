#include "openpuzzle/runtime/WindowsRuntimeRecord.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
using openpuzzle::WindowsRuntimeRecord;
void require(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
int main() {
  try {
    const WindowsRuntimeRecord original{123,133700001234567890ULL};
    const auto decoded=WindowsRuntimeRecord::parse(original.encode());
    require(decoded && *decoded==original,"Windows record round trip failed");
    require(WindowsRuntimeRecord::parse("123\r\nwindows-filetime-v1\r\n133700001234567890\r\n")==decoded,"CRLF record changed");
    require(WindowsRuntimeRecord::parse("123\nwindows-filetime-v1\n133700001234567890")==decoded,"Final LF must be optional");
    require(WindowsRuntimeRecord::parse("2147483647\nwindows-filetime-v1\n18446744073709551615\n").has_value(),"Supported integer boundary rejected");
    for(const auto& text:std::vector<std::string>{
      "", "123", "123\n", "123\nwindows-filetime-v1\n", "0\nwindows-filetime-v1\n1\n",
      "-1\nwindows-filetime-v1\n1\n", "+1\nwindows-filetime-v1\n1\n", " 1\nwindows-filetime-v1\n1\n",
      "123\nlinux-boot-id\n100\n", "123\nwindows-filetime-v2\n100\n", "123\nwindows-filetime-v1\n0\n",
      "123\nwindows-filetime-v1\n-1\n", "123\nwindows-filetime-v1\n1x\n", "123\nwindows-filetime-v1\n1\nextra",
      "123\nwindows-filetime-v1\n1\n\n", "2147483648\nwindows-filetime-v1\n1\n",
      "1\nwindows-filetime-v1\n18446744073709551616\n", std::string(129,'1')}) {
      require(!WindowsRuntimeRecord::parse(text),"Malformed/foreign record accepted");
    }
    require(WindowsRuntimeRecord{}.encode().empty(),"Invalid identity encoded");
    std::cout<<"WindowsRuntimeRecordTests passed: round trips, boundaries, truncation, foreign platform and trailing data\n";
    return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
