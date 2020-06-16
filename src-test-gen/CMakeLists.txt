set(CMAKE_C_STANDARD 99)

include_directories(../include ../external)

project(objectbox-c-gen-test C)

add_executable(${PROJECT_NAME}
        plain-c-test-main.c
        ../external/flatcc-src/builder.c
        ../external/flatcc-src/emitter.c
        ../external/flatcc-src/refmap.c
        )

target_link_libraries(${PROJECT_NAME} objectbox)

IF (CMAKE_ANDROID)
    target_link_libraries(${PROJECT_NAME} log)
ENDIF ()
