cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME objectbox-examples-vectorsearch-cities)
project(${PROJECT_NAME} CXX)
add_executable(${PROJECT_NAME}
        main.cpp
        city.obx.cpp
)
set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
)
target_link_libraries(${PROJECT_NAME} objectbox)

if (NOT TARGET objectbox) # Using "if" in case a (potential) parent project already defined "objectbox"
    include(FetchContent)
    FetchContent_Declare(
            objectbox
            GIT_REPOSITORY https://github.com/objectbox/objectbox-c.git
            GIT_TAG v5.0.0-rc4 # For latest versions, check https://github.com/objectbox/objectbox-c/releases
    )
    FetchContent_MakeAvailable(objectbox)

    if ((WIN32 OR CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
            AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64|ARM|AARCH64)")
        message(STATUS "Skipping ObjectBox Generator integration, which is not yet supported on this platform (${CMAKE_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR})")
    else ()
        find_package(ObjectBoxGenerator REQUIRED)
        add_obx_schema(TARGET ${PROJECT_NAME} SCHEMA_FILES city.fbs INSOURCE)
    endif ()
endif ()

configure_file(cities.csv ${CMAKE_CURRENT_BINARY_DIR}/cities.csv COPYONLY)
add_custom_target(copy_csv_file ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/cities.csv)
add_dependencies(${PROJECT_NAME} copy_csv_file)

# Print the executable path after build, so user can run it easily
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Built executable at \"$<TARGET_FILE:${PROJECT_NAME}>\""
        VERBATIM
)
