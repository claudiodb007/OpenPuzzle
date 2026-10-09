#include "openpuzzle/core/SignalHandler.hpp"

#include <atomic>
#include <csignal>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace openpuzzle {
namespace {

std::atomic_bool g_stopRequested{false};

#ifdef _WIN32
BOOL WINAPI handleConsole(DWORD event) {
    if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT) return FALSE;
    g_stopRequested.store(true);
    return TRUE;
}
#endif

void handleSignal(int) {
    g_stopRequested.store(true);
}

} // namespace

void SignalHandler::install() {
#ifdef _WIN32
    SetConsoleCtrlHandler(handleConsole, TRUE);
#endif
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
}

bool SignalHandler::stopRequested() {
    return g_stopRequested.load();
}

void SignalHandler::requestStop() {
    g_stopRequested.store(true);
}

void SignalHandler::reset() {
    g_stopRequested.store(false);
}

} // namespace openpuzzle
