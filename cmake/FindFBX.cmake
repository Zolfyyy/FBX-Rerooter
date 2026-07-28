#[=======================================================================[.rst:
FindFBX
-------

Find the Autodesk FBX SDK static libraries and headers.

Imported Targets
^^^^^^^^^^^^^^^^

This module defines the following imported target:

``FBX::SDK``
  Library target of the FBX SDK.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``FBX_FOUND``
  True if the FBX SDK was found.
``FBX_INCLUDE_DIR``
  The include directory for the FBX SDK.
``FBX_LIBRARY_RELEASE``
  The release version of the FBX SDK library.
``FBX_LIBRARY_DEBUG``
  The debug version of the FBX SDK library.

Cache Variables
^^^^^^^^^^^^^^^

``FBX_SDK_DIR``
  Path to the FBX SDK root directory (containing ``include/`` and ``lib/``).

#]=======================================================================]

include(GNUInstallDirs)

set(FBX_SDK_DIR "" CACHE PATH "Path to the FBX SDK root directory (containing include/ and lib/)")

if(NOT FBX_SDK_DIR)
    message(FATAL_ERROR "FBX_SDK_DIR is required but not set. "
                        "Set it to the FBX SDK root directory, "
                        "e.g. -DFBX_SDK_DIR=\"C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.9\"")
endif()

set(FBX_INCLUDE_DIR "${FBX_SDK_DIR}/include")
set(FBX_LIB_DIR     "${FBX_SDK_DIR}/lib")

if(NOT EXISTS "${FBX_INCLUDE_DIR}")
    message(FATAL_ERROR "FBX_SDK_DIR set to '${FBX_SDK_DIR}' but '${FBX_INCLUDE_DIR}' does not exist")
endif()
if(NOT EXISTS "${FBX_LIB_DIR}")
    message(FATAL_ERROR "FBX_SDK_DIR set to '${FBX_SDK_DIR}' but '${FBX_LIB_DIR}' does not exist")
endif()

if(WIN32)
    set(_fbx_arch "x64")
    if(CMAKE_GENERATOR_PLATFORM MATCHES "x86|Win32")
        set(_fbx_arch "x86")
    endif()

    find_library(FBX_LIBRARY_RELEASE
        NAMES "libfbxsdk-mt"
        PATHS "${FBX_LIB_DIR}/${_fbx_arch}/release"
        NO_DEFAULT_PATH
    )
    find_library(FBX_LIBRARY_DEBUG
        NAMES "libfbxsdk-mt"
        PATHS "${FBX_LIB_DIR}/${_fbx_arch}/debug"
        NO_DEFAULT_PATH
    )
else()
    find_library(FBX_LIBRARY_RELEASE
        NAMES "libfbxsdk.a" "fbxsdk"
        PATHS "${FBX_LIB_DIR}/gcc4/x64/release"
              "${FBX_LIB_DIR}/clang/x64/release"
        NO_DEFAULT_PATH
    )
    set(FBX_LIBRARY_DEBUG "${FBX_LIBRARY_RELEASE}")
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(FBX
    REQUIRED_VARS
        FBX_LIBRARY_RELEASE
        FBX_INCLUDE_DIR
)

if(FBX_FOUND AND NOT TARGET FBX::SDK)
    add_library(FBX::SDK STATIC IMPORTED)

    set_target_properties(FBX::SDK PROPERTIES
        IMPORTED_LOCATION             "${FBX_LIBRARY_RELEASE}"
        IMPORTED_LOCATION_RELEASE     "${FBX_LIBRARY_RELEASE}"
        IMPORTED_LOCATION_DEBUG       "${FBX_LIBRARY_DEBUG}"
        INTERFACE_INCLUDE_DIRECTORIES "${FBX_INCLUDE_DIR}"
    )

    # Link the static CRT dependencies the FBX SDK was built with
    if(WIN32 AND FBX_XML2_LIBRARY_RELEASE AND FBX_ZLIB_LIBRARY_RELEASE)
        set_target_properties(FBX::SDK PROPERTIES
            IMPORTED_LINK_INTERFACE_LIBRARIES_RELEASE "${FBX_XML2_LIBRARY_RELEASE};${FBX_ZLIB_LIBRARY_RELEASE}"
            IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG   "${FBX_XML2_LIBRARY_DEBUG};${FBX_ZLIB_LIBRARY_DEBUG}"
        )
    endif()

    message(STATUS "FBX SDK found (release): ${FBX_LIBRARY_RELEASE}")
    message(STATUS "FBX SDK found (debug):   ${FBX_LIBRARY_DEBUG}")
endif()

# Export variables for parent scope
mark_as_advanced(FBX_INCLUDE_DIR FBX_LIBRARY_RELEASE FBX_LIBRARY_DEBUG)