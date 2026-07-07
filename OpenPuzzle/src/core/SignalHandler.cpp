#include "openpuzzle/core/SignalHandler.hpp"

#include <atomic>
#include <csignal>

namespace openpuzzle {
namespace {

std::atomic_bool g_stopRequested{false};

void handleSignal(int) {
    g_stopRequested.store(true);
}

} // namespace

void SignalHandler::install() {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
}

bool SignalHandler::stopRequested() {
    return g_stopRequested.load();
}

void SignalHandler::reset() {
    g_stopRequested.store(false);
}

} // namespace openpuzzle
