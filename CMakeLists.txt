cmake_minimum_required(VERSION 3.20)
project(HumTrace VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HUMTRACE_BUILD_TESTS "Build unit and integration tests" ON)
option(HUMTRACE_BUILD_GUI "Build the Qt 6 Widgets desktop application" OFF)
option(HUMTRACE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
set(HUMTRACE_SANITIZE "" CACHE STRING "Sanitizers to enable, e.g. address,undefined")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(humtrace_warnings INTERFACE)
if(MSVC)
    target_compile_options(humtrace_warnings INTERFACE /W4 $<$<BOOL:${HUMTRACE_WARNINGS_AS_ERRORS}>:/WX>)
else()
    target_compile_options(humtrace_warnings INTERFACE -Wall -Wextra -Wpedantic -Wconversion
                           $<$<BOOL:${HUMTRACE_WARNINGS_AS_ERRORS}>:-Werror>)
endif()
if(HUMTRACE_SANITIZE)
    target_compile_options(humtrace_warnings INTERFACE -fsanitize=${HUMTRACE_SANITIZE} -fno-omit-frame-pointer -g)
    target_link_options(humtrace_warnings INTERFACE -fsanitize=${HUMTRACE_SANITIZE})
endif()

# Build metadata recorded in reports.
find_package(Git QUIET)
set(HUMTRACE_GIT_COMMIT "unknown")
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE HUMTRACE_GIT_COMMIT
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT HUMTRACE_GIT_COMMIT)
        set(HUMTRACE_GIT_COMMIT "unknown")
    endif()
endif()
configure_file(cmake/version.hpp.in ${CMAKE_BINARY_DIR}/generated/humtrace/version.hpp @ONLY)

# Dependency-free analysis core: every .cpp under src/core is part of it.
file(GLOB_RECURSE HUMTRACE_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/core/*.cpp)
add_library(humtrace_core STATIC ${HUMTRACE_CORE_SOURCES})
target_include_directories(humtrace_core PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(humtrace_core PRIVATE humtrace_warnings)
find_package(Threads REQUIRED)
target_link_libraries(humtrace_core PUBLIC Threads::Threads)

file(GLOB HUMTRACE_CLI_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/cli/*.cpp)
add_executable(humtrace ${HUMTRACE_CLI_SOURCES})
target_link_libraries(humtrace PRIVATE humtrace_core humtrace_warnings)
if(MINGW)
    target_link_options(humtrace PRIVATE -static)
endif()
install(TARGETS humtrace RUNTIME DESTINATION bin)

if(HUMTRACE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HUMTRACE_BUILD_GUI)
    add_subdirectory(app)
endif()

# libFuzzer targets (clang only); see fuzz/CMakeLists.txt.
option(HUMTRACE_BUILD_FUZZERS "Build libFuzzer targets (requires clang)" OFF)
if(HUMTRACE_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()
