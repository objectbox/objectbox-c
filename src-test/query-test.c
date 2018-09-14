#include "query-test.h"
#include "c_test_builder.h"
#include "c_test_objects.h"
#include <assert.h>

int testQueryBuilderError(OBX_store* store, OBX_cursor* cursor, uint32_t entity_id) {
    int rc = 0;
    OBX_query_builder* builder = obx_qb_create(store, entity_id);
    assert(builder);

    uint32_t entity_prop_id = obx_store_entity_property_id(store, entity_id, "id");
    assert(entity_prop_id);

    // comparing id (long) with a string
    int rc1 = obx_qb_string_equal(builder, entity_prop_id, "aaa", true);
    assert(rc1);
    assert(rc1 == obx_qb_error_code(builder));
    assert(rc1 == OBX_ERROR_PROPERTY_TYPE_MISMATCH);

    // check that the message is same
    const char * msg1 = obx_last_error_message();
    const char * msg2 = obx_qb_error_message(builder);
    assert(msg1);
    assert(msg2);
    assert(strcmp(msg1, msg2) == 0);

    // this should not create a query
    OBX_query* query = obx_query_create(builder);
    assert(query == NULL);

    // TODO try another type of error
//    int rc2 = obx_qb_long_equal(builder, FOO_prop_text, 1);
//    assert(rc1 != rc2);
//    // the code should stay the same as before (first error is stored)
//    assert(rc1 == obx_qb_error_code(builder));

    obx_qb_close(builder);
    obx_query_close(query);
    return rc;
}

void checkFooItem(void* data, uint64_t id, const char* text) {
    Foo_table_t foo1 = Foo_as_root(data);
    assert(Foo_id(foo1) == id);
    assert(strcmp(Foo_text(foo1), text) == 0);
}

int testQueryBuilderEqual(OBX_store* store, OBX_cursor* cursor, uint32_t entity_id) {
    int rc;

    if ((rc = obx_cursor_remove_all(cursor))) goto err;

    uint64_t id1 = 0, id2 = 0, id3 = 0;
    if ((rc = put_foo(cursor, &id1, "aaa"))) goto err;
    if ((rc = put_foo(cursor, &id2, "AAA"))) goto err;
    if ((rc = put_foo(cursor, &id3, "aaa"))) goto err;

    {   // STRING case sensitive
        OBX_query_builder* builder = obx_qb_create(store, entity_id);
        assert(builder);

        obx_qb_string_equal(builder, FOO_prop_text, "aaa", true);

        OBX_query* query = obx_query_create(builder);
        assert(query);

        OBX_bytes_array* items = obx_query_find(query, cursor);
        assert(items);
        assert(items->size == 2);
        checkFooItem(items->bytes[0].data, id1, "aaa");
        checkFooItem(items->bytes[1].data, id3, "aaa");
        obx_bytes_array_destroy(items);

        obx_qb_close(builder);
        obx_query_close(query);
    }


    {   // STRING case insensitive
        OBX_query_builder* builder = obx_qb_create(store, entity_id);
        assert(builder);

        obx_qb_string_equal(builder, FOO_prop_text, "aaa", false);

        OBX_query* query = obx_query_create(builder);
        assert(query);

        OBX_bytes_array* items = obx_query_find(query, cursor);
        assert(items);
        assert(items->size == 3);
        checkFooItem(items->bytes[0].data, id1, "aaa");
        checkFooItem(items->bytes[1].data, id2, "AAA");
        checkFooItem(items->bytes[2].data, id3, "aaa");
        obx_bytes_array_destroy(items);

        obx_qb_close(builder);
        obx_query_close(query);
    }

    {   // STRING no-match
        OBX_query_builder* builder = obx_qb_create(store, entity_id);
        assert(builder);

        obx_qb_string_equal(builder, FOO_prop_text, "you-wont-find-me", true);

        OBX_query* query = obx_query_create(builder);
        assert(query);

        OBX_bytes_array* items = obx_query_find(query, cursor);
        assert(items);
        assert(items->size == 0);
        obx_bytes_array_destroy(items);

        obx_qb_close(builder);
        obx_query_close(query);
    }

    {   // LONG
        OBX_query_builder* builder = obx_qb_create(store, entity_id);
        assert(builder);

        obx_qb_long_equal(builder, FOO_prop_id, id3);

        OBX_query* query = obx_query_create(builder);
        assert(query);

        OBX_bytes_array* items = obx_query_find(query, cursor);
        assert(items);
        assert(items->size == 1);
        checkFooItem(items->bytes[0].data, id3, "aaa");
        obx_bytes_array_destroy(items);

        obx_qb_close(builder);
        obx_query_close(query);
    }

    {   // LONG no-match
        OBX_query_builder* builder = obx_qb_create(store, entity_id);
        assert(builder);

        obx_qb_long_equal(builder, FOO_prop_id, -1);

        OBX_query* query = obx_query_create(builder);
        assert(query);

        OBX_bytes_array* items = obx_query_find(query, cursor);
        assert(items);
        assert(items->size == 0);
        obx_bytes_array_destroy(items);

        obx_qb_close(builder);
        obx_query_close(query);
    }

    return 0;

    err:
    printf("%s error: %d", __FUNCTION__, rc);
    return rc;
}

