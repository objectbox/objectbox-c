cmake_minimum_required(VERSION 3.0)

set(PROJECT_NAME objectbox-c-examples-tasks)

project(${PROJECT_NAME} C)

include_directories(../../external)

add_executable(${PROJECT_NAME}
        main.c
        # Note: the flatcc sources are embedded into this project for convenience:
        ../../external/flatcc-src/builder.c
        ../../external/flatcc-src/emitter.c
        ../../external/flatcc-src/refmap.c
        )

target_link_libraries(${PROJECT_NAME} objectbox)

IF (CMAKE_ANDROID)
    target_link_libraries(${PROJECT_NAME} log)
ENDIF ()
