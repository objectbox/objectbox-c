cmake_minimum_required(VERSION 3.5...4.0)
# ^ We likely require a new minimum version soon, as we do not test those ancient CMake versions anymore.

# This CMake file has the following purposes:
# * Define the ObjectBox library target (target name "objectbox"; or, if the sync variant is used, "objectbox-sync")
# * Fetch (download) the ObjectBoxGenerator CMake
# * Unless this CMake is consumed from a user project, add the sub-projects (tests and examples)
#
# Options are available via plain (not cached) variables that can be declared before fetching this CMake module:
# * ObjectBoxGenerator_CMAKE_DISABLE: set to ON to skip CMake integration of ObjectBox Generator
# * ObjectBoxGenerator_CMAKE_VERSION: override the default version of the ObjectBox Generator CMake.
#                                     Changes the version to be fetched (git tag or branch).

project(ObjectBoxCRoot)

# Remove Warning (as of CMake >= 3.24): 
# "The DOWNLOAD_EXTRACT_TIMESTAMP option was not given.." 
# We use the new behaviour (file timestamps from downloaded/extracted archives are updated).
if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

if (${CMAKE_VERSION} VERSION_LESS "3.11.0")
    message("Please consider upgrading your CMake to a more recent version (v3.11+) to get automatic library download.")
    if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
        message(FATAL_ERROR "Directory lib does not exist; please run ./download.sh first")
    endif ()
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib")
else ()
    function(defineObjectBoxLibForURL VARIANT DL_URL)
        # Fetch (download and un-archive) the ObjectBox library
        include(FetchContent)
        set(_obx_download_name objectbox${VARIANT}-download)
        # Note: FetchContent_Populate(URL ${DL_URL}) on Windows CMake 3.31 downloaded to "wrong" dir (leaving out "_deps")
        FetchContent_Declare(${_obx_download_name} URL ${DL_URL})
        if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.14") # FetchContent_Populate with single param is deprecated
            FetchContent_MakeAvailable(${_obx_download_name})
        else()
            FetchContent_Populate(${_obx_download_name})
        endif()
        set(DL_DIR "${${_obx_download_name}_SOURCE_DIR}")
        message(STATUS "ObjectBox library saved to ${DL_DIR}")

        # Create the public imported target with the expected name: objectbox or objectbox-sync
        set(_obx_public_target objectbox${VARIANT})
        add_library(${_obx_public_target} SHARED IMPORTED GLOBAL)
        set(objectbox_include_dirs ${DL_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/external/)
        set_target_properties(
                ${_obx_public_target} PROPERTIES
                IMPORTED_LOCATION ${DL_DIR}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}objectbox${CMAKE_SHARED_LIBRARY_SUFFIX}
                IMPORTED_IMPLIB ${DL_DIR}/lib/${CMAKE_IMPORT_LIBRARY_PREFIX}objectbox${CMAKE_IMPORT_LIBRARY_SUFFIX}
                INTERFACE_INCLUDE_DIRECTORIES "${objectbox_include_dirs}"
        )
    endfunction()

    function(defineObjectBoxLib VARIANT)
        # Configuration updated for each release
        set(DL_VERSION 5.0.0-rc3)

        # Platform detection and other setup
        set(DL_URL https://github.com/objectbox/objectbox-c/releases/download)

        set(DL_EXTENSION "tar.gz")
        if (${CMAKE_SYSTEM_NAME} STREQUAL Darwin)
            set(DL_PLATFORM "MacOS-universal")
            set(DL_EXTENSION "zip")
        elseif (${CMAKE_SYSTEM_NAME} STREQUAL Windows)
            set(DL_EXTENSION "zip")
            if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|aarch64|arm64)$")
                set(DL_PLATFORM ${CMAKE_SYSTEM_NAME}-arm64)
            elseif (CMAKE_SIZEOF_VOID_P MATCHES 8)
                set(DL_PLATFORM ${CMAKE_SYSTEM_NAME}-x64)
            else ()
                set(DL_PLATFORM ${CMAKE_SYSTEM_NAME}-x86)
            endif ()
        elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL x86_64)
            set(DL_PLATFORM ${CMAKE_SYSTEM_NAME}-x64)
        else ()
            set(DL_PLATFORM ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})
        endif ()

        if (${DL_PLATFORM} MATCHES "^Linux-armv7")
            set(DL_PLATFORM "Linux-armv7hf") # show what we actually download
        elseif (${DL_PLATFORM} MATCHES "^Linux-armv6")
            set(DL_PLATFORM "Linux-armv6hf") # show what we actually download
        endif ()

        string(TOLOWER ${DL_PLATFORM} DL_PLATFORM)
        set(DL_URL ${DL_URL}/v${DL_VERSION}/objectbox${VARIANT}-${DL_PLATFORM}.${DL_EXTENSION})
        message(STATUS "Using pre-compiled ObjectBox${VARIANT} library v${DL_VERSION} for ${DL_PLATFORM}: ${DL_URL}")

        defineObjectBoxLibForURL("${VARIANT}" "${DL_URL}")
    endfunction()

    if (DEFINED ENV{OBJECTBOX_ARTIFACT_URL})
        set(DL_URL $ENV{OBJECTBOX_ARTIFACT_URL})
        message(STATUS "Using pre-compiled ObjectBox library from the OBJECTBOX_ARTIFACT_URL environment variable: ${DL_URL}")
        defineObjectBoxLibForURL("" "${DL_URL}")
    else ()
        defineObjectBoxLib("")
        defineObjectBoxLib("-sync")
    endif ()
