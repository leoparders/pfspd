cmake_minimum_required(VERSION 3.0)

configure_file(CMakeLists.txt.in googlecrc32c-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"  .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googlecrc32c-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build  .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googlecrc32c-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.

set(CRC32C_BUILD_TESTS OFF CACHE INTERNAL  "CRC32C_BUILD_TESTS")
set(CRC32C_BUILD_BENCHMARKS OFF CACHE INTERNAL  "CRC32C_BUILD_BENCHMARKS")
set(CRC32C_USE_GLOG OFF CACHE INTERNAL  "CRC32C_USE_GLOG")
set(CRC32C_INSTALL OFF CACHE INTERNAL  "CRC32C_INSTALL")
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googlecrc32c-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googlecrc32c-build
                 EXCLUDE_FROM_ALL)