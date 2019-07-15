cmake_minimum_required(VERSION 3.0)

IF(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
    message( FATAL_ERROR "Directory lib does not exist; please run ./download.sh first" )
ENDIF()

link_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib")

add_subdirectory(src-test)
add_subdirectory(examples)
