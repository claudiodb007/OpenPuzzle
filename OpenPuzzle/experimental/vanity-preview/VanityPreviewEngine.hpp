#pragma once
#include "openpuzzle/engines/ISearchEngine.hpp"
#include "openpuzzle/engines/EngineOutputParser.hpp"
#include "openpuzzle/engines/common/EngineFactory.hpp"
#include "openpuzzle/engines/common/EngineRegistry.hpp"
#include "openpuzzle/runtime/VanityRangeProtocol.hpp"
#include <chrono>
#include <filesystem>
namespace openpuzzle::vanityPreview {
struct AdapterConfig {
  std::string adapter,engine,knownKeys,binding,controls,mode="exhaust";
  bool targetMode=false;
  bool commonRuntime=false;
};
std::string shellQuote(const std::string& value);
uint64_t inclusiveCount(const std::string& start,const std::string& end);
class VanityPreviewEngine final:public ISearchEngine {
 public:
  explicit VanityPreviewEngine(AdapterConfig config):config_(std::move(config)){}
  EngineInfo info()const override;
  bool prepare()override;
  std::string buildCommand(const EngineLaunchRequest&)const override;
  bool launch()override{return false;} // Execution belongs to the shared process runner.
  bool stop()override{return false;}
  bool running()const override{return false;}
 private:AdapterConfig config_;
};
void registerVanityPreview(EngineRegistry&,EngineFactory&,const AdapterConfig&,bool explicitSyntheticOptIn);
class VanityPreviewProgressParser final:public EngineOutputParser {
 public:
  explicit VanityPreviewProgressParser(uint64_t total,uint64_t resumed=0);
  std::optional<ExecutionProgress> parseLine(const std::string&)override;
 private:uint64_t total_,checked_,resumed_;bool terminal_=false;
  std::chrono::steady_clock::time_point start_=std::chrono::steady_clock::now();
};
}