endif ()

# ObjectBoxGenerator CMake Downloader
# -----------------------------------
# Make "FindObjectBoxGenerator" available, which is used to download/find and run the ObjectBox Generator.

if (ObjectBoxGenerator_CMAKE_DISABLE)
    message(STATUS "ObjectBox Generator: CMake integration disabled")
else ()
    if(NOT DEFINED ObjectBoxGenerator_CMAKE_VERSION OR ObjectBoxGenerator_CMAKE_VERSION STREQUAL "")
        # The default version is a specific version that "matches" the ObjectBox library version of this repo.
        # Nevertheless, it's often possible to use a newer Generator version as breaking changes are infrequent.
        set(ObjectBoxGenerator_CMAKE_VERSION "v5.0.0-rc2")
    endif ()
    set(OBX_GEN_DL_URL https://raw.githubusercontent.com/objectbox/objectbox-generator/${ObjectBoxGenerator_CMAKE_VERSION}/cmake/FindObjectBoxGenerator.cmake)

    if (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.18) # 3.18 because DOWNLOAD_NO_EXTRACT
        message(STATUS "ObjectBox Generator: fetching version \"${ObjectBoxGenerator_CMAKE_VERSION}\"")
        include(FetchContent)
        FetchContent_Declare(FindObjectBoxGenerator URL ${OBX_GEN_DL_URL} DOWNLOAD_NO_EXTRACT TRUE)
        FetchContent_MakeAvailable(FindObjectBoxGenerator)
        set(OBX_GEN_MODULE_DIR ${findobjectboxgenerator_SOURCE_DIR})
    else ()
        message(STATUS "ObjectBox Generator: fetching version \"${ObjectBoxGenerator_CMAKE_VERSION}\" (using old CMake version)")
        set(OBX_GEN_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR}/cmake)
        file(MAKE_DIRECTORY ${OBX_GEN_MODULE_DIR})
        if (NOT EXISTS ${OBX_GEN_MODULE_DIR}/FindObjectBoxGenerator.cmake)
            file(DOWNLOAD ${OBX_GEN_DL_URL} ${OBX_GEN_MODULE_DIR}/FindObjectBoxGenerator.cmake
                    TLS_VERIFY ON
                    STATUS DL_STATUS
            )
            if (NOT DL_STATUS EQUAL 0)
                message(WARNING "Downloading FindObjectBoxGenerator.cmake from URL ${DL_URL} failed")
            endif ()
        endif ()
    endif ()

    if (EXISTS ${OBX_GEN_MODULE_DIR}/FindObjectBoxGenerator.cmake)
        # Enable find_package to locate ObjectBoxGenerator find module.
        list(APPEND CMAKE_MODULE_PATH ${OBX_GEN_MODULE_DIR})
        get_directory_property(hasParent PARENT_DIRECTORY)
        if (hasParent)
            set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} PARENT_SCOPE)
        endif ()
    endif ()
endif ()


# If this project is top-level, include public tests.
# Otherwise, this CMake file is used from a user project, so we do not want to expose these.
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    add_subdirectory(src-test)     # target:  objectbox-c-test
    add_subdirectory(src-test-gen) # target:  objectbox-c-gen-test
    # Note: examples are standalone CMake projects, so we do not include them here.
endif ()
