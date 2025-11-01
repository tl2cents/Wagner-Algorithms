cmake_minimum_required(VERSION 3.16)
project(eq LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Release)

add_compile_options(-Ofast -march=native -mavx2 -funroll-loops -Wall -Wextra -Wno-unused-variable)

include_directories(
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party/blake
        ${CMAKE_SOURCE_DIR}/third_party/blake2-avx2
        ${CMAKE_SOURCE_DIR}/third_party/kxsort
)

set(SRC_FILES
        src/apr_main.cpp
        src/apr_alg.cpp
        third_party/blake/blake2b.cpp
        third_party/blake2-avx2/blake2bip.c
)

add_executable(apr ${SRC_FILES})
target_link_libraries(apr PRIVATE m)

option(PROFILE_RSS "Enable peak RSS measurement" ON)
if(PROFILE_RSS)
    target_compile_definitions(apr PRIVATE PROFILE_RSS=1)
endif()

add_custom_target(benchmark_apr
        COMMAND ${CMAKE_BINARY_DIR}/apr --mode=cip --iters=5 --seed=42 --verbose
        DEPENDS apr
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running APR performance benchmark..."
)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "RSS profiling: ${PROFILE_RSS}")
