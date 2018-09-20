#include <string.h>

#include "objectbox.h"
#include "c_test_builder.h"
#include "c_test_objects.h"

#include "query-test.h"

int printError() {
    printf("Unexpected error: %d, %d (%s)\n", obx_last_error_code(), obx_last_error_secondary(),
           obx_last_error_message());
    return obx_last_error_code();
}

OBX_model* createModel() {
    OBX_model* model = obx_model_create();
    if (!model) {
        return NULL;
    }
    uint64_t uid = 1000;
    uint64_t fooUid = uid++;
    uint64_t fooIdUid = uid++;
    uint64_t fooTextUid = uid++;

    if (obx_model_entity(model, "Foo", FOO_entity, fooUid)
        || obx_model_property(model, "id", PropertyType_Long, FOO_prop_id, fooIdUid)
            || obx_model_property_flags(model, PropertyFlags_ID)
        || obx_model_property(model, "text", PropertyType_String, FOO_prop_text, fooTextUid)
        || obx_model_entity_last_property_id(model, FOO_prop_text, fooTextUid)) {

        obx_model_destroy(model);
        return NULL;
    }

    uint64_t barUid = uid++;
    uint64_t barIdUid = uid++;
    uint64_t barTextUid = uid++;
    uint64_t barFooIdUid = uid++;
    uint64_t relUid = uid++;
    uint64_t relIndex = 1;
    uint64_t relIndexUid = uid++;


    if (obx_model_entity(model, "Bar", BAR_entity, barUid)
        || obx_model_property(model, "id", PropertyType_Long, BAR_prop_id, barIdUid)
            || obx_model_property_flags(model, PropertyFlags_ID)
        || obx_model_property(model, "text", PropertyType_String, BAR_prop_text, barTextUid)
        || obx_model_property(model, "fooId", PropertyType_Relation, BAR_prop_id_foo, barFooIdUid)
           || obx_model_property_relation(model, "Foo", relIndex, relIndexUid)
        || obx_model_entity_last_property_id(model, BAR_prop_id_foo, barFooIdUid)) {

        obx_model_destroy(model);
        return NULL;
    }

    obx_model_last_relation_id(model, BAR_rel_foo, relUid);
    obx_model_last_index_id(model, relIndex, relIndexUid);
    obx_model_last_entity_id(model, BAR_entity, barUid);
    return model;
}

int testVersion() {
    if (obx_version_is_at_least(999, 0, 0)) return 999;
    if (obx_version_is_at_least(OBX_VERSION_MAJOR, OBX_VERSION_MINOR, OBX_VERSION_PATCH + 1)) return 1;
    if (!obx_version_is_at_least(OBX_VERSION_MAJOR, OBX_VERSION_MINOR, OBX_VERSION_PATCH)) return 2;
    if (!obx_version_is_at_least(0, 1, 0)) return 3;
    if (!obx_version_is_at_least(0, 0, 1)) return 4;
    int major = 99, minor = 99, patch = 99;
    obx_version(&major, &minor, &patch);
    if (major != OBX_VERSION_MAJOR || minor != OBX_VERSION_MINOR || patch != OBX_VERSION_PATCH) return 5;
    return 0;
}

int testOpenWithNullBytesError() {
    OBX_store* store = obx_store_open_bytes(NULL, 0, NULL);
    if (store) {
        obx_store_close(store);
        return -1;
    }
    printf("Error message set: %s\n", obx_last_error_message());
    if (obx_last_error_code() != OBX_ERROR_ILLEGAL_ARGUMENT) {
        return 1;
    }
    obx_last_error_clear();
    return OBX_SUCCESS;
}

