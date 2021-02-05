# C++ local-only example
set(PROJECT_NAME objectbox-c-examples-tasks-cpp-gen)
project(${PROJECT_NAME} CXX)
add_executable(${PROJECT_NAME}
        main.cpp
        tasklist.obx.cpp
        )
set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        )
target_link_libraries(${PROJECT_NAME} objectbox)
target_include_directories(${PROJECT_NAME} PRIVATE ../../include ../../external)

# C++ sync-enabled client example
set(PROJECT_NAME objectbox-c-examples-tasks-cpp-gen-sync)
project(${PROJECT_NAME} CXX)
add_executable(${PROJECT_NAME}
        main-sync.cpp
        tasklist.obx.cpp
        )
set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        )
target_link_libraries(${PROJECT_NAME} objectbox)
target_include_directories(${PROJECT_NAME} PRIVATE ../../include ../../external)
