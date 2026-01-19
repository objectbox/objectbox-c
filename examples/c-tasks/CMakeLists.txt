cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME objectbox-examples-c-tasks)

project(${PROJECT_NAME} C)

add_executable(${PROJECT_NAME}
        main.c
        # Note: the flatcc sources are embedded into this project for convenience:
        ../../external/flatcc-src/builder.c
        ../../external/flatcc-src/emitter.c
        ../../external/flatcc-src/refmap.c
)

if (NOT TARGET objectbox) # Using "if" in case a (potential) parent project already defined "objectbox"
    include(FetchContent)
    FetchContent_Declare(
            objectbox
            GIT_REPOSITORY https://github.com/objectbox/objectbox-c.git
            GIT_TAG v5.1.0 # For latest versions, check https://github.com/objectbox/objectbox-c/releases
    )
    FetchContent_MakeAvailable(objectbox)

    # TODO: fix generator error with C-only build: "Missing variable CMAKE_CXX_COMPILE_OBJECT/CMAKE_CXX_LINK_EXECUTABLE"
    #find_package(ObjectBoxGenerator REQUIRED)
    #add_obx_schema(TARGET ${PROJECT_NAME} SCHEMA_FILES tasklist.fbs INSOURCE)
endif ()

target_link_libraries(${PROJECT_NAME} objectbox)

# Print the executable path after build, so user can run it easily
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Built executable at \"$<TARGET_FILE:${PROJECT_NAME}>\""
        VERBATIM
)
