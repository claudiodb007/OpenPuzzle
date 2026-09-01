if(NOT DEFINED OPENPUZZLE_BIN)
  message(FATAL_ERROR "OPENPUZZLE_BIN is required")
endif()

set(TEST_ROOT "${CMAKE_CURRENT_BINARY_DIR}/kangaroo-selftest-cli")
set(TEST_HOME "${TEST_ROOT}/home")
set(FAKE_ENGINE "${TEST_ROOT}/fake-psckangaroo.sh")
set(SEED_CAPTURE "${TEST_ROOT}/captured-seed.txt")

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_HOME}")
file(WRITE "${FAKE_ENGINE}" [=[#!/bin/sh
if [ "$#" -ne 20 ] ||
   [ "$1" != "-gpu" ] || [ "$2" != "0" ] ||
   [ "$3" != "-dp" ] || [ "$4" != "6" ] ||
   [ "$5" != "-range" ] || [ "$6" != "32" ] ||
   [ "$7" != "-pubkey" ] ||
   [ "$8" != "02c9fb33cd0e097bc593ca0ec10f80b8e8cc1eae11d67e89dbc156d5ff56bb7f5f" ] ||
   [ "$9" != "-start" ] || [ "${10}" != "100000000" ] ||
   [ "${11}" != "-seed" ] ||
   [ "${13}" != "-ramlimit" ] || [ "${14}" != "4" ] ||
   [ "${15}" != "-concurrent" ] || [ "${16}" != "1" ] ||
   [ "${17}" != "-wwbuffer" ] || [ "${18}" != "5" ] ||
   [ "${19}" != "-checkpoint" ] || [ "${20}" != "0" ]; then
  printf '%s\n' 'unexpected Kangaroo self-test arguments' >&2
  exit 64
fi

seed="${12}"

if [ "${#seed}" -ne 16 ]; then
  printf '%s\n' 'invalid Kangaroo self-test walk seed length' >&2
  exit 66
fi

case "$seed" in
  *[!0123456789abcdefABCDEF]*)
    printf '%s\n' 'invalid Kangaroo self-test walk seed format' >&2
    exit 67
    ;;
esac

if [ "$seed" = "0000000000000000" ]; then
  printf '%s\n' 'zero Kangaroo self-test walk seed' >&2
  exit 68
fi

: "${OPENPUZZLE_SELFTEST_SEED_CAPTURE:?}"
printf '%s\n' "$seed" > "$OPENPUZZLE_SELFTEST_SEED_CAPTURE"

case " $* " in
  *" 180000001 "*)
    printf '%s\n' 'private test value appeared in arguments' >&2
    exit 65
    ;;
esac

printf '%s\n' 'CONC: Speed: 0.75 GKeys/s | Time: 0d 00h 01m'
printf '%s\n' 'PRIVATE KEY: 0000000000000000000000000000000000000000000000000000000180000001' > RESULTS.TXT
exit 0
]=])
file(CHMOD "${FAKE_ENGINE}"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "HOME=${TEST_HOME}"
          "OPENPUZZLE_KANGAROO_PATH=${FAKE_ENGINE}"
          "OPENPUZZLE_SELFTEST_SEED_CAPTURE=${SEED_CAPTURE}"
          "${OPENPUZZLE_BIN}" selftest --backend kangaroo --device 0
  RESULT_VARIABLE selftest_result
  OUTPUT_VARIABLE selftest_stdout
  ERROR_VARIABLE selftest_stderr
  TIMEOUT 10
)

set(selftest_output "${selftest_stdout}${selftest_stderr}")

if(NOT selftest_result EQUAL 0)
  message(FATAL_ERROR
    "Kangaroo self-test returned ${selftest_result}\n${selftest_output}")
endif()

foreach(expected
    "Backend............ kangaroo"
    "Vector............. synthetic 32-bit"
    "Result............. passed"
    "Speed.............. 750.00 MKey/s")
  string(FIND "${selftest_output}" "${expected}" found_at)
  if(found_at EQUAL -1)
    message(FATAL_ERROR
      "Kangaroo self-test output is missing: ${expected}\n${selftest_output}")
  endif()
