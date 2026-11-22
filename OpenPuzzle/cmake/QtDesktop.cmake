# Both operating systems compile this one widget tree, stylesheet, translations,
# artwork and font set. Platform differences are command/capability adapters.
function(openpuzzle_add_qt_panel target settings)
    set(_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    set(_entry "${_root}/src/ui/main.cpp")
    if(ARGC GREATER 2)
        set(_entry "${ARGV2}")
    endif()
    add_executable(${target}
        "${_entry}"
        "${_root}/src/ui/MainWindow.cpp"
        "${_root}/src/ui/MainWindow.hpp"
        "${_root}/src/ui/GpuControlPanel.cpp"
        "${_root}/src/ui/GpuControlPanel.hpp"
        "${_root}/src/ui/ThermalAlerts.cpp"
        "${_root}/src/ui/ThermalHistory.cpp"
        "${_root}/src/ui/RunCommandBuilder.cpp"
        "${_root}/src/ui/UiLanguage.cpp"
        "${_root}/src/ui/UiTheme.cpp"
        "${_root}/resources/openpuzzle-ui.qrc")
    set_target_properties(${target} PROPERTIES OUTPUT_NAME openpuzzle-ui AUTOMOC ON AUTORCC ON)
    target_include_directories(${target} PRIVATE "${_root}/include" "${_root}/src/ui")
    target_link_libraries(${target} PRIVATE ${settings} Qt6::Widgets)
    if(WIN32)
        set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE ON)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_root}/include/openpuzzle/core/WindowsVersion.hpp")
        file(STRINGS "${_root}/include/openpuzzle/core/WindowsVersion.hpp" _version_line REGEX "windowsClientVersion=")
        string(REGEX REPLACE ".*windowsClientVersion=\"([^\"]+)\".*" "\\1" _version "${_version_line}")
        target_compile_definitions(${target} PRIVATE OPENPUZZLE_DESKTOP_WINDOWS=1 OPENPUZZLE_VERSION="${_version}")
        target_sources(${target} PRIVATE "${_root}/src/ui/WindowsUiProcess.cpp")
        target_link_libraries(${target} PRIVATE OpenPuzzleRunPreparation OpenPuzzleGpuControl shell32)
    else()
        target_compile_definitions(${target} PRIVATE OPENPUZZLE_VERSION="${PROJECT_VERSION}")
    endif()
    if(OPENPUZZLE_ENABLE_VANITY OR WIN32)
        target_sources(${target} PRIVATE "${_root}/src/ui/VanityPanel.cpp")
        target_compile_definitions(${target} PRIVATE OPENPUZZLE_ENABLE_VANITY_PREVIEW=1)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
