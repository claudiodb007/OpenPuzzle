#pragma once
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace openpuzzle {
// Microsoft C/C++ argv quoting for direct CreateProcessW launches. Never use
// this encoding as a cmd.exe or PowerShell command: shell metacharacters remain literal.
struct WindowsCommandLine {
  static std::string quote(std::string_view value) {
    if (value.find('\0') != std::string_view::npos) throw std::invalid_argument("NUL in Windows argument");
    std::string out="\"";
    std::size_t slashes=0;
    for (char c:value) {
      if (c=='\\') { ++slashes; continue; }
      if (c=='\"') out.append(slashes*2+1,'\\');
      else out.append(slashes,'\\');
      slashes=0; out+=c;
    }
    out.append(slashes*2,'\\'); out+='\"'; return out;
  }
  static std::string build(const std::string& executable,const std::vector<std::string>& arguments) {
    if(executable.empty() || executable.find('\"')!=std::string::npos) throw std::invalid_argument("Invalid Windows executable path");
    std::string result=quote(executable);
    for(const auto& arg:arguments){result+=' ';result+=quote(arg);}
    return result;
  }
};
}