endforeach()

if(NOT EXISTS "${SEED_CAPTURE}")
  message(FATAL_ERROR "Kangaroo self-test did not provide a walk seed")
endif()

file(READ "${SEED_CAPTURE}" captured_seed)
string(STRIP "${captured_seed}" captured_seed)
string(LENGTH "${captured_seed}" captured_seed_length)

if(NOT captured_seed_length EQUAL 16 OR
   NOT captured_seed MATCHES "^[0-9A-Fa-f]+$" OR
   captured_seed STREQUAL "0000000000000000")
  message(FATAL_ERROR
    "Kangaroo self-test supplied an invalid walk seed")
endif()

string(FIND "${selftest_output}" "${captured_seed}" leaked_seed_at)
if(NOT leaked_seed_at EQUAL -1)
  message(FATAL_ERROR
    "Kangaroo self-test exposed its walk seed in public output")
endif()

string(FIND "${selftest_output}" "180000001" leaked_private_at)
if(NOT leaked_private_at EQUAL -1)
  message(FATAL_ERROR
    "Kangaroo self-test exposed private synthetic material")
endif()

file(GLOB remaining_workspaces
  "${TEST_HOME}/.local/share/OpenPuzzle/selftest/kangaroo-*")
if(remaining_workspaces)
  message(FATAL_ERROR
    "Successful Kangaroo self-test left a workspace: ${remaining_workspaces}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "HOME=${TEST_HOME}"
          "OPENPUZZLE_KANGAROO_PATH=/nonexistent/psckangaroo"
          "${OPENPUZZLE_BIN}" selftest --backend kangaroo
  RESULT_VARIABLE missing_result
  OUTPUT_VARIABLE missing_stdout
  ERROR_VARIABLE missing_stderr
  TIMEOUT 10
)

set(missing_output "${missing_stdout}${missing_stderr}")
if(NOT missing_result EQUAL 1 OR
   NOT missing_output MATCHES "selected self-test engine is unavailable")
  message(FATAL_ERROR
    "Missing Kangaroo executor did not fail closed\n${missing_output}")
endif()

file(WRITE "${FAKE_ENGINE}" [=[#!/bin/sh
printf '%s\n' 'CONC: Speed: 10 MKeys/s'
printf '%s\n' 'PRIVATE KEY: 0000000000000000000000000000000000000000000000000000000180000002' > RESULTS.TXT
exit 0
]=])
file(CHMOD "${FAKE_ENGINE}"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "HOME=${TEST_HOME}"
          "OPENPUZZLE_KANGAROO_PATH=${FAKE_ENGINE}"
          "${OPENPUZZLE_BIN}" selftest --backend kangaroo
  RESULT_VARIABLE wrong_result
  OUTPUT_VARIABLE wrong_stdout
  ERROR_VARIABLE wrong_stderr
  TIMEOUT 10
)

set(wrong_output "${wrong_stdout}${wrong_stderr}")
if(NOT wrong_result EQUAL 1 OR
   NOT wrong_output MATCHES "Result[.]+ failed" OR
   wrong_output MATCHES "180000001")
  message(FATAL_ERROR
    "Incorrect Kangaroo result was not handled safely\n${wrong_output}")
endif()

file(GLOB failed_workspaces
  "${TEST_HOME}/.local/share/OpenPuzzle/selftest/kangaroo-*")
list(LENGTH failed_workspaces failed_workspace_count)
if(NOT failed_workspace_count EQUAL 1)
  message(FATAL_ERROR
    "Expected one protected diagnostic workspace, found: ${failed_workspaces}")
endif()

file(GLOB failed_workspace_entries "${failed_workspaces}/*")
list(LENGTH failed_workspace_entries failed_entry_count)
if(NOT failed_entry_count EQUAL 1 OR
   NOT EXISTS "${failed_workspaces}/engine.log" OR
   EXISTS "${failed_workspaces}/RESULTS.TXT")
  message(FATAL_ERROR
    "Failed Kangaroo self-test preserved private result material")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
