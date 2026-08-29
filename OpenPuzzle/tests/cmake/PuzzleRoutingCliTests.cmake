if(NOT DEFINED OPENPUZZLE_BIN)
  message(FATAL_ERROR "OPENPUZZLE_BIN is required")
endif()

set(TEST_ROOT "${CMAKE_CURRENT_BINARY_DIR}/puzzle-routing-cli-test")
file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")

function(require_text content expected label)
  string(FIND "${content}" "${expected}" found_at)
  if(found_at EQUAL -1)
    message(FATAL_ERROR
      "${label}: expected text was not found: ${expected}\n${content}")
  endif()
endfunction()

function(reject_text content rejected label)
  string(FIND "${content}" "${rejected}" found_at)
  if(NOT found_at EQUAL -1)
    message(FATAL_ERROR
      "${label}: rejected text was found: ${rejected}\n${content}")
  endif()
endfunction()

# 1. Valid Kangaroo metadata with no executor must fail before any request.
file(WRITE "${TEST_ROOT}/140.json" [=[{
  "number": 140,
  "address": "1QKBaU6WAeycb3DbKbLBkX7vJiaS8r42Xo",
  "hash160": "ffbb35a7bb9bbe16c1aa2534f7ff11d59c8e3d1a",
  "search_mode": "kangaroo",
  "keyspace": "80000000000000000000000000000000000:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
  "public_key": "031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640",
  "required_backend": "cuda"
}
]=])

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "OPENPUZZLE_PUZZLE_DIR=${TEST_ROOT}"
          "OPENPUZZLE_KANGAROO_PATH=/nonexistent/openpuzzle-cli-test-psckangaroo"
          "${OPENPUZZLE_BIN}" run 140 --dry-run
          --server http://127.0.0.1:1
  RESULT_VARIABLE kangaroo_missing_result
  OUTPUT_VARIABLE kangaroo_missing_stdout
  ERROR_VARIABLE kangaroo_missing_stderr
  TIMEOUT 10
)
set(kangaroo_missing_output
  "${kangaroo_missing_stdout}${kangaroo_missing_stderr}")
if(NOT kangaroo_missing_result EQUAL 1)
  message(FATAL_ERROR
    "Kangaroo missing-executor dry-run returned ${kangaroo_missing_result}\n${kangaroo_missing_output}")
endif()
require_text("${kangaroo_missing_output}"
  "Search mode........ Kangaroo" "Kangaroo missing executor")
require_text("${kangaroo_missing_output}"
  "Assignment......... not requested" "Kangaroo missing executor")
require_text("${kangaroo_missing_output}"
  "PSCKangaroo executable is not installed" "Kangaroo missing executor")
reject_text("${kangaroo_missing_output}"
  "Requesting assignment" "Kangaroo missing executor")

# 1b. An available executable makes the local dry-run ready, but dry-run must
# still stop before registration, assignment claim, or engine execution.
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "OPENPUZZLE_PUZZLE_DIR=${TEST_ROOT}"
          "OPENPUZZLE_KANGAROO_PATH=/bin/true"
          "${OPENPUZZLE_BIN}" run 140 --dry-run
          --server http://127.0.0.1:1
  RESULT_VARIABLE kangaroo_ready_result
  OUTPUT_VARIABLE kangaroo_ready_stdout
  ERROR_VARIABLE kangaroo_ready_stderr
  TIMEOUT 10
)
set(kangaroo_ready_output
  "${kangaroo_ready_stdout}${kangaroo_ready_stderr}")
if(NOT kangaroo_ready_result EQUAL 0)
  message(FATAL_ERROR
    "Kangaroo ready dry-run returned ${kangaroo_ready_result}\n${kangaroo_ready_output}")
endif()
require_text("${kangaroo_ready_output}"
  "Search mode........ Kangaroo" "Kangaroo ready")
require_text("${kangaroo_ready_output}"
  "Engine............. kangaroo" "Kangaroo ready")
require_text("${kangaroo_ready_output}"
  "Backend............ CUDA" "Kangaroo ready")
require_text("${kangaroo_ready_output}"
  "Executable......... /bin/true" "Kangaroo ready")
require_text("${kangaroo_ready_output}"
  "No assignment was requested" "Kangaroo ready")
reject_text("${kangaroo_ready_output}"
  "Requesting assignment" "Kangaroo ready")

# 2. Missing metadata for an explicit puzzle must fail before any request.
file(REMOVE "${TEST_ROOT}/140.json")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "OPENPUZZLE_PUZZLE_DIR=${TEST_ROOT}"
          "${OPENPUZZLE_BIN}" run 141 --dry-run
          --server http://127.0.0.1:1
  RESULT_VARIABLE missing_result
  OUTPUT_VARIABLE missing_stdout
  ERROR_VARIABLE missing_stderr
  TIMEOUT 10
)
set(missing_output "${missing_stdout}${missing_stderr}")
if(NOT missing_result EQUAL 1)
  message(FATAL_ERROR
    "Missing-metadata dry-run returned ${missing_result}\n${missing_output}")
endif()
require_text("${missing_output}"
  "OpenPuzzle puzzle metadata validation failed" "Missing metadata")
require_text("${missing_output}"
  "Assignment......... not requested" "Missing metadata")
require_text("${missing_output}"
  "puzzle metadata is unavailable or invalid" "Missing metadata")
reject_text("${missing_output}" "Assignment request failed" "Missing metadata")

# 3. Packaged linear puzzle 71 must retain its documented CPU dry-run path.
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "OPENPUZZLE_PUZZLE_DIR=${TEST_ROOT}"
          "${OPENPUZZLE_BIN}" run 71 --dry-run --backend cpu
          --server http://127.0.0.1:1
  RESULT_VARIABLE linear_result
  OUTPUT_VARIABLE linear_stdout
  ERROR_VARIABLE linear_stderr
  TIMEOUT 10
)
set(linear_output "${linear_stdout}${linear_stderr}")
if(NOT linear_result EQUAL 0)
  message(FATAL_ERROR
    "Linear CPU dry-run returned ${linear_result}\n${linear_output}")
endif()
reject_text("${linear_output}"
  "OpenPuzzle puzzle metadata validation failed" "Linear")
reject_text("${linear_output}" "Search mode........ Kangaroo" "Linear")
reject_text("${linear_output}" "Assignment request failed" "Linear")

file(REMOVE_RECURSE "${TEST_ROOT}")
