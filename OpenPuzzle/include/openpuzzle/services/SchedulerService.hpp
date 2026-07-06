#pragma once

#include "openpuzzle/services/Service.hpp"

#include <string>
#include <vector>

namespace openpuzzle {

class SchedulerService : public Service {
public:
    using Service::Service;

    int executeResume(const std::vector<std::string>& args);
    int executeDashboard(const std::vector<std::string>& args);
    int executeStats(const std::vector<std::string>& args);
};

} // namespace openpuzzle
