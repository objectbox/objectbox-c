# C++ local-only example using objectbox-generator via cmake
set(PROJECT_NAME objectbox-c-examples-tasks-cpp-autogen)
project(${PROJECT_NAME} CXX)
add_executable(${PROJECT_NAME}
        main.cpp
        )

# Find Package is available at https://github.com/objectbox/objectbox-generator under cmake folder
# In this example it is automatically downloaded via top-level CMakeLists.txt using FetchContent
find_package(ObjectBoxGenerator 0.15.0 REQUIRED)

# add_obx_schema provided by ObjectBoxGenerator package.
add_obx_schema(TARGET ${PROJECT_NAME} SCHEMA_FILES tasklist.fbs INSOURCE)

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
        )
target_link_libraries(${PROJECT_NAME} objectbox)
target_include_directories(${PROJECT_NAME} PRIVATE ../../include ../../external)
