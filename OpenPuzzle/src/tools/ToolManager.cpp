#include "openpuzzle/tools/ToolManager.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
namespace fs = std::filesystem;
namespace openpuzzle {
std::string ToolManager::configPath() {
  const char *home = getenv("HOME");
  fs::path p = home ? fs::path(home) : fs::current_path();
  p /= ".config/OpenPuzzle/config.json";
  return p.string();
}
bool ToolManager::configureBitCrack(const std::string &path) {
  fs::path cfg = configPath();
  fs::create_directories(cfg.parent_path());
  std::ofstream out(cfg);
  if (!out)
    return false;
  out << "{\n  \"bitcrack\": \"" << path << "\",\n  \"gpu_device\": 0\n}\n";
  return true;
}

bool ToolManager::configureBitCrack(const std::string &cudaPath,
                                    const std::string &openclPath) {
  fs::path cfg = configPath();
  fs::create_directories(cfg.parent_path());

  std::ofstream out(cfg);

  if (!out)
    return false;

  out << "{\n";
  out << "  \"bitcrack\": {\n";
  out << "    \"cuda\": \"" << cudaPath << "\",\n";
  out << "    \"opencl\": \"" << openclPath << "\"\n";
  out << "  },\n";
  out << "  \"gpu_device\": 0\n";
  out << "}\n";

  return true;
}

static std::optional<std::string> readJsonStringAfterKey(const std::string &text,
                                                         const std::string &key) {
  auto k = text.find("\"" + key + "\"");

  if (k == std::string::npos)
    return std::nullopt;

  auto c = text.find(':', k);
  auto f = text.find('"', c);
  auto e = text.find('"', f + 1);

  if (c == std::string::npos || f == std::string::npos || e == std::string::npos)
    return std::nullopt;

  return text.substr(f + 1, e - f - 1);
}

static std::string readConfigText() {
  std::ifstream in(ToolManager::configPath());

  if (!in)
    return {};

  std::stringstream buffer;
  buffer << in.rdbuf();

  return buffer.str();
}
std::optional<std::string> ToolManager::bitcrackPath() {
  return bitcrackCudaPath();
}

std::optional<std::string> ToolManager::bitcrackCudaPath() {
  const auto text = readConfigText();

  if (text.empty())
    return std::nullopt;

  auto cudaPath = readJsonStringAfterKey(text, "cuda");

  if (cudaPath)
    return cudaPath;

  return readJsonStringAfterKey(text, "bitcrack");
}

std::optional<std::string> ToolManager::bitcrackOpenCLPath() {
  const auto text = readConfigText();

  if (text.empty())
    return std::nullopt;

  auto openclPath = readJsonStringAfterKey(text, "opencl");

  if (openclPath)
    return openclPath;

  auto cudaPath = bitcrackCudaPath();

  if (!cudaPath)
    return std::nullopt;

  fs::path cudaExecutable(*cudaPath);
  auto inferredOpenCL = cudaExecutable.parent_path() / "clBitCrack";

  return inferredOpenCL.string();
}
} // namespace openpuzzle
