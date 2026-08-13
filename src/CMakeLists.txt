# CMake build file for SACD Extract

cmake_minimum_required(VERSION 3.16)

project(sacd_extract VERSION 0.8.0 LANGUAGES C CXX)

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Force to compile for Mac OS X 10.13. target (if you want the app run on all OS X versions).
# Without this adding, if build is made on a MacOS 10.15 or newer, then the app will not run on systems older than 10.15.
# You can delete this if you don't care about the Mac OS X 10.15 incompatibilty problem. 
if (APPLE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)
set(ENV{MACOSX_DEPLOYMENT_TARGET} 10.13)
endif()

option(SACD_WINDOWS_STATIC
       "Statically link non-system libraries into the Windows executable" OFF)

execute_process(
    COMMAND git describe --always --dirty --abbrev=12
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    RESULT_VARIABLE GIT_COMMIT_HASH_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND git remote get-url origin
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_REPO_URL
    RESULT_VARIABLE GIT_REPO_URL_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Obtain git commit hash and repo url
if(GIT_COMMIT_HASH_RESULT GREATER 0)
    add_definitions("-DGIT_COMMIT_HASH=NA")
else()
    add_definitions("-DGIT_COMMIT_HASH=${GIT_COMMIT_HASH}")
    MESSAGE(STATUS "git commit hash: ${GIT_COMMIT_HASH}")
endif()

if(GIT_REPO_URL_RESULT GREATER 0)
    add_definitions("-DGIT_REPO_URL=NA")
else()
    add_definitions("-DGIT_REPO_URL=\"${GIT_REPO_URL}\"")
    MESSAGE(STATUS "git repo url: ${GIT_REPO_URL}")
endif()

# Load some macros.
SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")

# Macros we'll need
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckTypeSize)

# Include directory paths
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${sacd_extract_SOURCE_DIR})

include_directories("../libs/libcommon")
include_directories("../libs/libdstdec")
include_directories("../libs/libid3")
include_directories("../libs/libsacd")


find_package(Threads REQUIRED)

