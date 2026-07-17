#pragma once

namespace openpuzzle {

class SignalHandler {
public:
    static void install();

    static bool stopRequested();

    static void reset();
};

} // namespace openpuzzle
