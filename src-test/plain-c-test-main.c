#include <inttypes.h>
#include <string.h>

#include "c_test_builder.h"
#include "c_test_objects.h"
#include "objectbox.h"
#include "query-test.h"

obx_err printError() {
    printf("Unexpected error: %d, %d (%s)\n", obx_last_error_code(), obx_last_error_secondary(),
           obx_last_error_message());
    return obx_last_error_code();
}

OBX_model* createModel() {
    OBX_model* model = obx_model_create();
    if (!model) {
        return NULL;
    }
    obx_uid uid = 1000;
    obx_uid fooUid = uid++;
    obx_uid fooIdUid = uid++;
    obx_uid fooTextUid = uid++;

    if (obx_model_entity(model, "Foo", FOO_entity, fooUid) ||
        obx_model_property(model, "id", PropertyType_Long, FOO_prop_id, fooIdUid) ||
        obx_model_property_flags(model, PropertyFlags_ID) ||
        obx_model_property(model, "text", PropertyType_String, FOO_prop_text, fooTextUid) ||
        obx_model_entity_last_property_id(model, FOO_prop_text, fooTextUid)) {
        obx_model_free(model);
        return NULL;
    }

    obx_uid barUid = uid++;
    obx_uid barIdUid = uid++;
    obx_uid barTextUid = uid++;
    obx_uid barFooIdUid = uid++;
    obx_uid relUid = uid++;
    obx_uid relIndex = 1;
    obx_uid relIndexUid = uid++;

    if (obx_model_entity(model, "Bar", BAR_entity, barUid) ||
        obx_model_property(model, "id", PropertyType_Long, BAR_prop_id, barIdUid) ||
        obx_model_property_flags(model, PropertyFlags_ID) ||
        obx_model_property(model, "text", PropertyType_String, BAR_prop_text, barTextUid) ||
        obx_model_property(model, "fooId", PropertyType_Relation, BAR_prop_id_foo, barFooIdUid) ||
        obx_model_property_relation(model, "Foo", relIndex, relIndexUid) ||
        obx_model_entity_last_property_id(model, BAR_prop_id_foo, barFooIdUid)) {
        obx_model_free(model);
        return NULL;
    }

    obx_model_last_relation_id(model, BAR_rel_foo, relUid);
    obx_model_last_index_id(model, relIndex, relIndexUid);
    obx_model_last_entity_id(model, BAR_entity, barUid);
    return model;
}

