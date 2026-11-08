if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT OPENKEY_BUILD_UI)
    message(FATAL_ERROR "UI DEB requires Linux and OPENKEY_BUILD_UI=ON")
endif()
if(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    message(FATAL_ERROR "Configure UI DEB builds with -DCMAKE_INSTALL_PREFIX=/usr")
endif()
if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "UI DEB currently supports native builds only")
endif()
find_program(OPENKEY_UI_DPKG dpkg REQUIRED)
find_program(OPENKEY_UI_SHLIBDEPS dpkg-shlibdeps REQUIRED)
find_program(OPENKEY_UI_DESKTOP_VALIDATOR desktop-file-validate REQUIRED)
execute_process(COMMAND "${OPENKEY_UI_DPKG}" --print-architecture
    OUTPUT_VARIABLE OPENKEY_UI_DEB_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE arch_result)
if(NOT arch_result EQUAL 0 OR NOT OPENKEY_UI_DEB_ARCH)
    message(FATAL_ERROR "Could not determine Debian architecture")
endif()
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
    DESTINATION share/doc/openkey-settings RENAME copyright COMPONENT openkey_ui)
install(FILES "${PROJECT_SOURCE_DIR}/Sources/OpenKey/linux/README.md"
    DESTINATION share/doc/openkey-settings RENAME README.md COMPONENT openkey_ui)
install(FILES "${PROJECT_SOURCE_DIR}/docs/UBUNTU_UI.md"
    DESTINATION share/doc/openkey-settings RENAME UI.md COMPONENT openkey_ui)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME openkey-settings)
set(CPACK_PACKAGE_VENDOR "OpenKey contributors")
set(CPACK_PACKAGE_CONTACT "TuanPham <minhtuan9922@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Experimental Qt Quick settings UI for OpenKey")
set(CPACK_PACKAGE_DESCRIPTION "Standalone settings preview for OpenKey on Linux.\n The Fcitx5 addon and session tray are installed separately.\n Installation does not change input-method profiles, enable autostart,\n or restart Fcitx5. Qt and QML runtime dependencies are system packages.")
set(CPACK_PACKAGE_VERSION "3.1.0~preview.12")
set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${OPENKEY_UI_DEB_ARCH}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
set(CPACK_DEBIAN_PACKAGE_PRIORITY optional)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/minhtuan9922/OpenKey")
# Dynamic QML imports are not ELF dependencies; list these explicitly.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-window, qml6-module-qtquick-templates, qml6-module-qtqml-workerscript, qml6-module-qtquick-dialogs")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "fcitx5-openkey")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_COMPRESSION_TYPE xz)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_SOURCE_DIR}/dist/linux")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_STRIP_FILES ON)
# Explicitly stage only this component, even if other install rules are added.
set(CPACK_INSTALL_CMAKE_PROJECTS "${PROJECT_BINARY_DIR};${PROJECT_NAME};openkey_ui;/")
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
include(CPack)
add_custom_target(verify_linux_ui_deb
    COMMAND "${CMAKE_COMMAND}"
        "-DDEB_PATH=${CPACK_PACKAGE_DIRECTORY}/openkey-settings_${CPACK_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE}_${OPENKEY_UI_DEB_ARCH}.deb"
        "-DEXTRACT_DIR=${PROJECT_BINARY_DIR}/ui-deb-validation"
        -P "${PROJECT_SOURCE_DIR}/cmake/VerifyLinuxUiDeb.cmake"
    DEPENDS openkey-settings
    VERBATIM)
add_custom_target(verify_linux_ui_deb_lifecycle
    COMMAND "${CMAKE_COMMAND}"
        "-DDEB_PATH=${CPACK_PACKAGE_DIRECTORY}/openkey-settings_${CPACK_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE}_${OPENKEY_UI_DEB_ARCH}.deb"
        "-DSANDBOX_PARENT=${PROJECT_BINARY_DIR}/ui-deb-lifecycle"
        -P "${PROJECT_SOURCE_DIR}/cmake/TestLinuxUiDebLifecycle.cmake"
    DEPENDS openkey-settings
    VERBATIM)
