#pragma once

#include <string>

namespace openpuzzle {

class Database;

class HeartbeatService {
public:
    explicit HeartbeatService(Database& database);

    void update(const std::string& machine,
                const std::string& gpu,
                const std::string& backend,
                const std::string& engine,
                const std::string& status,
                double speed,
                double temperature,
                double power);

private:
    Database& database_;
};

} // namespace openpuzzle
