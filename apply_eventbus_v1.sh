#!/usr/bin/env bash
set -euo pipefail

if [ ! -d "OpenPuzzle" ]; then
  echo "ERROR: run this from the repo root that contains the OpenPuzzle/ directory."
  exit 1
fi

mkdir -p OpenPuzzle/include/openpuzzle/core
mkdir -p OpenPuzzle/src/core
mkdir -p OpenPuzzle/docs

cat > OpenPuzzle/include/openpuzzle/core/EventBus.hpp <<'EOF'
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace openpuzzle {

enum class EventType {
    ExecutionStarted,
    Speed,
    Progress,
    FoundKey,
    ExecutionFinished,
    Error
};

struct Event {
    EventType type = EventType::Progress;
    int executionId = 0;
    int jobId = 0;
    std::string message;
    std::string value;
    double numericValue = 0.0;
};

class EventBus {
public:
    using Listener = std::function<void(const Event&)>;

    void subscribe(const Listener& listener);
    void publish(const Event& event) const;

    static std::string eventTypeToString(EventType type);

private:
    std::vector<Listener> listeners_;
};

} // namespace openpuzzle
EOF

cat > OpenPuzzle/src/core/EventBus.cpp <<'EOF'
#include "openpuzzle/core/EventBus.hpp"

namespace openpuzzle {

void EventBus::subscribe(const Listener& listener) {
    listeners_.push_back(listener);
}

void EventBus::publish(const Event& event) const {
    for (const auto& listener : listeners_) {
        if (listener) {
            listener(event);
        }
    }
}

std::string EventBus::eventTypeToString(EventType type) {
    switch (type) {
        case EventType::ExecutionStarted:
            return "EXECUTION_STARTED";
        case EventType::Speed:
            return "SPEED";
        case EventType::Progress:
            return "PROGRESS";
        case EventType::FoundKey:
            return "FOUND_KEY";
        case EventType::ExecutionFinished:
            return "EXECUTION_FINISHED";
        case EventType::Error:
            return "ERROR";
    }

    return "UNKNOWN";
}

} // namespace openpuzzle
EOF

cat > OpenPuzzle/docs/EVENT_BUS.md <<'EOF'
# EventBus

`EventBus` is the internal event system for OpenPuzzle.

It allows core components to emit events without knowing who will consume them.

Example:

```text
ExecutionManager
  -> EventBus
      -> Dashboard
      -> Database telemetry
      -> Logger
      -> CommunitySync
```

## Current events

- `ExecutionStarted`
- `Speed`
- `Progress`
- `FoundKey`
- `ExecutionFinished`
- `Error`

## Test

```bash
./OpenPuzzle event-test
```

Expected output includes:

```text
EVENT................ EXECUTION_STARTED
EVENT................ SPEED
EVENT................ EXECUTION_FINISHED
Events received...... 3
```
EOF

python3 <<'PY'
from pathlib import Path

cmake = Path("OpenPuzzle/CMakeLists.txt")
txt = cmake.read_text()
if "src/core/EventBus.cpp" not in txt:
    marker = "src/core/ExecutionSession.cpp"
    if marker in txt:
        txt = txt.replace(marker, marker + "\n    src/core/EventBus.cpp", 1)
    else:
        marker = "src/core/Application.cpp"
        if marker not in txt:
            raise SystemExit("Could not patch CMakeLists.txt")
        txt = txt.replace(marker, marker + "\n    src/core/EventBus.cpp", 1)
    cmake.write_text(txt)

hpp_path = Path("OpenPuzzle/include/openpuzzle/core/Application.hpp")
hpp = hpp_path.read_text()
if "cmdEventTest" not in hpp:
    marker = "int cmdSessionTest(const std::vector<std::string>&);"
    if marker in hpp:
        hpp = hpp.replace(marker, marker + " int cmdEventTest(const std::vector<std::string>&);", 1)
    else:
        marker = "int cmdExecutionTest(const std::vector<std::string>&);"
        if marker not in hpp:
            raise SystemExit("Could not find insertion point in Application.hpp")
        hpp = hpp.replace(marker, marker + " int cmdEventTest(const std::vector<std::string>&);", 1)
    hpp_path.write_text(hpp)