int testCursorStuff(OBX_cursor* cursor) {
    uint64_t id = obx_cursor_id_for_put(cursor, 0);
    if (!id) return printError();
    const char* hello = "Hello C!\0\0\0\0"; // Trailing zeros as padding (put rounds up to next %4 length)
    printf("Putting data at ID %ld\n", (long) id);
    size_t size = strlen(hello) + 1;
    if (obx_cursor_put(cursor, id, hello, size, 0)) return printError();

    void* dataRead;
    size_t sizeRead;
    if (obx_cursor_get(cursor, id, &dataRead, &sizeRead)) return printError();
    printf("Data read from ID %ld: %s\n", (long) id, (char*) dataRead);

    int rc = obx_cursor_get(cursor, id + 1, &dataRead, &sizeRead);
    if (rc != OBX_NOT_FOUND) {
        printf("Get expected OBX_NOT_FOUND, but got %d\n", rc);
        return 1;
    }

    uint64_t count = 0;
    if (obx_cursor_count(cursor, &count)) return printError();
    printf("Count: %ld\n", (long) count);
    if (obx_cursor_remove(cursor, id)) return printError();
    if (obx_cursor_count(cursor, &count)) return printError();
    printf("Count after remove: %ld\n", (long) count);

    rc = obx_cursor_remove(cursor, id);
    if (rc != OBX_NOT_FOUND) {
        printf("Remove expected OBX_NOT_FOUND, but got %d\n", rc);
        return 1;
    }

    return OBX_SUCCESS;
}

int testQueryNoData(OBX_cursor* cursor) {
    OBX_bytes_array* bytesArray = obx_query_by_string(cursor, 2, "dummy");
    if (!bytesArray) {
        printf("Query failed\n");
        return -99;
    }
    if (bytesArray->bytes) {
        printf("Query tables value\n");
        return -98;
    }
    if (bytesArray->size) {
        printf("Query table size value\n");
        return -98;
    }
    obx_bytes_array_destroy(bytesArray);
    return OBX_SUCCESS;
}

int testCursorMultiple(OBX_cursor* cursor) {
    // Trailing zeros as padding (put rounds up to next %4 length)
    const char* data1 = "Apple\0\0\0\0";
    const char* data2 = "Banana\0\0\0\0";
    const char* data3 = "Mango\0\0\0\0";

    uint64_t id1 = obx_cursor_id_for_put(cursor, 0);
    uint64_t id2 = obx_cursor_id_for_put(cursor, 0);
    uint64_t id3 = obx_cursor_id_for_put(cursor, 0);
    if (obx_cursor_put(cursor, id1, data1, strlen(data1) + 1, 0)) return printError();
    if (obx_cursor_put(cursor, id2, data2, strlen(data2) + 1, 0)) return printError();
    if (obx_cursor_put(cursor, id3, data3, strlen(data3) + 1, 0)) return printError();
    printf("Put at ID %ld, %ld, and %ld\n", id1, id2, id3);

    void* dataRead;
    size_t sizeRead;
    if (obx_cursor_first(cursor, &dataRead, &sizeRead)) return printError();
    printf("Data1 read: %s\n", (char*) dataRead);
    if (obx_cursor_next(cursor, &dataRead, &sizeRead)) return printError();
    printf("Data2 read: %s\n", (char*) dataRead);
    if (obx_cursor_next(cursor, &dataRead, &sizeRead)) return printError();
    printf("Data3 read: %s\n", (char*) dataRead);

    int rc = obx_cursor_next(cursor, &dataRead, &sizeRead);
    if (rc != OBX_NOT_FOUND) {
        printf("Next expected OBX_NOT_FOUND, but got %d\n", rc);
        return 1;
    }

    if (obx_cursor_remove_all(cursor)) return printError();
    uint64_t count = 0;
    if (obx_cursor_count(cursor, &count)) return printError();
    printf("Count after remove all: %ld\n", (long) count);
    if (count) return (int) count;

    return OBX_SUCCESS;
}

int testBoxStuff(OBX_box* box) {
    uint64_t id = obx_box_id_for_put(box, 0);
    if (!id) return printError();
    const char* hello = "Hello asnyc box!\0\0\0\0"; // Trailing zeros as padding (put rounds up to next %4 length)
    printf("Putting data at ID %ld\n", (long) id);
    size_t size = strlen(hello) + 1;
    if (obx_box_put_async(box, id, hello, size, 0)) return printError();

    return OBX_SUCCESS;
}

