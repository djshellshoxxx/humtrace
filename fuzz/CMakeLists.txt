# libFuzzer targets. Requires clang: cmake -DCMAKE_CXX_COMPILER=clang++ -DHUMTRACE_BUILD_FUZZERS=ON
# The parser sources are compiled into each target so they carry coverage
# instrumentation.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "HUMTRACE_BUILD_FUZZERS requires clang (libFuzzer)")
endif()

add_executable(fuzz_wav fuzz_wav.cpp ${CMAKE_SOURCE_DIR}/src/core/wav.cpp)
target_include_directories(fuzz_wav PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(fuzz_wav PRIVATE humtrace_warnings)
target_compile_options(fuzz_wav PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -g)
target_link_options(fuzz_wav PRIVATE -fsanitize=fuzzer,address,undefined)
