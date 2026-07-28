cmake_minimum_required(VERSION 3.27)

project(fbx-reroot
    VERSION     1.0.0
    DESCRIPTION "FBX armature re-rooting utility"
    LANGUAGES   CXX
)

set(CMAKE_CXX_STANDARD               20)
set(CMAKE_CXX_STANDARD_REQUIRED      ON)
set(CMAKE_CXX_EXTENSIONS             OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY   ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY   ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY   ${CMAKE_BINARY_DIR}/bin)

# Ensure static linking
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if (MSVC)
    add_link_options(
        $<$<NOT:$<STREQUAL:${CMAKE_MSVC_RUNTIME_LIBRARY},MultiThreaded>>:/NODEFAULTLIB:libcmt.lib>
        $<$<NOT:$<STREQUAL:${CMAKE_MSVC_RUNTIME_LIBRARY},MultiThreadedDLL>>:/NODEFAULTLIB:msvcrt.lib>
        $<$<NOT:$<STREQUAL:${CMAKE_MSVC_RUNTIME_LIBRARY},MultiThreadedDebug>>:/NODEFAULTLIB:libcmtd.lib>
        $<$<NOT:$<STREQUAL:${CMAKE_MSVC_RUNTIME_LIBRARY},MultiThreadedDebugDLL>>:/NODEFAULTLIB:msvcrtd.lib>
    )
endif()

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Select static FBX SDK with static CRT (libfbxsdk-mt.lib)
set(FBX_SHARED OFF)
set(FBX_MSVC_RT_DLL OFF)

find_package(FBX REQUIRED)

# Dependencies
add_subdirectory(third_party/cli11)

add_subdirectory(src)

install(
    DIRECTORY   include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
)