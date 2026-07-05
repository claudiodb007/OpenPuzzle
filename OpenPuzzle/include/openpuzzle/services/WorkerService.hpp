#pragma once

#include <string>
#include <vector>

namespace openpuzzle {

class Database;

class WorkerService {
public:
    explicit WorkerService(Database& database);

    int execute(const std::vector<std::string>& args);

private:
    Database& database_;
};

} // namespace openpuzzle
