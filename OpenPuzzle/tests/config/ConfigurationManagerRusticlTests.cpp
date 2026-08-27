#include "openpuzzle/config/ConfigurationManager.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
namespace fs = std::filesystem;
int main() {
  const char* old = std::getenv("HOME");
  const std::string saved = old ? old : "";
  const fs::path root = fs::temp_directory_path() /
      ("openpuzzle-rusticl-config-" + std::to_string((long long)getpid()));
  fs::remove_all(root);
  fs::create_directories(root);
  assert(setenv("HOME", root.c_str(), 1) == 0);
  openpuzzle::Configuration c;
  c.engine.id="bitcrack";
  c.engine.backend="opencl";
  c.gpu.device=1;
  c.gpu.rusticlEnable="radeonsi";
  assert(openpuzzle::ConfigurationManager::save(c));
  const auto r=openpuzzle::ConfigurationManager::load();
  assert(r.engine.backend=="opencl");
  assert(r.gpu.device==1);
  assert(r.gpu.rusticlEnable=="radeonsi");
  std::ifstream in(openpuzzle::ConfigurationManager::configPath());
  std::stringstream text; text << in.rdbuf();
  assert(text.str().find("\"rusticl_enable\": \"radeonsi\"") != std::string::npos);
  if (old) assert(setenv("HOME", saved.c_str(), 1) == 0);
  else assert(unsetenv("HOME") == 0);
  fs::remove_all(root);
  return 0;
}