cpp_path = Path("OpenPuzzle/src/core/Application.cpp")
cpp = cpp_path.read_text()

if '#include "openpuzzle/core/EventBus.hpp"' not in cpp:
    first_include = cpp.find("#include")
    end_line = cpp.find("\n", first_include)
    cpp = cpp[:end_line+1] + '#include "openpuzzle/core/EventBus.hpp"\n' + cpp[end_line+1:]

if 'cmd=="event-test"' not in cpp and 'cmd == "event-test"' not in cpp:
    if 'if(cmd=="session-test")return cmdSessionTest(r);' in cpp:
        cpp = cpp.replace(
            'if(cmd=="session-test")return cmdSessionTest(r);',
            'if(cmd=="session-test")return cmdSessionTest(r); if(cmd=="event-test")return cmdEventTest(r);',
            1
        )
    elif 'if(cmd=="execution-test")return cmdExecutionTest(r);' in cpp:
        cpp = cpp.replace(
            'if(cmd=="execution-test")return cmdExecutionTest(r);',
            'if(cmd=="execution-test")return cmdExecutionTest(r); if(cmd=="event-test")return cmdEventTest(r);',
            1
        )
    else:
        raise SystemExit("Could not patch command dispatcher in Application.cpp")

if "Application::cmdEventTest" not in cpp:
    func = r'''
int Application::cmdEventTest(const std::vector<std::string>& args) {
    (void)args;

    EventBus bus;
    int received = 0;

    bus.subscribe([&](const Event& event) {
        received++;
        std::cout << "EVENT................ " << EventBus::eventTypeToString(event.type) << "\n";

        if (!event.message.empty()) {
            std::cout << "Message.............. " << event.message << "\n";
        }

        if (!event.value.empty()) {
            std::cout << "Value................ " << event.value << "\n";
        }

        if (event.numericValue != 0.0) {
            std::cout << "Numeric.............. " << event.numericValue << "\n";
        }
    });

    bus.publish(Event{EventType::ExecutionStarted, 1, 42, "Execution started", "", 0.0});
    bus.publish(Event{EventType::Speed, 1, 42, "Speed sample", "MKey/s", 1334.62});
    bus.publish(Event{EventType::ExecutionFinished, 1, 42, "Execution finished", "", 0.0});

    std::cout << "Events received...... " << received << "\n";

    return received == 3 ? 0 : 1;
}

'''
    marker = "} // namespace openpuzzle"
    idx = cpp.rfind(marker)
    if idx != -1:
        cpp = cpp[:idx] + func + "\n" + cpp[idx:]
    else:
        cpp += "\nnamespace openpuzzle {\n" + func + "\n}\n"

cpp_path.write_text(cpp)

readme = Path("OpenPuzzle/README.md")
rtxt = readme.read_text()
if "EventBus test" not in rtxt:
    rtxt += "\n\n## EventBus test\n\n```bash\n./OpenPuzzle event-test\n```\n"
    readme.write_text(rtxt)

changelog = Path("OpenPuzzle/CHANGELOG.md")
ctxt = changelog.read_text()
if "0.17-dev — EventBus" not in ctxt:
    ctxt += "\n\n## 0.17-dev — EventBus\n\n- Added `EventBus`.\n- Added basic execution-related events.\n- Added `event-test` command.\n- Added `docs/EVENT_BUS.md`.\n"
    changelog.write_text(ctxt)
PY

echo "EventBus v1 applied."
echo
echo "Build and test:"
echo "  cd OpenPuzzle"
echo "  rm -rf build && mkdir build && cd build"
echo "  cmake .."
echo "  cmake --build ."
echo "  ./OpenPuzzle event-test"
