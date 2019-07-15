include_directories(../include ../external)

add_executable(objectbox-c-test
        plain-c-test-main.c
        c_test_objects.c
        c_test_objects.h
        ../external/flatcc-src/builder.c
        ../external/flatcc-src/emitter.c
        ../external/flatcc-src/refmap.c
        )

target_link_libraries(objectbox-c-test objectbox)

IF (CMAKE_ANDROID)
    target_link_libraries(objectbox-c-test log)
ENDIF ()
