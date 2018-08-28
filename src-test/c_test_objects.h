#ifndef PROJECT_C_TEST_OBJECTS_H
#define PROJECT_C_TEST_OBJECTS_H

#include <stdint.h>

#include "objectbox.h"
#include "c_test_builder.h"

int create_foo(flatcc_builder_t *B, uint64_t id, char* text);
int put_foo(OB_cursor* cursor, uint64_t* idInOut, char* text);
Foo_table_t get_foo(OB_cursor* cursor, uint64_t id);

#endif //PROJECT_C_TEST_OBJECTS_H
