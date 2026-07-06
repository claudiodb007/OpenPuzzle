#include "openpuzzle/services/SchedulerService.hpp"

#include <iostream>

namespace openpuzzle {

int SchedulerService::executeResume(const std::vector<std::string>&) {
    std::cout << "Scheduler resume service is not implemented yet.\n";
    return 0;
}

int SchedulerService::executeDashboard(const std::vector<std::string>&) {
    std::cout << "Scheduler dashboard service is not implemented yet.\n";
    return 0;
}

int SchedulerService::executeStats(const std::vector<std::string>&) {
    std::cout << "Scheduler statistics service is not implemented yet.\n";
    return 0;
}

} // namespace openpuzzle
