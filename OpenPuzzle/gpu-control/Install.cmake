# Used by normal Linux installs and both CPack package formats.
if(UNIX AND NOT APPLE)
    # Match the fixed desktop, systemd and polkit paths.
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    install(
        TARGETS OpenPuzzleGpuControlHelper
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT GpuControls
    )
    install(
        FILES ${CMAKE_CURRENT_LIST_DIR}/../packaging/polkit/org.openpuzzle.gpu-control.policy
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/polkit-1/actions
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        COMPONENT GpuControls
    )
    install(
        PROGRAMS ${CMAKE_CURRENT_LIST_DIR}/../scripts/install-gpu-control-helper.sh
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/scripts
        COMPONENT GpuControls
    )
    install(
        FILES ${CMAKE_CURRENT_LIST_DIR}/../docs/GPU_CONTROLS.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs
        COMPONENT GpuControls
    )
endif()
