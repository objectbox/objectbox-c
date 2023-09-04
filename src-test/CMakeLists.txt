set(CMAKE_C_STANDARD 99)

include_directories(../include ../external)

set(PLAIN_C_TEST_SOURCES
        plain-c-test-main.c
        c_test_objects.c
        c_test_objects.h
        ../external/flatcc-src/builder.c
        ../external/flatcc-src/emitter.c
        ../external/flatcc-src/refmap.c
        )

add_executable(objectbox-c-test ${PLAIN_C_TEST_SOURCES})
target_link_libraries(objectbox-c-test objectbox)

IF (CMAKE_ANDROID)
    target_link_libraries(objectbox-c-test log)
ENDIF ()

if (TARGET objectbox-static-distribution-lib)
    # Even if it doesn't do much, this is already useful e.g. to detect missing symbols during linking
    add_executable(objectbox-c-test-static-lib ${PLAIN_C_TEST_SOURCES})
    target_link_libraries(objectbox-c-test-static-lib objectbox-static-distribution-lib stdc++ pthread)
    IF (CMAKE_ANDROID)
        target_link_libraries(objectbox-c-test-static-lib log)
    ENDIF ()
endif()