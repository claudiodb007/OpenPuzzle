#pragma once
#ifdef _WIN32
#include <string>
#include <ostream>
namespace openpuzzle {
// Explicit reconciliation only: no engine restart, heartbeat, registration or
// new claim. Uncertain receipts and local solutions remain preserved.
int recoverWindowsAssignment(const std::string& slot,const std::string& server,std::ostream& output);
}
#endif
