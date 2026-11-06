if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT OPENKEY_BUILD_FCITX5)
    message(FATAL_ERROR "DEB packaging requires Linux and OPENKEY_BUILD_FCITX5=ON")
endif()
if(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    message(FATAL_ERROR "Configure DEB builds with -DCMAKE_INSTALL_PREFIX=/usr")
endif()
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}" OR IS_ABSOLUTE "${CMAKE_INSTALL_DATADIR}")
    message(FATAL_ERROR "DEB packaging requires relative install directories")
endif()

find_program(OPENKEY_DPKG dpkg REQUIRED)
find_program(OPENKEY_SHLIBDEPS dpkg-shlibdeps REQUIRED)
execute_process(COMMAND "${OPENKEY_DPKG}" --print-architecture
    OUTPUT_VARIABLE OPENKEY_DEB_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE arch_result)
if(NOT arch_result EQUAL 0 OR NOT OPENKEY_DEB_ARCH)
    message(FATAL_ERROR "Could not determine Debian architecture")
endif()
if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "DEB packaging currently supports native builds only")
endif()

install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
    DESTINATION share/doc/fcitx5-openkey RENAME copyright COMPONENT openkey_fcitx5)
install(FILES "${PROJECT_SOURCE_DIR}/Sources/OpenKey/linux/README.md"
    DESTINATION share/doc/fcitx5-openkey RENAME README.md COMPONENT openkey_fcitx5)
install(FILES "${PROJECT_SOURCE_DIR}/docs/UBUNTU_INSTALL.md"
    DESTINATION share/doc/fcitx5-openkey RENAME INSTALL.md COMPONENT openkey_fcitx5)
install(FILES "${PROJECT_SOURCE_DIR}/docs/UBUNTU_COMMIT_ONLY.md"
    DESTINATION share/doc/fcitx5-openkey RENAME COMMIT_ONLY.md COMPONENT openkey_fcitx5)
install(FILES "${PROJECT_SOURCE_DIR}/docs/UBUNTU_NATIVE_DIRECT.md"
    DESTINATION share/doc/fcitx5-openkey RENAME NATIVE_DIRECT.md COMPONENT openkey_fcitx5)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME fcitx5-openkey)
set(CPACK_PACKAGE_VENDOR "OpenKey contributors")
set(CPACK_PACKAGE_CONTACT "TuanPham <minhtuan9922@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Experimental OpenKey Vietnamese input method for Fcitx5")
set(CPACK_PACKAGE_DESCRIPTION "OpenKey engine by Mai Vu Tuyen, with experimental Linux integration.\n Telex and VNI with Preedit, Direct, CommitOnly, or NativeDirect input.\n Settings UI is packaged separately. Installation does not change input-method\n profiles or restart Fcitx5.")
set(CPACK_PACKAGE_VERSION "3.1.0~alpha.47")
set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${OPENKEY_DEB_ARCH}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
set(CPACK_DEBIAN_PACKAGE_PRIORITY optional)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/minhtuan9922/OpenKey")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "fcitx5 (>= 5.0.14)")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "fcitx5-config-qt")
# Derive libc/libstdc++/Fcitx ABI requirements from the actual ELF, not from a
# handwritten list that could incorrectly permit older Ubuntu installations.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_COMPRESSION_TYPE xz)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_SOURCE_DIR}/dist/linux")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_STRIP_FILES ON)
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
include(CPack)

if(OPENKEY_BUILD_SMOKE_TESTS)
    add_custom_target(verify_linux_deb
        COMMAND "${CMAKE_COMMAND}"
            "-DDEB_PATH=${CPACK_PACKAGE_DIRECTORY}/fcitx5-openkey_${CPACK_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE}_${OPENKEY_DEB_ARCH}.deb"
            "-DEXTRACT_DIR=${PROJECT_BINARY_DIR}/deb-validation"
            "-DLIBDIR=${CMAKE_INSTALL_LIBDIR}"
            "-DINTEGRATION_TEST=$<TARGET_FILE:openkey_fcitx5_integration_tests>"
            "-DDIRECT_TEST=$<TARGET_FILE:openkey_direct_tests>"
            -P "${PROJECT_SOURCE_DIR}/cmake/VerifyLinuxDeb.cmake"
        DEPENDS openkey_fcitx5_integration_tests openkey_direct_tests
        VERBATIM)
endif()
