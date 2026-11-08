cmake_minimum_required(VERSION 3.16)

project(OpenKey VERSION 3.1.0 LANGUAGES CXX)

option(OPENKEY_BUILD_SMOKE_TESTS "Build small Linux engine smoke tests" ON)
option(OPENKEY_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(OPENKEY_BUILD_FCITX5 "Build the experimental Fcitx5 input method" OFF)
option(OPENKEY_BUILD_DEB "Enable experimental Debian package generation" OFF)
option(OPENKEY_BUILD_SERVICE "Build the Linux session-service foundation" OFF)
option(OPENKEY_BUILD_X11_HOTKEY "Build opt-in X11 settings hotkey (not Wayland)" OFF)
option(OPENKEY_BUILD_UI "Build the optional Qt Quick settings preview" OFF)
option(OPENKEY_BUILD_UI_DEB "Package the standalone settings UI as a separate Debian package" OFF)
option(OPENKEY_BUILD_BENCHMARKS "Build the standalone replay microbenchmark" OFF)
if(OPENKEY_BUILD_UI_DEB AND (NOT OPENKEY_BUILD_UI OR OPENKEY_BUILD_DEB OR OPENKEY_BUILD_FCITX5 OR OPENKEY_BUILD_SERVICE))
    message(FATAL_ERROR "UI DEB requires UI=ON and DEB/FCITX5/SERVICE=OFF; use a separate build directory")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(OPENKEY_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Sources/OpenKey/engine")

add_library(openkey_engine STATIC
    Sources/OpenKey/engine/ConvertTool.cpp
    Sources/OpenKey/engine/Engine.cpp
    Sources/OpenKey/engine/Macro.cpp
    Sources/OpenKey/engine/SmartSwitchKey.cpp
    Sources/OpenKey/engine/Vietnamese.cpp
    Sources/OpenKey/linux/EngineConfig.cpp
)

target_compile_definitions(openkey_engine PUBLIC LINUX=1)
target_include_directories(openkey_engine PUBLIC "${OPENKEY_ENGINE_DIR}")
set_target_properties(openkey_engine PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

if(MSVC)
    target_compile_options(openkey_engine PRIVATE /W4)
else()
    target_compile_options(openkey_engine PRIVATE -Wall -Wextra -Wpedantic)
    if(OPENKEY_WARNINGS_AS_ERRORS)
        target_compile_options(openkey_engine PRIVATE -Werror)
    endif()
endif()

if(OPENKEY_BUILD_SMOKE_TESTS)
    enable_testing()
    add_executable(openkey_utf8_tests Tests/LinuxUtf8Tests.cpp)
    target_link_libraries(openkey_utf8_tests PRIVATE openkey_engine)
    if(NOT MSVC)
        target_compile_options(openkey_utf8_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME openkey_utf8_tests COMMAND openkey_utf8_tests)

    add_executable(openkey_engine_smoke
        Tests/LinuxEngineSmoke.cpp
    )
    target_link_libraries(openkey_engine_smoke PRIVATE openkey_engine)

    add_executable(openkey_engine_regression
        Tests/LinuxEngineRegressionTests.cpp
    )
    target_link_libraries(openkey_engine_regression PRIVATE openkey_engine)

    if(NOT MSVC AND OPENKEY_WARNINGS_AS_ERRORS)
        target_compile_options(openkey_engine_smoke PRIVATE -Wall -Wextra -Wpedantic -Werror)
        target_compile_options(openkey_engine_regression PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME openkey_engine_smoke COMMAND openkey_engine_smoke)
    add_test(NAME openkey_engine_regression COMMAND openkey_engine_regression)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            add_test(NAME openkey_linux_compat_report_tests
                COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/LinuxCompatReportTests.py")
            set_tests_properties(openkey_linux_compat_report_tests PROPERTIES TIMEOUT 20)
        else()
            message(STATUS "Python3 not found: Linux compatibility report tests unavailable")
        endif()
        find_program(OPENKEY_TEST_BASH NAMES bash)
        if(NOT OPENKEY_TEST_BASH)
            message(FATAL_ERROR "Linux launcher tests require bash")
        endif()
        add_test(NAME openkey_vscode_launcher_tests COMMAND "${OPENKEY_TEST_BASH}"
            "${PROJECT_SOURCE_DIR}/Tests/LinuxVscodeLauncherTests.sh" "${PROJECT_SOURCE_DIR}")
        set_tests_properties(openkey_vscode_launcher_tests PROPERTIES TIMEOUT 20)
    endif()
endif()

if(OPENKEY_BUILD_BENCHMARKS)
    add_executable(openkey_replay_benchmark Tests/LinuxReplayBenchmark.cpp)
    target_include_directories(openkey_replay_benchmark PRIVATE Sources/OpenKey/linux/fcitx5)
    target_link_libraries(openkey_replay_benchmark PRIVATE openkey_engine)
endif()

if(OPENKEY_BUILD_FCITX5)
    add_subdirectory(Sources/OpenKey/linux/fcitx5)
endif()

if(OPENKEY_BUILD_DEB)
    include(cmake/LinuxDeb.cmake)
endif()

if(OPENKEY_BUILD_SERVICE)
    add_subdirectory(Sources/OpenKey/linux/service)
endif()
if(OPENKEY_BUILD_UI)
    add_subdirectory(Sources/OpenKey/linux/ui)
endif()
if(OPENKEY_BUILD_UI_DEB)
    include(cmake/LinuxUiDeb.cmake)
endif()
