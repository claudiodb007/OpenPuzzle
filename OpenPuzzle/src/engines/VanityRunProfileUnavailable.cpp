#include "openpuzzle/engines/VanityRunProfile.hpp"
#include <stdexcept>
namespace openpuzzle {
namespace {[[noreturn]] void unavailable(){throw std::runtime_error("Vanity common routing is not included in this build");}}
bool VanityRunProfile::configured(){return false;}
VanityRunProfile VanityRunProfile::load(int){unavailable();}
void VanityRunProfile::requireServer(const std::string&){unavailable();}
std::uint64_t VanityRunProfile::maximumKeys()const{unavailable();}
std::uint64_t VanityRunProfile::requestedKeys(int)const{unavailable();}
void VanityRunProfile::validateHardware(int,bool)const{unavailable();}
void VanityRunProfile::validatePuzzle(const PuzzleExecutionMetadata&)const{unavailable();}
void VanityRunProfile::validateAssignment(const client::RangeAssignment&,const PuzzleExecutionMetadata&)const{unavailable();}
void VanityRunProfile::bindWorkspace(const std::filesystem::path&,const std::string&)const{unavailable();}
void VanityRunProfile::requireWorkspaceBinding(const std::filesystem::path&,const std::string&)const{unavailable();}
std::uint64_t VanityRunProfile::prepareRecovery(const client::ClientExecutionState&,const std::filesystem::path&,const std::string&)const{unavailable();}
}
