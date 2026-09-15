if(NOT DEFINED OPENPUZZLE_BUILD_DIR)
    message(FATAL_ERROR "OPENPUZZLE_BUILD_DIR is required")
endif()

set(_prefix "${OPENPUZZLE_BUILD_DIR}/desktop-ui-install-test")
file(REMOVE_RECURSE "${_prefix}")

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        --install "${OPENPUZZLE_BUILD_DIR}"
        --prefix "${_prefix}"
        --component DesktopUi
    RESULT_VARIABLE _install_result
    OUTPUT_VARIABLE _install_output
    ERROR_VARIABLE _install_error
)

if(NOT _install_result EQUAL 0)
    message(FATAL_ERROR
        "DesktopUi installation failed:\n"
        "${_install_output}${_install_error}"
    )
endif()

set(_required_files
    "bin/openpuzzle-ui"
    "share/applications/openpuzzle-ui.desktop"
    "share/pixmaps/openpuzzle.png"
    "share/icons/hicolor/512x512/apps/openpuzzle.png"
)

foreach(_relative_path IN LISTS _required_files)
    if(NOT EXISTS "${_prefix}/${_relative_path}")
        message(FATAL_ERROR
            "DesktopUi file was not installed: ${_relative_path}"
        )
    endif()
endforeach()

if(EXISTS "${_prefix}/bin/openpuzzle")
    message(FATAL_ERROR
        "DesktopUi component unexpectedly installed the CLI"
    )
endif()

file(GLOB_RECURSE _installed_files
    LIST_DIRECTORIES false
    RELATIVE "${_prefix}"
    "${_prefix}/*"
)
list(SORT _installed_files)
list(SORT _required_files)

if(NOT _installed_files STREQUAL _required_files)
    message(FATAL_ERROR
        "Unexpected DesktopUi installation contents:\n"
        "${_installed_files}"
    )
endif()

file(READ
    "${_prefix}/share/applications/openpuzzle-ui.desktop"
    _desktop_entry
)

foreach(_required_entry
        "TryExec=openpuzzle-ui"
        "Exec=openpuzzle-ui"
        "Icon=openpuzzle"
        "Terminal=false"
        "Comment[pt]="
        "Comment[fr]="
        "Comment[es]=")
    string(FIND "${_desktop_entry}" "${_required_entry}" _entry_position)
    if(_entry_position EQUAL -1)
        message(FATAL_ERROR
            "Desktop entry is missing: ${_required_entry}"
        )
    endif()
endforeach()
