cmake_minimum_required(VERSION 3.20)
project(Ashfall VERSION 0.1.0 LANGUAGES C CXX)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
endif()

option(ASHFALL_BUILD_GAME "Build the Raylib application" ON)
option(ASHFALL_BUILD_TESTS "Build display-independent simulation tests" ON)
option(ASHFALL_FETCH_RAYLIB "Fetch pinned Raylib when no installed package is found" ON)
option(ASHFALL_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
option(ASHFALL_WEB_TEST_API "Expose read-only browser diagnostics for smoke tests" OFF)

add_library(ashfall_core
    src/grid.cpp
    src/pathfinding.cpp
    src/equipment.cpp
    src/player.cpp
    src/combat.cpp
    src/world.cpp
)
target_include_directories(ashfall_core PUBLIC src)
if(MSVC)
    target_compile_options(ashfall_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(ashfall_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
if(ASHFALL_SANITIZERS AND NOT EMSCRIPTEN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ashfall_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(ashfall_core PUBLIC -fsanitize=address,undefined)
endif()

if(ASHFALL_BUILD_GAME)
    if(EMSCRIPTEN)
        set(PLATFORM Web CACHE STRING "Raylib platform" FORCE)
        set(GRAPHICS GRAPHICS_API_OPENGL_ES2 CACHE STRING "Raylib graphics API" FORCE)
    endif()
    find_package(raylib 5.5 QUIET)
    if(NOT raylib_FOUND)
        if(NOT ASHFALL_FETCH_RAYLIB)
            message(FATAL_ERROR "Raylib 5.5 is required. Install it or enable ASHFALL_FETCH_RAYLIB.")
        endif()
        include(FetchContent)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
        set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
        set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)
        set(SUPPORT_MODULE_RMODELS OFF CACHE BOOL "" FORCE)
        # Raylib 5.5 still declares compatibility with CMake 3.5.
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
        FetchContent_Declare(raylib
            URL https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/5.5
            URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e
        )
        FetchContent_MakeAvailable(raylib)
    endif()
    add_executable(ashfall src/main.cpp src/renderer.cpp)
    target_link_libraries(ashfall PRIVATE ashfall_core raylib)
    if(MSVC)
        target_compile_options(ashfall PRIVATE /W4 /permissive-)
    else()
        target_compile_options(ashfall PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    if(EMSCRIPTEN)
        set_target_properties(ashfall PROPERTIES OUTPUT_NAME index SUFFIX .html)
        target_link_options(ashfall PRIVATE
            "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
            -sUSE_GLFW=3
            -sALLOW_MEMORY_GROWTH=1
            -sINITIAL_MEMORY=33554432
            -sSTACK_SIZE=1048576
            -sENVIRONMENT=web
            -sFILESYSTEM=0
            -sEXIT_RUNTIME=0
            "$<$<CONFIG:Debug>:-sASSERTIONS=2>"
        )
        set_property(TARGET ashfall APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
        if(ASHFALL_WEB_TEST_API)
            target_compile_definitions(ashfall PRIVATE ASHFALL_WEB_TEST_API=1)
            target_link_options(ashfall PRIVATE "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString']")
        endif()
    endif()
endif()

if(ASHFALL_BUILD_TESTS AND NOT EMSCRIPTEN)
    enable_testing()
    add_executable(ashfall_tests tests/core_tests.cpp)
    target_link_libraries(ashfall_tests PRIVATE ashfall_core)
    add_test(NAME ashfall_core COMMAND ashfall_tests)
endif()
