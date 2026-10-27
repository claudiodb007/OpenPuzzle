#pragma once
#include <map>
#include <string>
namespace openpuzzle::vanityPreview {
int runCoordinatorSession(const std::map<std::string,std::string>&,bool resume);
}
