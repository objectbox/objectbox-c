set(PROJECT_NAME objectbox-c-examples-tasks-c-gen)

project(${PROJECT_NAME} C)

add_executable(${PROJECT_NAME}
        main.c
        # Note: the flatcc sources are embedded into this project for convenience:
        ../../external/flatcc-src/builder.c
        ../../external/flatcc-src/emitter.c
        ../../external/flatcc-src/refmap.c
        )

target_link_libraries(${PROJECT_NAME} objectbox)
target_include_directories(${PROJECT_NAME} PRIVATE ../../external ../../include)
