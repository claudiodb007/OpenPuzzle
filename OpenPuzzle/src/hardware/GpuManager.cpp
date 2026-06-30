#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/tools/ToolManager.hpp"
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
namespace fs = std::filesystem;
namespace openpuzzle {
static std::string run(const char *cmd) {
  std::array<char, 256> buf{};
  std::string r;
  FILE *p = popen(cmd, "r");
  if (!p)
    return r;
  while (fgets(buf.data(), buf.size(), p))
    r += buf.data();
  pclose(p);
  return r;
}
std::vector<GpuInfo> GpuManager::listGpus() {
  std::vector<GpuInfo> v;
  auto out = run("nvidia-smi --query-gpu=index,name,uuid,memory.total "
                 "--format=csv,noheader 2>/dev/null");
  std::stringstream ss(out);
  std::string line;
  auto trim = [](std::string s) {
    while (!s.empty() && s.front() == ' ')
      s.erase(s.begin());
    while (!s.empty() &&
           (s.back() == ' ' || s.back() == '\n' || s.back() == '\r'))
      s.pop_back();
    return s;
  };
  while (std::getline(ss, line)) {
    if (line.empty())
      continue;
    std::stringstream ls(line);
    std::string idx, name, uuid, mem;
    std::getline(ls, idx, ',');
    std::getline(ls, name, ',');
    std::getline(ls, uuid, ',');
    std::getline(ls, mem, ',');
    GpuInfo g;
    g.device = std::stoi(trim(idx));
    g.name = trim(name);
    g.uuid = trim(uuid);
    g.memory = trim(mem);
    v.push_back(g);
  }
  return v;
}
bool GpuManager::selectGpu(int device) {
  fs::path cfg = ToolManager::configPath();
  fs::create_directories(cfg.parent_path());
  std::string bit;
  if (auto p = ToolManager::bitcrackPath())
    bit = *p;
  std::ofstream out(cfg);
  if (!out)
    return false;
  out << "{\n  \"bitcrack\": \"" << bit << "\",\n  \"gpu_device\": " << device
      << "\n}\n";
  return true;
}
int GpuManager::selectedGpu() {
  std::ifstream in(ToolManager::configPath());
  if (!in)
    return 0;
  std::stringstream b;
  b << in.rdbuf();
  std::string t = b.str();
  auto k = t.find("\"gpu_device\"");
  if (k == std::string::npos)
    return 0;
  auto c = t.find(':', k);
  return std::stoi(t.substr(c + 1));
}
} // namespace openpuzzle
