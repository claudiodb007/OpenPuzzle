if(NOT DEFINED INSTALLER OR NOT EXISTS "${INSTALLER}")
  message(FATAL_ERROR "PSCKangaroo installer is required")
endif()
if(NOT DEFINED PATCH_FILE OR NOT EXISTS "${PATCH_FILE}")
  message(FATAL_ERROR "PSCKangaroo compatibility patch is required")
endif()

file(SHA256 "${PATCH_FILE}" actual_patch_sha)
file(READ "${INSTALLER}" installer_content)
file(READ "${PATCH_FILE}" patch_content)

if(NOT installer_content MATCHES "PATCH_SHA256=\"${actual_patch_sha}\"")
  message(FATAL_ERROR "installer does not pin the bundled patch SHA-256")
endif()
foreach(required_text
    "git -C \"$source_dir\" apply --check \"$patch_file\""
    "strip --strip-all \"$source_dir/psckangaroo\""
    "binary_symbols=stripped"
    "PATCHED_RCK_SHA256"
    "PATCHED_README_SHA256"
    "schema=openpuzzle-external-engine-v2"
    "patch_sha256=$PATCH_SHA256")
  string(FIND "${installer_content}" "${required_text}" required_position)
  if(required_position EQUAL -1)
    message(FATAL_ERROR "installer patch contract is missing: ${required_text}")
  endif()
endforeach()

foreach(required_patch_text
    "-seed <16hex>"
    "gWalkSeedSet"
    "SetRndSeed")
  string(FIND "${patch_content}" "${required_patch_text}" required_position)
  if(required_position EQUAL -1)
    message(FATAL_ERROR "compatibility patch is missing: ${required_patch_text}")
  endif()
endforeach()

if(NOT DEFINED CHECKPOINT_PATCH_FILE OR NOT EXISTS "${CHECKPOINT_PATCH_FILE}")
  message(FATAL_ERROR "PSCKangaroo bitmap checkpoint patch is required")
endif()
file(SHA256 "${CHECKPOINT_PATCH_FILE}" actual_checkpoint_patch_sha)
if(NOT installer_content MATCHES "CHECKPOINT_PATCH_SHA256=\"${actual_checkpoint_patch_sha}\"")
  message(FATAL_ERROR "installer does not pin the bitmap checkpoint patch SHA-256")
endif()
foreach(required_checkpoint_text
    "git -C \"$source_dir\" apply --check \"$checkpoint_patch_file\""
    "git -C \"$source_dir\" apply --reverse --check \"$checkpoint_patch_file\""
    "PATCHED_TAMESTORE_SHA256"
    "checkpoint_storage=bitmap-blocks-v1"
    "checkpoint_format=OPZKCP1-v4"
    "-lcuda -lpthread -lz"
    "checkpoint_patch_sha256=$CHECKPOINT_PATCH_SHA256")
  string(FIND "${installer_content}" "${required_checkpoint_text}" required_position)
  if(required_position EQUAL -1)
    message(FATAL_ERROR "installer checkpoint contract is missing: ${required_checkpoint_text}")
  endif()
endforeach()