obx_err testVersion() {
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

obx_err testOpenWithNullBytesError() {
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

obx_err testCursorStuff(OBX_cursor* cursor) {
    obx_id id = obx_cursor_id_for_put(cursor, 0);
    if (!id) return printError();
    const char* hello = "Hello C!\0\0\0\0";  // Trailing zeros as padding (put rounds up to next %4 length)
    printf("Putting data at ID %ld\n", (long) id);
    size_t size = strlen(hello) + 1;
    if (obx_cursor_put(cursor, id, hello, size, 0)) return printError();

    void* dataRead;
    size_t sizeRead;
    if (obx_cursor_get(cursor, id, &dataRead, &sizeRead)) return printError();
    printf("Data read from ID %ld: %s\n", (long) id, (char*) dataRead);

    obx_err rc = obx_cursor_get(cursor, id + 1, &dataRead, &sizeRead);
    if (rc != OBX_NOT_FOUND) {
        printf("Get expected OBX_NOT_FOUND, but got %d\n", rc);
        return 1;
    }

    OBX_bytes_array* bytesArray = obx_cursor_get_all(cursor);
    if (bytesArray == NULL || bytesArray->count != 1) {
        printf("obx_cursor_get_all did not return one result: %d\n", rc);
        return 1;
    }
    obx_bytes_array_free(bytesArray);

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

obx_err testQueryNoData(OBX_cursor* cursor, OBX_store* store) {
    OBX_query_builder* builder = obx_qb_create(store, 1);
    assert(builder);

    OBX_query* query = obx_query_create(builder);
    assert(query);

    OBX_bytes_array* bytesArray = obx_query_find(query, cursor);
    if (!bytesArray) {
        printf("Query failed\n");
        return -99;
    }
    if (bytesArray->bytes) {
        printf("Query tables value\n");
        return -98;
    }
    if (bytesArray->count) {
        printf("Query table size value\n");
        return -98;
    }
    obx_bytes_array_free(bytesArray);
    obx_query_close(query);
    obx_qb_close(builder);
    return OBX_SUCCESS;
}

obx_err testCursorMultiple(OBX_cursor* cursor) {
    // Trailing zeros as padding (put rounds up to next %4 length)
    const char* data1 = "Apple\0\0\0\0";
    const char* data2 = "Banana\0\0\0\0";
    const char* data3 = "Mango\0\0\0\0";

    obx_id id1 = obx_cursor_id_for_put(cursor, 0);
    obx_id id2 = obx_cursor_id_for_put(cursor, 0);
    obx_id id3 = obx_cursor_id_for_put(cursor, 0);
    if (obx_cursor_put(cursor, id1, data1, strlen(data1) + 1, 0)) return printError();
    if (obx_cursor_put(cursor, id2, data2, strlen(data2) + 1, 0)) return printError();
    if (obx_cursor_put(cursor, id3, data3, strlen(data3) + 1, 0)) return printError();
    printf("Put at ID %" PRIu64 ", %" PRIu64 ", and %" PRIu64 "\n", id1, id2, id3);

    void* dataRead;
    size_t sizeRead;
    if (obx_cursor_first(cursor, &dataRead, &sizeRead)) return printError();
    printf("Data1 read: %s\n", (char*) dataRead);
    if (obx_cursor_next(cursor, &dataRead, &sizeRead)) return printError();
    printf("Data2 read: %s\n", (char*) dataRead);
    if (obx_cursor_next(cursor, &dataRead, &sizeRead)) return printError();
    printf("Data3 read: %s\n", (char*) dataRead);

    obx_err rc = obx_cursor_next(cursor, &dataRead, &sizeRead);
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

obx_err testBoxStuff(OBX_box* box) {
    obx_id id = obx_box_id_for_put(box, 0);
    if (!id) return printError();
    const char* hello = "Hello asnyc box!\0\0\0\0";  // Trailing zeros as padding (put rounds up to next %4 length)
    printf("Putting data at ID %ld\n", (long) id);
    size_t size = strlen(hello) + 1;
    if (obx_box_put_async(box, id, hello, size, 0)) return printError();

    return OBX_SUCCESS;
}

obx_err testFlatccRoundtrip() {
    flatcc_builder_t builder;
    obx_err rc;
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
    printf("%s error: %d\n", __func__, rc);
    return rc;
}

obx_err testPutAndGetFlatObjects(OBX_cursor* cursor) {
    obx_err rc;
    obx_id id = 0;

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
    printf("%s error: %d\n", __func__, rc);
    return rc;
}

obx_err testBacklink(OBX_cursor* cursor_foo, OBX_cursor* cursor_bar) {
    obx_err rc;
    obx_id fid1 = 0, fid2 = 0, fid3 = 0;
    obx_id bid1 = 0, bid2 = 0, bid3 = 0;

    if ((rc = obx_cursor_remove_all(cursor_foo))) goto err;
    if ((rc = obx_cursor_remove_all(cursor_bar))) goto err;

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

    {  // bar->foo, find in BAR cursor - trivial, not really a "backlink"
        OBX_id_array* barIds = obx_cursor_backlink_ids(cursor_bar, BAR_entity, BAR_prop_id_foo, fid1);
        assert(barIds);
        assert(barIds->count == 2);
        assert(barIds->ids[0] == bid1);
        assert(barIds->ids[1] == bid2);
        obx_id_array_free(barIds);
    }

    {  // bar->foo, find in FOO cursor - actual backlinks
        OBX_id_array* barIds = obx_cursor_backlink_ids(cursor_foo, BAR_entity, BAR_prop_id_foo, fid1);
        assert(barIds);
        assert(barIds->count == 2);
        assert(barIds->ids[0] == bid1);
        assert(barIds->ids[1] == bid2);
        obx_id_array_free(barIds);
    }

    {  // bar->foo, find in FOO cursor - actual backlinks
        OBX_bytes_array* bars = obx_cursor_backlink_bytes(cursor_foo, BAR_entity, BAR_prop_id_foo, fid3);
        assert(bars);
        assert(bars->count == 1);
        {
            Bar_table_t bar = Bar_as_root(bars->bytes[0].data);
            assert(Bar_id(bar) == bid3);
            assert(Bar_fooId(bar) == fid3);
            assert(strcmp(Bar_text(bar), "bar3") == 0);
        }

        obx_bytes_array_free(bars);
    }

    return 0;

err:
    printf("%s error: %d\n", __func__, rc);
    return rc;
}

int main(int argc, char* args[]) {
    printf("Testing libobjectbox version %s, core version: %s\n", obx_version_string(), obx_version_core_string());

    OBX_store* store = NULL;
    OBX_txn* txn = NULL;
    OBX_cursor* cursor = NULL;
    OBX_cursor* cursor_bar = NULL;
    OBX_box* box = NULL;

    obx_err rc = testVersion();
    if (rc) return rc;

    rc = testOpenWithNullBytesError();
    if (rc) return rc;

    // Firstly, we need to create a model for our data and the store
    {
        OBX_model* model = createModel();
        if (!model) goto handle_error;

        store = obx_store_open(model, NULL);
        if (!store) goto handle_error;

        // model is freed by the obx_store_open(), we can't access it anymore
    }

    txn = obx_txn_begin(store);
    if (!txn) goto handle_error;

    cursor = obx_cursor_create(txn, FOO_entity);
    if (!cursor) goto handle_error;
    cursor_bar = obx_cursor_create(txn, BAR_entity);
    if (!cursor_bar) goto handle_error;

    // Clear any existing data
    if (obx_cursor_remove_all(cursor_bar)) goto handle_error;
    if (obx_cursor_remove_all(cursor)) goto handle_error;

    if ((rc = testQueryNoData(cursor, store))) goto handle_error;
    if ((rc = testCursorStuff(cursor))) goto handle_error;
    if ((rc = testCursorMultiple(cursor))) goto handle_error;

    if ((rc = testFlatccRoundtrip())) goto handle_error;
    if ((rc = testPutAndGetFlatObjects(cursor))) goto handle_error;

    if ((rc = testQueries(store, cursor))) goto handle_error;
    // TODO temporarily disabled due to issue #10
    // if ((rc = testBacklink(cursor, cursor_bar))) goto handle_error;

    if (obx_cursor_close(cursor)) goto handle_error;
    if (obx_cursor_close(cursor_bar)) goto handle_error;
    if (obx_txn_commit(txn)) goto handle_error;
    if (obx_txn_close(txn)) goto handle_error;

    box = obx_box_create(store, 1);
    if (!box) goto handle_error;
    if (obx_store_debug_flags(store, DebugFlags_LOG_ASYNC_QUEUE)) goto handle_error;
    if ((rc = testBoxStuff(box))) goto handle_error;
    if (obx_box_close(box)) goto handle_error;
    if (obx_store_await_async_completion(store)) goto handle_error;

    if (obx_store_close(store)) goto handle_error;

    return 0;

// print error and cleanup on error
handle_error:
    if (!rc) rc = -1;
    printError();

    // cleanup anything remaining
    if (cursor) {
        obx_cursor_close(cursor);
    }
    if (txn) {
        obx_txn_close(txn);
    }
    if (box) {
        obx_box_close(box);
    }
    if (store) {
        obx_store_await_async_completion(store);
        obx_store_close(store);
    }
    return rc;
}