add_subdirectory(c-cursor-no-gen)
add_subdirectory(c-gen)
add_subdirectory(cpp-gen)
add_subdirectory(cpp-gen-sync)

find_package(ObjectBoxGenerator 4.0.0)
# Some platforms such as Linux ARM(64) and Windows ARM(64) are not supported.
# We exclude this example for these cases where the generator was not found.
if (ObjectBoxGenerator_FOUND)
    add_subdirectory(cpp-autogen)
else ()
  message(WARNING "Did not add all examples, as the ObjectBoxGenerator CMake was not found")
endif ()
add_subdirectory(vectorsearch-cities)
