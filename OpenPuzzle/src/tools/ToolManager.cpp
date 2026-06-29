#include "openpuzzle/tools/ToolManager.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
namespace fs=std::filesystem; namespace openpuzzle {
std::string ToolManager::configPath(){ const char* home=getenv("HOME"); fs::path p=home?fs::path(home):fs::current_path(); p/=".config/OpenPuzzle/config.json"; return p.string(); }
bool ToolManager::configureBitCrack(const std::string& path){ fs::path cfg=configPath(); fs::create_directories(cfg.parent_path()); std::ofstream out(cfg); if(!out) return false; out<<"{\n  \"bitcrack\": \""<<path<<"\",\n  \"gpu_device\": 0\n}\n"; return true; }
std::optional<std::string> ToolManager::bitcrackPath(){ std::ifstream in(configPath()); if(!in) return std::nullopt; std::stringstream b; b<<in.rdbuf(); std::string t=b.str(); auto k=t.find("\"bitcrack\""); if(k==std::string::npos) return std::nullopt; auto c=t.find(':',k); auto f=t.find('"',c); auto s=t.find('"',f+1); if(f==std::string::npos||s==std::string::npos) return std::nullopt; return t.substr(f+1,s-f-1); }
}