int testFlatccRoundtrip() {
    flatcc_builder_t builder;
    int rc;
    if ((rc = create_foo(&builder, 42, "bar"))) goto err;

    size_t size;
    void* buffer = flatcc_builder_get_direct_buffer(&builder, &size);
    if (!buffer) {
        rc = -1;
        goto err;
    }

    Foo_table_t table = Foo_as_root(buffer);
    assert(Foo_id(table) == 42);
    assert(strcmp(Foo_text(table), "bar") == 0);

    flatcc_builder_clear(&builder);
    return 0;

    err:
    printf("testFlatccRoundtrip error: %d", rc);
    return rc;
}

int testPutAndGetFlatObjects(OBX_cursor* cursor) {
    int rc;
    uint64_t id = 0;

    if ((rc = put_foo(cursor, &id, "bar"))) goto err;
    Foo_table_t table = get_foo(cursor, id);
    if (table == NULL) {
        rc = -1;
        goto err;
    }
    assert(Foo_id(table) == id);
    assert(strcmp(Foo_text(table), "bar") == 0);
    return 0;

    err:
    printf("testPutAndGetFlatObjects error: %d", rc);
    return rc;
}

int testQueryFlatObjects(OBX_cursor* cursor) {
    int rc;
    uint64_t id1 = 0, id2 = 0, id3 = 0;

    if ((rc = obx_cursor_remove_all(cursor))) goto err;

    if ((rc = put_foo(cursor, &id1, "foo"))) goto err;
    if ((rc = put_foo(cursor, &id2, "bar"))) goto err;
    if ((rc = put_foo(cursor, &id3, "foo"))) goto err;

    uint64_t count = 0;
    if ((rc = obx_cursor_count(cursor, &count))) goto err;
    assert(count == 3);

    OBX_bytes_array* resultNone = obx_query_by_string(cursor, 2, "nothing here");
    assert(resultNone);
    assert(resultNone->size == 0);
    obx_bytes_array_destroy(resultNone);

    OBX_bytes_array* resultFoo = obx_query_by_string(cursor, 2, "foo");
    assert(resultFoo);
    assert(resultFoo->size == 2);
    Foo_table_t foo1 = Foo_as_root(resultFoo->bytes[0].data);
    assert(Foo_id(foo1) == id1);
    assert(strcmp(Foo_text(foo1), "foo") == 0);
    Foo_table_t foo2 = Foo_as_root(resultFoo->bytes[1].data);
    assert(Foo_id(foo2) == id3);
    assert(strcmp(Foo_text(foo2), "foo") == 0);
    obx_bytes_array_destroy(resultFoo);

    OBX_bytes_array* resultBar = obx_query_by_string(cursor, 2, "bar");
    assert(resultBar);
    assert(resultBar->size == 1);
    Foo_table_t bar = Foo_as_root(resultBar->bytes[0].data);
    assert(Foo_id(bar) == id2);
    assert(strcmp(Foo_text(bar), "bar") == 0);
    obx_bytes_array_destroy(resultBar);

    return 0;

    err:
    printf("testPutAndGetFlatObjects error: %d", rc);
    return rc;
}

