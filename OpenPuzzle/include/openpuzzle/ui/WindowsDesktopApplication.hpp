#pragma once
#ifdef _WIN32
#include <filesystem>
#include "openpuzzle/hardware/WindowsTelemetryModel.hpp"
#include <functional>
#include <ostream>
#include <string>
#include <vector>
namespace openpuzzle::ui {
// CLI adapter for the shared Linux/Windows Qt panel. All searches still use
// the existing Windows providers, measured profiles and protected receipts.
class WindowsDesktopApplication {
public:
 using Command=std::function<int(const std::vector<std::string>&,std::ostream&)>;
 explicit WindowsDesktopApplication(Command command={},std::filesystem::path profileRoot={},std::filesystem::path legacyRoot={},WindowsRuntimeThermalSettings::Reader sensors={});
 int run(const std::vector<std::string>& args,std::ostream& output);
private:
 Command command_;
 WindowsRuntimeThermalSettings::Reader sensors_;
 std::filesystem::path root_,legacy_;
 std::filesystem::path profile(const std::string& backend,int device,int threads)const;
 std::filesystem::path publish(const std::filesystem::path& source);
 int benchmark(const std::vector<std::string>& args,std::ostream& output);
};
}
#endif
