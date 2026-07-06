#include "openpuzzle/dispatcher/Dispatcher.hpp"

#include "openpuzzle/database/Database.hpp"

namespace openpuzzle {

Dispatcher::Dispatcher(Database& database)
    : database_(database) {}

std::optional<DispatchTask> Dispatcher::next() {
    return std::nullopt;
}

} // namespace openpuzzle
