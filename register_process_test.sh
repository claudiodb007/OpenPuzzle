#!/usr/bin/env bash
set -euo pipefail

# Run from repository root:
# /home/claudiodb/Secretária/OpenPuzzle/OpenPuzzle_0.12_github_ready

if [ ! -d "OpenPuzzle" ]; then
  echo "ERROR: run this from the repo root that contains the OpenPuzzle/ directory."
  exit 1
fi

if [ ! -f "OpenPuzzle/include/openpuzzle/core/ProcessRunner.hpp" ] || [ ! -f "OpenPuzzle/src/core/ProcessRunner.cpp" ]; then
  echo "ERROR: ProcessRunner files are missing."
  echo "Expected:"
  echo "  OpenPuzzle/include/openpuzzle/core/ProcessRunner.hpp"
  echo "  OpenPuzzle/src/core/ProcessRunner.cpp"
  exit 1
fi

python3 <<'PY'
from pathlib import Path

app_hpp = Path("OpenPuzzle/include/openpuzzle/core/Application.hpp")
app_cpp = Path("OpenPuzzle/src/core/Application.cpp")
cmake = Path("OpenPuzzle/CMakeLists.txt")

cmake_txt = cmake.read_text()
if "src/core/ProcessRunner.cpp" not in cmake_txt:
    marker = "src/core/Application.cpp"
    if marker not in cmake_txt:
        raise SystemExit("Could not find src/core/Application.cpp in CMakeLists.txt")
    cmake_txt = cmake_txt.replace(marker, marker + "\n    src/core/ProcessRunner.cpp", 1)
    cmake.write_text(cmake_txt)

hpp = app_hpp.read_text()
if "cmdProcessTest" not in hpp:
    marker = "int cmdStartJob(const std::vector<std::string>&"
    pos = hpp.find(marker)
    if pos == -1:
        raise SystemExit("Could not find cmdStartJob declaration in Application.hpp")
    semi = hpp.find(";", pos)
    if semi == -1:
        raise SystemExit("Could not find end of cmdStartJob declaration")
    hpp = hpp[:semi+1] + " int cmdProcessTest(const std::vector<std::string>&);" + hpp[semi+1:]
    app_hpp.write_text(hpp)

cpp = app_cpp.read_text()
if '#include "openpuzzle/core/ProcessRunner.hpp"' not in cpp:
    first_include = cpp.find("#include")
    if first_include == -1:
        raise SystemExit("No #include found in Application.cpp")
    end_line = cpp.find("\n", first_include)
    cpp = cpp[:end_line+1] + '#include "openpuzzle/core/ProcessRunner.hpp"\n' + cpp[end_line+1:]

if 'cmd=="process-test"' not in cpp and 'cmd == "process-test"' not in cpp:
    if 'if(cmd=="start-job")return cmdStartJob(r);' in cpp:
        cpp = cpp.replace(
            'if(cmd=="start-job")return cmdStartJob(r);',
            'if(cmd=="start-job")return cmdStartJob(r); if(cmd=="process-test")return cmdProcessTest(r);',
            1
        )
    elif 'if (cmd == "start-job") return cmdStartJob(rest);' in cpp:
        cpp = cpp.replace(
            'if (cmd == "start-job") return cmdStartJob(rest);',
            'if (cmd == "start-job") return cmdStartJob(rest);\n        if (cmd == "process-test") return cmdProcessTest(rest);',
            1
        )
    else:
        raise SystemExit("Could not find start-job dispatcher in Application.cpp")

if "Application::cmdProcessTest" not in cpp:
    func = r'''
int Application::cmdProcessTest(const std::vector<std::string>& args) {
    std::string command = getArg(args, "--command", "printf 'line1\\nline2\\n'");

    ProcessRunner runner;

    auto result = runner.run(command, [](const std::string& line) {
        std::cout << "OUT.................. " << line << "\n";
    });

    std::cout << "Started.............. " << (result.started ? "yes" : "no") << "\n";
    std::cout << "Exit code............ " << result.exitCode << "\n";

    return result.exitCode == 0 ? 0 : 1;
}

'''
    marker = "} // namespace openpuzzle"
    idx = cpp.rfind(marker)
    if idx != -1:
        cpp = cpp[:idx] + func + "\n" + cpp[idx:]
    else:
        cpp += "\nnamespace openpuzzle {\n" + func + "\n}\n"

app_cpp.write_text(cpp)
PY

echo "ProcessRunner command registered."
echo
echo "Now build and test:"
echo "  cd OpenPuzzle"
echo "  rm -rf build && mkdir build && cd build"
echo "  cmake .."
echo "  cmake --build ."
echo "  ./OpenPuzzle process-test"