# Build the reference FLAC encoder into sacd_extract.  Only libFLAC is built:
# no command-line programs, C++ wrapper, Ogg support, docs, or upstream tests.
# The release executable therefore has no end-user libFLAC dependency.
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
set(_SACD_BUILD_TESTING "${BUILD_TESTING}")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(BUILD_CXXLIBS OFF CACHE BOOL "Build libFLAC++" FORCE)
set(BUILD_PROGRAMS OFF CACHE BOOL "Build FLAC programs" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build FLAC examples" FORCE)
set(BUILD_DOCS OFF CACHE BOOL "Build FLAC documentation" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build dependency tests" FORCE)
set(INSTALL_MANPAGES OFF CACHE BOOL "Install FLAC man pages" FORCE)
set(INSTALL_PKGCONFIG_MODULES OFF CACHE BOOL "Install FLAC pkg-config files" FORCE)
set(INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "Install FLAC CMake files" FORCE)
set(WITH_OGG OFF CACHE BOOL "Build Ogg FLAC support" FORCE)
set(ENABLE_MULTITHREADING OFF CACHE BOOL "Build threaded FLAC encoder" FORCE)
FetchContent_Declare(
    flac
    URL https://downloads.xiph.org/releases/flac/flac-1.5.0.tar.xz
    URL_HASH SHA256=f2c1c76592a82ffff8413ba3c4a1299b6c7ab06c734dee03fd88630485c2b920
)
FetchContent_MakeAvailable(flac)
set(BUILD_TESTING "${_SACD_BUILD_TESTING}" CACHE BOOL "Build sacd_extract tests" FORCE)
unset(_SACD_BUILD_TESTING)

# Extra flags for GCC
STRING(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)

if (CMAKE_COMPILER_IS_GNUCC OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
    add_definitions(
        -pipe
        -Wall -Wextra -Wcast-align -Wpointer-arith
        -Wno-unused-parameter)
 if (WIN32)
     add_definitions(
        -fno-stack-protector)
 else ()
      add_definitions(
        -fstack-protector)
 endif ()       
 if(NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")
    add_definitions(
        -msse2)
 endif ()        
endif () 


if(NOT CMAKE_BUILD_TYPE_UPPER STREQUAL "DEBUG")
  if (CMAKE_COMPILER_IS_GNUCC OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
    add_definitions(-O3)
  endif ()

else()
  if (CMAKE_COMPILER_IS_GNUCC OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
    add_definitions(-g -Wpedantic -pedantic-errors)
  endif ()  
endif ()


file(GLOB libcommon_headers ../libs/libcommon/*.h)
file(GLOB libcommon_sources ../libs/libcommon/*.c)
# avoid ISO C empty translation unit errors
list(REMOVE_ITEM libcommon_headers "${CMAKE_CURRENT_SOURCE_DIR}/../libs/libcommon/usocket.h")
list(REMOVE_ITEM libcommon_sources "${CMAKE_CURRENT_SOURCE_DIR}/../libs/libcommon/usocket.c")
list(REMOVE_ITEM libcommon_headers "${CMAKE_CURRENT_SOURCE_DIR}/../libs/libcommon/wsocket.h")
list(REMOVE_ITEM libcommon_sources "${CMAKE_CURRENT_SOURCE_DIR}/../libs/libcommon/wsocket.c")
source_group(libcommon FILES ${libcommon_headers} ${libcommon_sources})

file(GLOB libdstdec_headers ../libs/libdstdec/*.h)
file(GLOB libdstdec_sources ../libs/libdstdec/*.c)
source_group(libdstdec FILES ${libdstdec_headers} ${libdstdec_sources})

file(GLOB libid3_headers ../libs/libid3/*.h)
file(GLOB libid3_sources ../libs/libid3/*.c)
source_group(libid3 FILES ${libid3_headers} ${libid3_sources})

file(GLOB libsacd_headers ../libs/libsacd/*.h)
file(GLOB libsacd_sources ../libs/libsacd/*.c)
source_group(libsacd FILES ${libsacd_headers} ${libsacd_sources})

file(GLOB main_headers ./*.h)
file(GLOB main_sources ./*.c)
source_group(main FILES ${main_headers} ${main_sources})

add_library(sacd_extract_core STATIC
    ${libcommon_headers} ${libcommon_sources}
    ${libdstdec_headers} ${libdstdec_sources}
    ${libid3_headers} ${libid3_sources}
    ${libsacd_headers} ${libsacd_sources}
)
target_include_directories(sacd_extract_core PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../libs/libcommon
    ../libs/libdstdec
    ../libs/libid3
    ../libs/libsacd)

add_executable(sacd_extract ${main_headers} ${main_sources})
target_link_libraries(sacd_extract PRIVATE sacd_extract_core)

if(WIN32)
    if(NOT MINGW)
        message(FATAL_ERROR
                "The Windows build requires the MinGW-w64 toolchain. "
                "Configure from an MSYS2 MinGW shell.")
    endif()

    if(SACD_WINDOWS_STATIC)
        set(_SACD_LIBRARY_SUFFIXES "${CMAKE_FIND_LIBRARY_SUFFIXES}")
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    endif()
    find_package(Iconv REQUIRED)
    if(SACD_WINDOWS_STATIC)
        set(CMAKE_FIND_LIBRARY_SUFFIXES "${_SACD_LIBRARY_SUFFIXES}")
        unset(_SACD_LIBRARY_SUFFIXES)
    endif()
    target_compile_definitions(sacd_extract_core PUBLIC
        _FILE_OFFSET_BITS=64
        UNICODE
        _UNICODE)
    target_compile_definitions(sacd_extract PRIVATE
        _FILE_OFFSET_BITS=64
        UNICODE
        _UNICODE)
    target_link_libraries(sacd_extract_core PUBLIC
        Threads::Threads
        FLAC::FLAC
        Iconv::Iconv
        ws2_32)
    # main.c uses wmain so paths and arguments remain Unicode on Windows.
    target_link_options(sacd_extract PRIVATE -municode)

    if(SACD_WINDOWS_STATIC)
        target_compile_definitions(sacd_extract_core PUBLIC LIBICONV_STATIC)
        target_link_options(sacd_extract PRIVATE -static -static-libgcc)
    endif()
elseif(APPLE)
    add_definitions(-D_FILE_OFFSET_BITS=64)
    target_link_libraries(sacd_extract_core PUBLIC -liconv FLAC::FLAC)
else()
    add_definitions(-D_FILE_OFFSET_BITS=64)
    target_link_libraries(sacd_extract_core PUBLIC Threads::Threads FLAC::FLAC)
endif()

if(BUILD_TESTING)
    add_subdirectory(../tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()