int testQueryBuilderBetween(OBX_store* store, OBX_cursor* cursor, uint32_t entity_id) {
    int rc;

    if ((rc = obx_cursor_remove_all(cursor))) goto err;

    uint64_t id1 = 0, id2 = 0, id3 = 0;
    if ((rc = put_foo(cursor, &id1, "aaa"))) goto err;
    if ((rc = put_foo(cursor, &id2, "AAA"))) goto err;
    if ((rc = put_foo(cursor, &id3, "aaa"))) goto err;

    {   // 2 and 3
        OBX_query_builder* builder = obx_qb_create(store, entity_id);
        assert(builder);

        obx_qb_long_between(builder, FOO_prop_id, id2, id3);

        OBX_query* query = obx_query_create(builder);
        assert(query);

        OBX_bytes_array* items = obx_query_find(query, cursor);
        assert(items);
        assert(items->size == 2);
        checkFooItem(items->bytes[0].data, id2, "AAA");
        checkFooItem(items->bytes[1].data, id3, "aaa");
        obx_bytes_array_destroy(items);

        obx_qb_close(builder);
        obx_query_close(query);
    }

    {   // 2 only
        OBX_query_builder* builder = obx_qb_create(store, entity_id);
        assert(builder);

        obx_qb_long_between(builder, FOO_prop_id, id2, id2);

        OBX_query* query = obx_query_create(builder);
        assert(query);

        OBX_bytes_array* items = obx_query_find(query, cursor);
        assert(items);
        assert(items->size == 1);
        checkFooItem(items->bytes[0].data, id2, "AAA");
        obx_bytes_array_destroy(items);

        obx_qb_close(builder);
        obx_query_close(query);
    }

    {   // 2 and 3
        OBX_query_builder* builder = obx_qb_create(store, entity_id);
        assert(builder);

        obx_qb_long_between(builder, FOO_prop_id, id3, id2);

        OBX_query* query = obx_query_create(builder);
        assert(query);

        OBX_bytes_array* items = obx_query_find(query, cursor);
        assert(items);
        assert(items->size == 2);
        checkFooItem(items->bytes[0].data, id2, "AAA");
        checkFooItem(items->bytes[1].data, id3, "aaa");
        obx_bytes_array_destroy(items);

        obx_qb_close(builder);
        obx_query_close(query);
    }

    return 0;

    err:
    printf("%s error: %d", __FUNCTION__, rc);
    return rc;
}

int testQueries(OBX_store* store, OBX_cursor* cursor) {
    int rc = 0;

    uint32_t entity_id = obx_store_entity_id(store, "Foo");
    if (!entity_id) return obx_last_error_code();

    if ((rc = testQueryBuilderError(store, cursor, entity_id))) return rc;
    if ((rc = testQueryBuilderEqual(store, cursor, entity_id))) return rc;
    if ((rc = testQueryBuilderBetween(store, cursor, entity_id))) return rc;
    return rc;
}