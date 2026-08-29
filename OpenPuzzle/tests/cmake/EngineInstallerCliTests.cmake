if(NOT DEFINED OPENPUZZLE_CLI)
  message(FATAL_ERROR "OPENPUZZLE_CLI is required")
endif()

execute_process(
  COMMAND "${OPENPUZZLE_CLI}" engine install psckangaroo --help
  RESULT_VARIABLE help_result
  OUTPUT_VARIABLE help_output
  ERROR_VARIABLE help_error
)
if(NOT help_result EQUAL 0 OR
   NOT help_output MATCHES "openpuzzle engine install psckangaroo")
  message(FATAL_ERROR "engine installer help failed: ${help_error}")
endif()

set(fake_installer "${CMAKE_CURRENT_BINARY_DIR}/fake-psckangaroo-installer.sh")
file(WRITE "${fake_installer}" "#!/bin/sh\nprintf 'FAKE_PSCKANGAROO_INSTALLER_DISPATCHED\\n'\n")
file(CHMOD "${fake_installer}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "OPENPUZZLE_PSCKANGAROO_INSTALLER=${fake_installer}"
          "${OPENPUZZLE_CLI}" engine install psckangaroo
  RESULT_VARIABLE dispatch_result
  OUTPUT_VARIABLE dispatch_output
  ERROR_VARIABLE dispatch_error
)
if(NOT dispatch_result EQUAL 0 OR
   NOT dispatch_output MATCHES "FAKE_PSCKANGAROO_INSTALLER_DISPATCHED")
  message(FATAL_ERROR "engine installer dispatch failed: ${dispatch_error}")
endif()

execute_process(
  COMMAND "${OPENPUZZLE_CLI}" engine install unsupported-engine
  RESULT_VARIABLE invalid_result
)
if(invalid_result EQUAL 0)
  message(FATAL_ERROR "unsupported engine was accepted")
endif()
