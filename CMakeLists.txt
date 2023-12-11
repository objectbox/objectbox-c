cmake_minimum_required(VERSION 3.0)

project(ObjectBoxCRoot) # to be displayed in an IDE when this CMake is opened

if (${CMAKE_VERSION} VERSION_LESS "3.11.0")
    message("Please consider upgrading your CMake to a more recent version (v3.11+) to get automatic library download.")
    if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
        message(FATAL_ERROR "Directory lib does not exist; please run ./download.sh first")
    endif ()
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib")
else ()
    function(defineObjectBoxLibForURL VARIANT DL_URL)
        include(FetchContent)
        project(objectbox${VARIANT}-download)
        FetchContent_Declare(${PROJECT_NAME} URL ${DL_URL})

        FetchContent_Populate(${PROJECT_NAME})
        set(DL_DIR "${${PROJECT_NAME}_SOURCE_DIR}")
        message(STATUS "Pre-compiled ObjectBox library is saved in ${DL_DIR}")

        project(objectbox${VARIANT})
        add_library(${PROJECT_NAME} SHARED IMPORTED GLOBAL)
        set(objectbox_include_dirs ${DL_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/external/)
        set_target_properties(
                ${PROJECT_NAME} PROPERTIES
                IMPORTED_LOCATION ${DL_DIR}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}objectbox${CMAKE_SHARED_LIBRARY_SUFFIX}
                IMPORTED_IMPLIB ${DL_DIR}/lib/${CMAKE_IMPORT_LIBRARY_PREFIX}objectbox${CMAKE_IMPORT_LIBRARY_SUFFIX}
                INTERFACE_INCLUDE_DIRECTORIES "${objectbox_include_dirs}"
        )
    endfunction()

    function(defineObjectBoxLib VARIANT)
        # Configuration updated for each release
        set(DL_VERSION 0.20.0)

        # Platform detection and other setup
        set(DL_URL https://github.com/objectbox/objectbox-c/releases/download)

        set(DL_EXTENSION "tar.gz")
        if (${CMAKE_SYSTEM_NAME} STREQUAL Darwin)
            set(DL_PLATFORM "MacOS-universal")
            set(DL_EXTENSION "zip")
        elseif (${CMAKE_SYSTEM_NAME} STREQUAL Windows)
            set(DL_EXTENSION "zip")
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

add_subdirectory(src-test)
add_subdirectory(src-test-gen)
add_subdirectory(examples)
