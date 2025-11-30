cmake_minimum_required(VERSION 3.16)
project(eq LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Keep PIC OFF for executables by default to reduce overhead
option(DISABLE_PIC_EXECUTABLES "Disable PIC/PIE for executables (better perf)" ON)
# High-level opts
option(ENABLE_AVX2         "Build AVX2 4-way targets" ON)
option(ENABLE_LTO          "Enable interprocedural optimization (LTO/IPO)" ON)
option(ENABLE_PGO_GENERATE "Build with PGO instrumentation (-fprofile-generate)" OFF)
option(ENABLE_PGO_USE      "Build using PGO profiles (-fprofile-use)" OFF)
option(USE_LLD             "Use lld linker if available (-fuse-ld=lld)" OFF)
option(PROFILE_RSS         "Enable peak RSS measurement macro" ON)
option(DISABLE_EXCEPTIONS  "Compile with -fno-exceptions (if code allows)" OFF)
option(DISABLE_RTTI        "Compile with -fno-rtti (if code allows)" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Prefer explicit IPO across the project if requested
if(ENABLE_LTO)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  # For GCC: enable parallel LTO jobs to avoid serial compilation warning
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-flto=auto)
    add_link_options(-flto=auto)
  endif()
endif()

# If using GCC, wire gcc-ar/gcc-ranlib so LTO archives work reliably
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  find_program(GCC_AR NAMES gcc-ar)
  find_program(GCC_RANLIB NAMES gcc-ranlib)
  if(GCC_AR)
    set(CMAKE_C_COMPILER_AR   "${GCC_AR}" CACHE FILEPATH "" FORCE)
    set(CMAKE_CXX_COMPILER_AR "${GCC_AR}" CACHE FILEPATH "" FORCE)
  endif()
  if(GCC_RANLIB)
    set(CMAKE_C_COMPILER_RANLIB   "${GCC_RANLIB}" CACHE FILEPATH "" FORCE)
    set(CMAKE_CXX_COMPILER_RANLIB "${GCC_RANLIB}" CACHE FILEPATH "" FORCE)
  endif()
endif()

# Base compile options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(
    -Ofast -march=native -mtune=native -funroll-loops
    -Wall -Wextra -Wno-unused-variable
    -ffast-math -fomit-frame-pointer -fstrict-aliasing
    -fdata-sections -ffunction-sections -falign-functions=32 -fno-plt
    -fno-asynchronous-unwind-tables -fno-unwind-tables
  )
endif()

# Optional: disable exceptions/RTTI
if(DISABLE_EXCEPTIONS)
  add_compile_options(-fno-exceptions)
endif()
if(DISABLE_RTTI)
  add_compile_options(-fno-rtti)
endif()

# Prefer lld if requested and available
if(USE_LLD)
  # Both GCC and Clang accept this when lld is installed
  add_link_options(-fuse-ld=lld)
endif()

# Include dirs
include_directories(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/third_party/blake
  ${CMAKE_SOURCE_DIR}/third_party/blake2-avx2
  ${CMAKE_SOURCE_DIR}/third_party/kxsort
)

# Sources
set(SRC_COMMON
  third_party/blake/blake2b.cpp
)

set(SRC_200_9
  src/eq200_9/apr_main.cpp
  src/eq200_9/apr_alg.cpp
  ${SRC_COMMON}
)

set(SRC_144_5
  src/eq144_5/apr_main.cpp
  src/eq144_5/apr_alg.cpp
  ${SRC_COMMON}
)
set(SRC_AVX
  third_party/blake2-avx2/blake2bip.c
)

# Common post-create target tweaks
function(apply_common_opts tgt)
  target_link_libraries(${tgt} PRIVATE m)
  # Link-time GC of unused sections
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_link_options(${tgt} PRIVATE -Wl,--gc-sections)
    # Optional: increase linker optimization
    target_link_options(${tgt} PRIVATE -Wl,-O3)
  endif()
  # Disable PIC/PIE for executables if desired
  if(DISABLE_PIC_EXECUTABLES)
    set_target_properties(${tgt} PROPERTIES POSITION_INDEPENDENT_CODE OFF)
  endif()
  # PGO generate/use
  if(ENABLE_PGO_GENERATE)
    target_compile_options(${tgt} PRIVATE -fprofile-generate)
    target_link_options(${tgt} PRIVATE -fprofile-generate)
  endif()
  if(ENABLE_PGO_USE)
    target_compile_options(${tgt} PRIVATE -fprofile-use -fauto-profile)
    target_link_options(${tgt} PRIVATE -fprofile-use)
  endif()
  if(PROFILE_RSS)
    target_compile_definitions(${tgt} PRIVATE PROFILE_RSS=1)
  endif()
