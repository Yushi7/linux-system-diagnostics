cmake_minimum_required(VERSION 3.20)

project(linux_system_diagnostics VERSION 0.1.0 LANGUAGES CXX)

add_library(sysdiag
    src/cli.cpp
    src/cpu.cpp
    src/memory.cpp
    src/process.cpp
)

target_include_directories(sysdiag PUBLIC include)
target_compile_features(sysdiag PUBLIC cxx_std_20)
set_target_properties(sysdiag PROPERTIES CXX_EXTENSIONS OFF)

function(enable_project_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
    )
endfunction()

enable_project_warnings(sysdiag)

add_executable(linux-sysdiag src/main.cpp)
target_link_libraries(linux-sysdiag PRIVATE sysdiag)
set_target_properties(linux-sysdiag PROPERTIES CXX_EXTENSIONS OFF)
enable_project_warnings(linux-sysdiag)

add_executable(linux-sysdiag-benchmark benchmarks/benchmark.cpp)
target_link_libraries(linux-sysdiag-benchmark PRIVATE sysdiag)
set_target_properties(linux-sysdiag-benchmark PROPERTIES CXX_EXTENSIONS OFF)
enable_project_warnings(linux-sysdiag-benchmark)

include(CTest)
if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)

    add_executable(linux-sysdiag-tests tests/test_sysdiag.cpp)
    target_link_libraries(linux-sysdiag-tests PRIVATE sysdiag GTest::gtest_main)
    set_target_properties(linux-sysdiag-tests PROPERTIES CXX_EXTENSIONS OFF)
    enable_project_warnings(linux-sysdiag-tests)

    include(GoogleTest)
    gtest_discover_tests(linux-sysdiag-tests)
endif()