#include "openpuzzle/services/BenchmarkService.hpp"

#include "openpuzzle/core/commands/BenchmarkCommand.hpp"

namespace openpuzzle {

int BenchmarkService::execute(const std::vector<std::string>& args) {
    BenchmarkCommand command;
    return command.run(args);
}

} // namespace openpuzzle
