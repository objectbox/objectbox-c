cmake_minimum_required(VERSION 3.0)

project(ObjectBoxCRoot) # to be displayed in an IDE when this CMake is opened

if (${CMAKE_VERSION} VERSION_LESS "3.11.0")
    message("Please consider upgrading your CMake to a more recent version (v3.11+) to get automatic library download.")
    if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
        message(FATAL_ERROR "Directory lib does not exist; please run ./download.sh first")
    endif ()
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib")
else ()
    project(objectbox-download)

    if (DEFINED ENV{OBJECTBOX_ARTIFACT_URL})
        set(DL_URL $ENV{OBJECTBOX_ARTIFACT_URL})
        message(STATUS "Using pre-compiled ObjectBox library from the OBJECTBOX_ARTIFACT_URL environment variable: ${DL_URL}")
    else ()
        # Configuration updated for each release
        set(DL_VERSION 0.13.0)

        # Platform detection and other setup
        set(DL_URL https://github.com/objectbox/objectbox-c/releases/download)

        # ${CMAKE_SYSTEM_PROCESSOR} is invalid on Windows, see https://gitlab.kitware.com/cmake/cmake/-/issues/15170
        if (${CMAKE_SYSTEM_NAME} STREQUAL Windows)
            if (CMAKE_SIZEOF_VOID_P MATCHES 8)
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
        set(DL_URL ${DL_URL}/v${DL_VERSION}/objectbox-${DL_PLATFORM}.zip)
        message(STATUS "Using pre-compiled ObjectBox library v${DL_VERSION} for ${DL_PLATFORM}: ${DL_URL}")
    endif ()

    include(FetchContent)
    FetchContent_Declare(${PROJECT_NAME} URL ${DL_URL})

    FetchContent_Populate(${PROJECT_NAME})
    message(STATUS "Pre-compiled ObjectBox library is saved in ${objectbox-download_SOURCE_DIR}")

    project(objectbox)
    add_library(${PROJECT_NAME} SHARED IMPORTED GLOBAL)
    set(objectbox_include_dirs ${objectbox-download_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/external/)
    set_target_properties(
            ${PROJECT_NAME} PROPERTIES
            IMPORTED_LOCATION ${objectbox-download_SOURCE_DIR}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}objectbox${CMAKE_SHARED_LIBRARY_SUFFIX}
            IMPORTED_IMPLIB ${objectbox-download_SOURCE_DIR}/lib/${CMAKE_IMPORT_LIBRARY_PREFIX}objectbox${CMAKE_IMPORT_LIBRARY_SUFFIX}
            INTERFACE_INCLUDE_DIRECTORIES "${objectbox_include_dirs}"
    )
endif ()

add_subdirectory(src-test)
add_subdirectory(src-test-gen)
add_subdirectory(examples)
