# Read and extract only; never run dpkg --install or maintainer scripts.
foreach(required DEB_PATH EXTRACT_DIR LIBDIR INTEGRATION_TEST DIRECT_TEST)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing ${required}")
    endif()
endforeach()
find_program(DPKG_DEB dpkg-deb REQUIRED)
# Use a fresh extraction tree so a previous package cannot mask missing files.
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef validation_id)
set(EXTRACT_DIR "${EXTRACT_DIR}/run-${validation_id}")
file(MAKE_DIRECTORY "${EXTRACT_DIR}")

function(check_command)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result
        OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Validation command failed: ${ARGN}\n${output}\n${error}")
    endif()
    set(command_output "${output}" PARENT_SCOPE)
endfunction()

check_command("${DPKG_DEB}" --field "${DEB_PATH}" Package)
string(STRIP "${command_output}" package_name)
if(NOT package_name STREQUAL "fcitx5-openkey")
    message(FATAL_ERROR "Unexpected package: ${package_name}")
endif()
check_command("${DPKG_DEB}" --field "${DEB_PATH}" Depends)
foreach(dependency fcitx5 libc6 libstdc++)
    string(FIND "${command_output}" "${dependency}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing runtime dependency: ${dependency}")
    endif()
endforeach()
message(STATUS "Package dependencies: ${command_output}")

check_command("${DPKG_DEB}" --extract "${DEB_PATH}" "${EXTRACT_DIR}/root")
check_command("${DPKG_DEB}" --control "${DEB_PATH}" "${EXTRACT_DIR}/control")
foreach(script preinst postinst prerm postrm)
    if(EXISTS "${EXTRACT_DIR}/control/${script}")
        message(FATAL_ERROR "Unexpected maintainer script: ${script}")
    endif()
endforeach()
set(prefix "${EXTRACT_DIR}/root/usr")
foreach(expected "${LIBDIR}/fcitx5/openkey.so"
        share/fcitx5/addon/openkey.conf
        share/fcitx5/inputmethod/openkey-telex.conf
        share/fcitx5/inputmethod/openkey-vni.conf
        share/doc/fcitx5-openkey/copyright
        share/doc/fcitx5-openkey/README.md
        share/doc/fcitx5-openkey/INSTALL.md
        share/doc/fcitx5-openkey/COMMIT_ONLY.md
        share/doc/fcitx5-openkey/NATIVE_DIRECT.md)
    if(NOT EXISTS "${prefix}/${expected}")
        message(FATAL_ERROR "Package is missing ${expected}")
    endif()
endforeach()
file(GLOB_RECURSE packaged_files LIST_DIRECTORIES false RELATIVE "${EXTRACT_DIR}/root" "${EXTRACT_DIR}/root/*")
list(LENGTH packaged_files file_count)
if(NOT file_count EQUAL 9)
    message(FATAL_ERROR "Unexpected package payload (${file_count} files): ${packaged_files}")
endif()

check_command("${INTEGRATION_TEST}" "${prefix}" "${LIBDIR}")
message(STATUS "${command_output}")
check_command("${DIRECT_TEST}" "${prefix}/${LIBDIR}/fcitx5/openkey.so")
message(STATUS "${command_output}")
message(STATUS "DEB payload, dependencies and extracted-addon integration passed")
