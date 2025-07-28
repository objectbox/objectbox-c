# C++ local-only example
cmake_minimum_required(VERSION 3.10)
set(PROJECT_NAME objectbox-c-examples-vectorsearch-cities)
project(${PROJECT_NAME} CXX)
add_executable(${PROJECT_NAME}
        main.cpp
        city.obx.cpp
        )
set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
        )
target_link_libraries(${PROJECT_NAME} objectbox)
target_include_directories(${PROJECT_NAME} PRIVATE ../../include ../../external)

configure_file(cities.csv ${CMAKE_CURRENT_BINARY_DIR}/cities.csv COPYONLY)
add_custom_target(copy_csv_file ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/cities.csv)
add_dependencies(${PROJECT_NAME} copy_csv_file)
