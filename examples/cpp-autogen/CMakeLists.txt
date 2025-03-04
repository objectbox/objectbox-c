# C++ local-only example using objectbox-generator via cmake
set(PROJECT_NAME objectbox-c-examples-tasks-cpp-autogen)
project(${PROJECT_NAME} CXX)
add_executable(${PROJECT_NAME}
        main.cpp
        )

# add_obx_schema provided by ObjectBoxGenerator package, which is fetched in one of the parent CMakeLists.txt files.
add_obx_schema(TARGET ${PROJECT_NAME} SCHEMA_FILES tasklist.fbs INSOURCE)

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
        )
target_link_libraries(${PROJECT_NAME} objectbox)
target_include_directories(${PROJECT_NAME} PRIVATE ../../include ../../external)
