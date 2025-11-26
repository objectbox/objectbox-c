cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME objectbox-examples-tasks)
project(${PROJECT_NAME} CXX)
add_executable(${PROJECT_NAME}
        main.cpp
        tasklist.obx.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
)

if (NOT TARGET objectbox) # Using "if" in case a (potential) parent project already defined "objectbox"
    include(FetchContent)
    FetchContent_Declare(
            objectbox
            GIT_REPOSITORY https://github.com/objectbox/objectbox-c.git
            GIT_TAG v5.0.0-rc2 # For latest versions, check https://github.com/objectbox/objectbox-c/releases
    )
    FetchContent_MakeAvailable(objectbox)

    if ((WIN32 OR CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
            AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64|ARM|AARCH64)")
        message(STATUS "Skipping ObjectBox Generator integration, which is not yet supported on this platform (${CMAKE_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR})")
    else ()
        find_package(ObjectBoxGenerator REQUIRED)
        add_obx_schema(TARGET ${PROJECT_NAME} SCHEMA_FILES tasklist.fbs INSOURCE)
    endif ()
endif ()

target_link_libraries(${PROJECT_NAME} objectbox)

# Print the executable path after build, so user can run it easily
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Built executable at \"$<TARGET_FILE:${PROJECT_NAME}>\""
        VERBATIM
)
