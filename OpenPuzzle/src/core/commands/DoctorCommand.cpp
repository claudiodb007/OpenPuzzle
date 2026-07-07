#include "openpuzzle/core/commands/DoctorCommand.hpp"

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/services/DoctorService.hpp"

namespace openpuzzle {

int DoctorCommand::run(const std::vector<std::string>& args) const {
    Database db;

    if (!db.open()) {
        return 1;
    }

    DoctorService service(db);

    return service.execute(args);
}

} // namespace openpuzzle
