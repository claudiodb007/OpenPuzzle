#include "openpuzzle/runtime/WindowsRunProvider.hpp"
#include "openpuzzle/engines/WindowsRunProfile.hpp"
#include "openpuzzle/engines/WindowsEngineDiagnostics.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/services/PuzzleMetadataCatalog.hpp"
namespace openpuzzle {
namespace fs=std::filesystem;
WindowsPreparedRun prepareWindowsRun(RunCommandOptions options,const fs::path& engineFolder) {
 if(options.profile.empty()||options.server.empty())throw std::invalid_argument("Run requires --profile result.json and an explicit --server URL");
 const auto profilePath=pathFromUtf8(options.profile);
 if(!profilePath.is_absolute())throw std::invalid_argument("Use an absolute benchmark profile path");
 bool missing=false;const auto contents=windowsFile::readPrivate(profilePath,missing,16384);
 if(!contents)throw std::runtime_error("Benchmark profile is missing or unreadable; preserved");
 const auto profile=WindowsRunProfile::parse(*contents);options=profile.apply(std::move(options));
 const auto folder=engineFolder.empty()?WindowsEngineDiagnostics::defaultFolder():engineFolder;
 const auto exe=folder/(profile.backend=="cpu"?"keyhunt.exe":profile.backend=="cuda"?"cuBitCrack.exe":"clBitCrack.exe");
 if(WindowsEngineDiagnostics::executableSha256(exe)!=profile.sha256)throw std::runtime_error("Engine differs from the benchmark; run local validation again");
 SYSTEM_INFO system{};GetNativeSystemInfo(&system);const auto count=GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
 if(system.wProcessorArchitecture!=PROCESSOR_ARCHITECTURE_AMD64||!count||count>INT_MAX)throw std::runtime_error("Native CPU inventory is unavailable");
 WindowsRunDependencies d;d.serverUrl=options.server;
 d.engines={{options.engine,profile.backend,exe,profile.backend=="cpu"?std::vector<int>{}:std::vector<int>{profile.device},profile.speed,true}};
 d.preflight=[profile,exe](const RunCommandOptions& o,const WindowsRunEngine& e){try{
   const auto bound=profile.apply(o);
   if(e.executable!=exe||e.speedMKeys!=profile.speed||e.backend!=profile.backend||!e.verified||
      WindowsEngineDiagnostics::executableSha256(exe)!=profile.sha256)return false;
   return WindowsEngineDiagnostics::inspectDevice(exe,profile.backend,bound.device,bound.threads)==profile.deviceIdentity;
  }catch(...){return false;}};
 d.capabilities=[profile,count](const std::string& id){client::ClientHeartbeat h;
   h.clientId=id;h.version="1.0.46";h.platform="windows";h.cpu={"Windows native x64",static_cast<int>(count),static_cast<int>(count)};
   h.engines={{profile.backend=="cpu"?"KeyHunt":"BitCrack",profile.backend,true,true}};
   if(profile.backend!="cpu"){
    const auto separator=profile.deviceIdentity.find(';');const auto at=profile.deviceIdentity.rfind("memory=");
    std::uint64_t bytes=0;if(at!=std::string::npos){const auto value=profile.deviceIdentity.substr(at+7);const auto parsed=std::from_chars(value.data(),value.data()+value.size(),bytes);if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size())bytes=0;}
    h.gpus={{profile.backend,profile.deviceIdentity.substr(0,separator),bytes/1048576}};
   }return h;};
 d.metadata=[folder](int number)->std::optional<PuzzleExecutionMetadata>{
   bool missing=false;const auto text=windowsFile::readPrivate(folder.parent_path()/"puzzles"/(std::to_string(number)+".json"),missing,16384);
   if(!text)return {};return PuzzleMetadataCatalog::parse(number,*text);};
 return {std::move(options),std::move(d)};
}
}
