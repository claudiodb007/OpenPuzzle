#include "openpuzzle/services/ServiceUtils.hpp"

namespace openpuzzle::services {

std::string getArg(const std::vector<std::string>& args,
                   const std::string& name,
                   const std::string& def) {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == name) {
            return args[i + 1];
        }
    }

    return def;
}

int getIntArg(const std::vector<std::string>& args,
              const std::string& name,
              int def) {
    auto value = getArg(args, name, "");

    if (value.empty()) {
        return def;
    }

    return std::stoi(value);
}

std::string rangeStatusToString(RangeStatus status) {
    switch (status) {
    case RangeStatus::Reserved:
        return "RESERVED";
    case RangeStatus::Running:
        return "RUNNING";
    case RangeStatus::Completed:
        return "COMPLETED";
    case RangeStatus::Failed:
        return "FAILED";
    case RangeStatus::Cancelled:
        return "CANCELLED";
    case RangeStatus::External:
        return "EXTERNAL";
    }

    return "UNKNOWN";
}

} // namespace openpuzzle::services
