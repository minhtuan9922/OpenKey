find_package(Fcitx5Core 5.0.14 REQUIRED)
include(GNUInstallDirs)

add_library(openkey_fcitx5 MODULE OpenKeyAddon.cpp)
set_target_properties(openkey_fcitx5 PROPERTIES OUTPUT_NAME openkey PREFIX ""
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
target_link_options(openkey_fcitx5 PRIVATE -Wl,--no-undefined)
target_link_libraries(openkey_fcitx5 PRIVATE openkey_engine Fcitx5::Core)
target_compile_features(openkey_fcitx5 PRIVATE cxx_std_20)
target_compile_options(openkey_fcitx5 PRIVATE -Wall -Wextra -Wpedantic)
if(OPENKEY_WARNINGS_AS_ERRORS)
    target_compile_options(openkey_fcitx5 PRIVATE -Werror)
endif()

install(TARGETS openkey_fcitx5 DESTINATION ${CMAKE_INSTALL_LIBDIR}/fcitx5 COMPONENT openkey_fcitx5)
install(FILES openkey-addon.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/addon
    RENAME openkey.conf COMPONENT openkey_fcitx5)
install(FILES openkey-telex.conf openkey-vni.conf openkey-simple-telex1.conf openkey-simple-telex2.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod
    COMPONENT openkey_fcitx5)

if(OPENKEY_BUILD_SMOKE_TESTS)
    find_program(OPENKEY_GNOME_DBUS_RUN_SESSION dbus-run-session REQUIRED)
    find_program(OPENKEY_GNOME_NODE node)
    find_program(OPENKEY_GNOME_GJS gjs)
    if(OPENKEY_GNOME_GJS)
        add_test(NAME openkey_gnome_gio_tests COMMAND "${CMAKE_COMMAND}" -E env GIO_USE_VFS=local
            "${OPENKEY_GNOME_DBUS_RUN_SESSION}" -- "${OPENKEY_GNOME_GJS}" -m "${PROJECT_SOURCE_DIR}/Tests/LinuxGnomeGioTests.js")
        set_tests_properties(openkey_gnome_gio_tests PROPERTIES TIMEOUT 10)
    endif()
    if(OPENKEY_GNOME_NODE)
        add_test(NAME openkey_gnome_extension_tests COMMAND "${OPENKEY_GNOME_NODE}"
            "${PROJECT_SOURCE_DIR}/Tests/LinuxGnomeExtensionTests.mjs")
        set_tests_properties(openkey_gnome_extension_tests PROPERTIES TIMEOUT 20)
    endif()
    # --prefix cannot relocate absolute install destinations. Keep the CTest
    # staging fixture confined to the build directory even with custom flags.
    if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}" OR IS_ABSOLUTE "${CMAKE_INSTALL_DATADIR}")
        message(FATAL_ERROR "Fcitx5 tests require relative CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_DATADIR")
    endif()
    add_executable(openkey_engine_context_tests ${PROJECT_SOURCE_DIR}/Tests/LinuxEngineContextTests.cpp)
    target_include_directories(openkey_engine_context_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(openkey_engine_context_tests PRIVATE openkey_engine)
    target_compile_options(openkey_engine_context_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME openkey_engine_context_tests COMMAND openkey_engine_context_tests)
    add_executable(openkey_composition_tests ${PROJECT_SOURCE_DIR}/Tests/LinuxCompositionTests.cpp)
    target_include_directories(openkey_composition_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(openkey_composition_tests PRIVATE openkey_engine)
    target_compile_options(openkey_composition_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME openkey_composition_tests COMMAND openkey_composition_tests)

    add_executable(openkey_fcitx5_tests ${PROJECT_SOURCE_DIR}/Tests/LinuxFcitx5Tests.cpp)
    target_link_libraries(openkey_fcitx5_tests PRIVATE Fcitx5::Core ${CMAKE_DL_LIBS})
    add_dependencies(openkey_fcitx5_tests openkey_fcitx5)
    target_compile_features(openkey_fcitx5_tests PRIVATE cxx_std_20)
    target_compile_options(openkey_fcitx5_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME openkey_fcitx5_tests COMMAND openkey_fcitx5_tests $<TARGET_FILE:openkey_fcitx5>)
    set_tests_properties(openkey_fcitx5_tests PROPERTIES TIMEOUT 20)
    set_tests_properties(openkey_fcitx5_tests PROPERTIES ENVIRONMENT
        "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-cache")

    add_executable(openkey_direct_tests ${PROJECT_SOURCE_DIR}/Tests/LinuxDirectInputTests.cpp)
    target_include_directories(openkey_direct_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(openkey_direct_tests PRIVATE openkey_engine Fcitx5::Core ${CMAKE_DL_LIBS})
    target_compile_features(openkey_direct_tests PRIVATE cxx_std_20)
    target_compile_options(openkey_direct_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_dependencies(openkey_direct_tests openkey_fcitx5)
    add_test(NAME openkey_direct_tests COMMAND openkey_direct_tests $<TARGET_FILE:openkey_fcitx5>)
    set_tests_properties(openkey_direct_tests PROPERTIES TIMEOUT 20 ENVIRONMENT
        "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/direct-config;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/direct-data;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/direct-cache")

    add_executable(openkey_fcitx5_integration_tests ${PROJECT_SOURCE_DIR}/Tests/LinuxFcitx5IntegrationTests.cpp)
    target_include_directories(openkey_fcitx5_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(openkey_fcitx5_integration_tests PRIVATE openkey_engine Fcitx5::Core)
    target_compile_features(openkey_fcitx5_integration_tests PRIVATE cxx_std_20)
    target_compile_options(openkey_fcitx5_integration_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_dependencies(openkey_fcitx5_integration_tests openkey_fcitx5)
    set(OPENKEY_TEST_PREFIX "${PROJECT_BINARY_DIR}/fcitx5-test-install")
    add_test(NAME openkey_fcitx5_stage COMMAND ${CMAKE_COMMAND} --install ${PROJECT_BINARY_DIR}
        --prefix ${OPENKEY_TEST_PREFIX} --config $<CONFIG> --component openkey_fcitx5)
    set_tests_properties(openkey_fcitx5_stage PROPERTIES FIXTURES_SETUP openkey_installed TIMEOUT 20)
    add_test(NAME openkey_fcitx5_integration_tests COMMAND "${OPENKEY_GNOME_DBUS_RUN_SESSION}" -- $<TARGET_FILE:openkey_fcitx5_integration_tests>
        ${OPENKEY_TEST_PREFIX} ${CMAKE_INSTALL_LIBDIR})
    set_tests_properties(openkey_fcitx5_integration_tests PROPERTIES
        FIXTURES_REQUIRED openkey_installed TIMEOUT 20)
endif()
