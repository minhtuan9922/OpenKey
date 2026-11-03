cmake_minimum_required(VERSION 3.16)

project(OpenKey VERSION 3.1.0 LANGUAGES CXX)

option(OPENKEY_BUILD_SMOKE_TESTS "Build small Linux engine smoke tests" ON)
option(OPENKEY_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(OPENKEY_BUILD_FCITX5 "Build the experimental Fcitx5 input method" OFF)
option(OPENKEY_BUILD_DEB "Enable experimental Debian package generation" OFF)
option(OPENKEY_BUILD_SERVICE "Build the Linux session-service foundation" OFF)
option(OPENKEY_BUILD_X11_HOTKEY "Build opt-in X11 settings hotkey (not Wayland)" OFF)
option(OPENKEY_BUILD_UI "Build the optional Qt Quick settings preview" OFF)

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
        # The shared legacy engine still uses std::wstring_convert. Keep the
        # deprecation visible while treating every other warning as an error.
        target_compile_options(openkey_engine PRIVATE -Werror -Wno-error=deprecated-declarations)
    endif()
endif()

if(OPENKEY_BUILD_SMOKE_TESTS)
    enable_testing()

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
