#ifndef QUERY_TEST_H
#define QUERY_TEST_H

#include "objectbox.h"

#define FOO_entity 1
#define FOO_prop_id 1
#define FOO_prop_text 2

#define BAR_entity 2
#define BAR_rel_foo 1
#define BAR_prop_id 1
#define BAR_prop_text 2
#define BAR_prop_id_foo 3

int testQueries(OBX_store* store, OBX_cursor* cursor);

#endif