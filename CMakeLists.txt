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

    # Configuration updated for each release
    set(DL_VERSION 0.12.0)
    set(DL_CHANNEL testing)

    # Platform detection and other setup
    set(DL_URL https://dl.bintray.com/objectbox/conan/objectbox/objectbox-c)

    # ${CMAKE_SYSTEM_PROCESSOR} is invalid on Windows, see https://gitlab.kitware.com/cmake/cmake/-/issues/15170
    if (${CMAKE_SYSTEM_NAME} STREQUAL Windows)
        if (CMAKE_SIZEOF_VOID_P MATCHES 8)
            set(DL_PLATFORM ${CMAKE_SYSTEM_NAME}-x86_64)
        else ()
            set(DL_PLATFORM ${CMAKE_SYSTEM_NAME}-x86)
        endif ()
    else ()
        set(DL_PLATFORM ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})
    endif ()

    if (${DL_PLATFORM} STREQUAL Linux-x86_64)
        set(DL_PACKAGE 4db1be536558d833e52e862fd84d64d75c2b3656)

    elseif (${DL_PLATFORM} MATCHES "^Linux-aarch64")
        # Note: aarch64 is how a 64-bit ARMv8 is reported.
        # Also, we don't match armv8 because we haven't seen it yet and also that "-R" and "-M: variants are 32-bit.
        set(DL_PACKAGE b0bab81756b4971d42859e9b1bc6f8b3fa8e036e)

    elseif (${DL_PLATFORM} MATCHES "^Linux-armv7")
        set(DL_PACKAGE d42930899c74345edc43f8b7519ec7645c13e4d8)
        set(DL_PLATFORM "Linux-armv7hf") # show what we actually download

    elseif (${DL_PLATFORM} MATCHES "^Linux-armv6")
        set(DL_PACKAGE 4a625f0bd5f477eacd9bd35e9c44c834d057524b)
        set(DL_PLATFORM "Linux-armv6hf") # show what we actually download

    elseif (${DL_PLATFORM} STREQUAL Darwin-x86_64)
        set(DL_PACKAGE 46f53f156846659bf39ad6675fa0ee8156e859fe)

    elseif (${DL_PLATFORM} STREQUAL Windows-x86_64)
        set(DL_PACKAGE ca33edce272a279b24f87dc0d4cf5bbdcffbc187)

    elseif (${DL_PLATFORM} STREQUAL Windows-x86)
        set(DL_PACKAGE 11e6a84a7894f41df553e7c92534c3bf26896802)

    else ()
        message(FATAL_ERROR "Can't download pre-compiled ObjectBox library - platform not supported: ${DL_PLATFORM}")
    endif ()

    include(FetchContent)
    FetchContent_Declare(
            ${PROJECT_NAME}
            URL ${DL_URL}/${DL_VERSION}/${DL_CHANNEL}/0/package/${DL_PACKAGE}/0/conan_package.tgz
    )

    message(STATUS "Using pre-compiled ObjectBox library version ${DL_CHANNEL}:${DL_VERSION} for ${DL_PLATFORM}")
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