int testBacklink(OBX_cursor* cursor_foo, OBX_cursor* cursor_bar) {
    int rc;
    uint64_t fid1 = 0, fid2 = 0, fid3 = 0;
    uint64_t bid1 = 0, bid2 = 0, bid3 = 0;

    if ((rc = obx_cursor_remove_all(cursor_foo))) goto err;

    if ((rc = put_foo(cursor_foo, &fid1, "foo1"))) goto err;
    if ((rc = put_foo(cursor_foo, &fid2, "foo2"))) goto err;
    if ((rc = put_foo(cursor_foo, &fid3, "foo3"))) goto err;

    if ((rc = put_bar(cursor_bar, &bid1, "bar1", fid1))) goto err;
    if ((rc = put_bar(cursor_bar, &bid2, "bar2", fid1))) goto err;
    if ((rc = put_bar(cursor_bar, &bid3, "bar3", fid3))) goto err;

    uint64_t count = 0;
    if ((rc = obx_cursor_count(cursor_foo, &count))) goto err;
    assert(count == 3);

    if ((rc = obx_cursor_count(cursor_bar, &count))) goto err;
    assert(count == 3);

    {   //bar->foo, find in BAR cursor - trivial, not really a "backlink"
        OBX_id_array* barIds = obx_cursor_backlink_ids(cursor_bar, BAR_entity, BAR_prop_id_foo, fid1);
        assert(barIds);
        assert(barIds->size == 2);
        assert(barIds->ids[0] == bid1);
        assert(barIds->ids[1] == bid2);
        obx_id_array_destroy(barIds);
    }

    {   //bar->foo, find in FOO cursor - actual backlinks
        OBX_id_array* barIds = obx_cursor_backlink_ids(cursor_foo, BAR_entity, BAR_prop_id_foo, fid1);
        assert(barIds);
        assert(barIds->size == 2);
        assert(barIds->ids[0] == bid1);
        assert(barIds->ids[1] == bid2);
        obx_id_array_destroy(barIds);
    }

    {   //bar->foo, find in FOO cursor - actual backlinks
        OBX_bytes_array* bars = obx_cursor_backlink_bytes(cursor_foo, BAR_entity, BAR_prop_id_foo, fid3);
        assert(bars);
        assert(bars->size == 1);
        {
            Bar_table_t bar = Bar_as_root(bars->bytes[0].data);
            assert(Bar_id(bar) == bid3);
            assert(Bar_fooId(bar) == fid3);
            assert(strcmp(Bar_text(bar), "bar3") == 0);
        }

        obx_bytes_array_destroy(bars);
    }

    return 0;

    err:
    printf("testPutAndGetFlatObjects error: %d", rc);
    return rc;
}


int main(int argc, char* args[]) {
    printf("Testing libobjectbox version %s, core version: %s\n", obx_version_string(), obx_version_core_string());

    int rc = testVersion();
    if (rc) return rc;

    rc = testOpenWithNullBytesError();
    if (rc) return rc;

    OBX_model* model = createModel();
    if (!model) return printError();
    OBX_store* store = obx_store_open(model, NULL);
    if (!store) return printError();
    OBX_txn* txn = obx_txn_begin(store);
    if (!txn) return printError();
    OBX_cursor* cursor = obx_cursor_create(txn, FOO_entity);
    if (!cursor) return printError();
    OBX_cursor* cursor_bar = obx_cursor_create(txn, BAR_entity);
    if (!cursor_bar) return printError();

    // Clear any existing data
    if (obx_cursor_remove_all(cursor_bar)) return printError();
    if (obx_cursor_remove_all(cursor)) return printError();

    if ((rc = testQueryNoData(cursor))) return rc;
    if ((rc = testCursorStuff(cursor))) return rc;
    if ((rc = testCursorMultiple(cursor))) return rc;

    if ((rc = testFlatccRoundtrip())) return rc;
    if ((rc = testPutAndGetFlatObjects(cursor))) return rc;
    if ((rc = testQueryFlatObjects(cursor))) return rc;

    if ((rc = testQueries(store, cursor))) return rc;
    if ((rc = testBacklink(cursor, cursor_bar))) return rc;

    if (obx_cursor_destroy(cursor)) return printError();
    if (obx_cursor_destroy(cursor_bar)) return printError();
    if (obx_txn_commit(txn)) return printError();
    if (obx_txn_destroy(txn)) return printError();


    OBX_box* box = obx_box_create(store, 1);
    if (!box) return printError();
    if (obx_store_debug_flags(store, DebugFlags_LOG_ASYNC_QUEUE)) return printError();
    if ((rc = testBoxStuff(box))) return rc;
    if (obx_box_destroy(box)) return printError();
    if (obx_store_await_async_completion(store)) return printError();

    if (obx_store_close(store)) return printError();

    return 0;
}