endfunction()


# In-place merge benchmark executable
add_executable(inplace_bench src/benchmarks/inplace_merge_benchmark.cpp third_party/blake/blake2b.cpp)
apply_common_opts(inplace_bench)

# convenience target to build inplace_bench
add_custom_target(build_inplace_bench
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target inplace_bench
  DEPENDS inplace_bench
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Build inplace_bench target"
)

# 1) Baseline (no AVX2 4-way blake)
add_executable(apr_200_9 ${SRC_200_9})
apply_common_opts(apr_200_9)
add_executable(apr ALIAS apr_200_9)

# Equihash (144,5) variant
add_executable(apr_144_5 ${SRC_144_5})
apply_common_opts(apr_144_5)

# 2) 4-way AVX2 single-thread (equix41)
# only support NBLAKES=4, using blake2bx4 implementation
if(ENABLE_AVX2)
  add_executable(apr_x41 ${SRC_200_9} ${SRC_AVX})
  target_compile_definitions(apr_x41 PRIVATE NBLAKES=4)
  target_compile_options(apr_x41 PRIVATE -mavx2 -mfma)
  apply_common_opts(apr_x41)
endif()

# Benchmark target
add_custom_target(benchmark_apr
  COMMAND ${CMAKE_BINARY_DIR}/apr --mode=cip --iters=5 --seed=42 --verbose
  DEPENDS apr
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running APR performance benchmark..."
)

# =============================================================================
# Testing Support
# =============================================================================
enable_testing()

# Test sources
set(TEST_COMMON
  third_party/blake/blake2b.cpp
)

# Test: test_iv - Basic IndexVector tests
add_executable(test_iv test/test_iv.cpp ${TEST_COMMON})
apply_common_opts(test_iv)
add_test(NAME test_iv COMMAND test_iv)

# Test: test_layer_iv - Layer IV key verification and sort consistency
add_executable(test_layer_iv test/test_layer_iv.cpp ${TEST_COMMON})
apply_common_opts(test_layer_iv)
add_test(NAME test_layer_iv COMMAND test_layer_iv)

# Test: check_size - Check IndexVector size consistency
add_executable(check_size test/check_size.cpp ${TEST_COMMON})
apply_common_opts(check_size)
add_test(NAME check_size COMMAND check_size)

# Test: verify_size - Verify sizeof == TotalBytes for IndexVector
add_executable(verify_size test/verify_size.cpp ${TEST_COMMON})
apply_common_opts(verify_size)
add_test(NAME verify_size COMMAND verify_size)

# Test: test_iv_optimization - Verify IV merge correctness and performance
add_executable(test_iv_optimization test/test_iv_optimization.cpp ${TEST_COMMON})
apply_common_opts(test_iv_optimization)
add_test(NAME test_iv_optimization COMMAND test_iv_optimization)

# Test: test_setivkey_perf - Measure SetIV_Key performance vs fill_layer0
add_executable(test_setivkey_perf test/test_setivkey_perf.cpp ${TEST_COMMON})
apply_common_opts(test_setivkey_perf)
add_test(NAME test_setivkey_perf COMMAND test_setivkey_perf)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER}")
message(STATUS "ENABLE_AVX2=${ENABLE_AVX2}, ENABLE_LTO=${ENABLE_LTO}, USE_LLD=${USE_LLD}")
message(STATUS "PGO generate=${ENABLE_PGO_GENERATE}, PGO use=${ENABLE_PGO_USE}")
message(STATUS "PROFILE_RSS=${PROFILE_RSS}, DISABLE_PIC_EXECUTABLES=${DISABLE_PIC_EXECUTABLES}")