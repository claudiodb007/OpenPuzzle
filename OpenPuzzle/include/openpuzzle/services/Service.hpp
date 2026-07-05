#pragma once

namespace openpuzzle {

class Database;

class Service {
public:
    explicit Service(Database& database) : database_(database) {}
    virtual ~Service() = default;

protected:
    Database& database_;
};

} // namespace openpuzzle
