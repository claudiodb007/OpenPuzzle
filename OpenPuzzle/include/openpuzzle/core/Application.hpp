#pragma once
#include "openpuzzle/database/Database.hpp"
#include <string>
#include <vector>
namespace openpuzzle { class Application { public: int run(int argc,char** argv); private: std::string dbPath() const; bool ensureDb(Database& db); static bool hasArg(const std::vector<std::string>& args,const std::string& name); static std::string getArg(const std::vector<std::string>& args,const std::string& name,const std::string& def={}); static int getIntArg(const std::vector<std::string>& args,const std::string& name,int def); int cmdInit(); int cmdImportPuzzleJson(const std::vector<std::string>&); int cmdListPuzzles(); int cmdCreateJob(const std::vector<std::string>&); int cmdListRanges(const std::vector<std::string>&); int cmdCompleteJob(const std::vector<std::string>&); int cmdStats(const std::vector<std::string>&); int cmdConfigureTool(const std::vector<std::string>&); int cmdTools(); int cmdGpuList(); int cmdGpuSelect(const std::vector<std::string>&); int cmdBitcrackCommand(const std::vector<std::string>&); int cmdStartJob(const std::vector<std::string>&); int cmdProcessTest(const std::vector<std::string>&); int cmdExecutionTest(const std::vector<std::string>&);
int cmdParseBitCrackLine(const std::vector<std::string>&);
int cmdDashboard(const std::vector<std::string>&);
int cmdAudit(const std::vector<std::string>&); }; }
