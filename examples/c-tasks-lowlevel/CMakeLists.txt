cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME objectbox-examples-c-tasks-lowlevel)

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
            GIT_TAG v5.0.0 # For latest versions, check https://github.com/objectbox/objectbox-c/releases
    )
    FetchContent_MakeAvailable(objectbox)

    # Note: ObjectBox generator is not used in this low-level example
endif ()

target_link_libraries(${PROJECT_NAME} objectbox)

IF (CMAKE_ANDROID)
    target_link_libraries(${PROJECT_NAME} log)
ENDIF ()

# Print the executable path after build, so user can run it easily
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Built executable at \"$<TARGET_FILE:${PROJECT_NAME}>\""
        VERBATIM
)
