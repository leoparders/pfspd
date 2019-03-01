cmake_minimum_required(VERSION 3.0)

project(cpfspd)

message(STATUS "###################################")

message(STATUS "operation system is ${CMAKE_SYSTEM}")

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    message(STATUS "current platform: Linux ")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    message(STATUS "current platform: Windows")
    if(MSVC)
        add_compile_options(
            /D_CRT_SECURE_NO_WARNINGS
            /D_LARGEFILE64_SOURCE
            "/DFIO_FUNC_FSEEK(stream,offset,origin)=_fseeki64(stream,offset,origin)"
            "/DFIO_OFFSET_T=__int64"
        )
        message(STATUS "MSVC")
    endif()
    if(MINGW)
        add_compile_options(
            -D_CRT_SECURE_NO_WARNINGS
            -Wno-long-long
            -D_LARGEFILE64_SOURCE
            "-DFIO_FUNC_FSEEK(stream,offset,origin)=fseeko64(stream,offset,origin)"
            "-DFIO_OFFSET_T=_off64_t"
            "-D_FILE_OFFSET_BITS=64"
        )
        message(STATUS "MINGW")
    endif()
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    message(STATUS "current platform: Mac Os")
    add_compile_options(
            -Wno-long-long
            -D_LARGEFILE64_SOURCE
            "-DFIO_FUNC_FSEEK(stream,offset,origin)=fseeko(stream,offset,origin)"
            "-DFIO_OFFSET_T=off_t"
            "-D_FILE_OFFSET_BITS=64"
        )

ELSE ()
    message(STATUS "other platform: ${CMAKE_SYSTEM_NAME}")
ENDIF (CMAKE_SYSTEM_NAME MATCHES "Linux")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -std=c++11 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -std=c++11 -Wall")
set(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -std=c99 -Wall -g -ggdb")
set(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -O3 -std=c99 -Wall")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} -s")

message(STATUS "###################################")

file(GLOB_RECURSE lib_src "${CMAKE_CURRENT_SOURCE_DIR}/src/cpfspd/*.c")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/cpfspd)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/include)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/googletest)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/googlecrc32c)


function(ADD_TEST test)
    file(GLOB_RECURSE test_src ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp)
    add_executable(${test} ${test_src})
    target_link_libraries(${test} gtest_main cpfspd crc32c)
endfunction(ADD_TEST test)

ADD_TEST(cpfspd_test)

add_library(cpfspd STATIC  ${lib_src})