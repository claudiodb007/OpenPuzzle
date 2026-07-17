#pragma once

#include "openpuzzle/services/Service.hpp"

#include <string>
#include <vector>

namespace openpuzzle {

class DoctorService : public Service {
public:
    using Service::Service;

    int execute(const std::vector<std::string>& args);
};

} // namespace openpuzzle
