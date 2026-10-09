#pragma once
#include <string>
#include <vector>
namespace openpuzzle {
struct RunCommandOptions {
  std::string command="run", server, engine, backend;
  int puzzle=0,device=0,blocks=256,threads=256,points=256,durationMinutes=60;
  bool once=false,dryRun=false,preflightOnly=false;
  bool gpuOptionsSpecified=false,threadsSpecified=false;
  static RunCommandOptions parse(const std::vector<std::string>& arguments);
};
}
