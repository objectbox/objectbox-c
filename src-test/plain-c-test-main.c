#include <string.h>

#include "objectbox.h"
#include "c_test_builder.h"
#include "c_test_objects.h"

int printError() {
    printf("Unexpected error: %d, %d (%s)\n", ob_last_error_code(), ob_last_error_secondary(), ob_last_error_message());
    return ob_last_error_code();
}

OB_model* createModel() {
    OB_model* model = ob_model_create();
    if (!model) {
        return NULL;
    }
    uint64_t uid = 1000;
    uint64_t fooUid = uid++;
    uint64_t idUid = uid++;
    uint64_t textUid = uid++;

    if (ob_model_entity(model, "Foo", 1, fooUid)
        || ob_model_property(model, "id", PropertyType_Long, 1, idUid)
        || ob_model_property_flags(model, PropertyFlags_ID)
        || ob_model_property(model, "text", PropertyType_String, 2, textUid)
        || ob_model_entity_last_property_id(model, 2, textUid)) {

        ob_model_destroy(model);
        return NULL;
    }

    ob_model_last_entity_id(model, 1, fooUid);
    return model;
}

int testOpenWithNullBytesError() {
    OB_store* store = ob_store_open_bytes(NULL, 0, NULL);
    if (store) {
        ob_store_close(store);
        return -1;
    }
    printf("Error message set: %s\n", ob_last_error_message());
    if (ob_last_error_code() != OB_ERROR_ILLEGAL_ARGUMENT) {
        return 1;
    }
    ob_last_error_clear();
    return OB_SUCCESS;
}

int testCursorStuff(OB_cursor* cursor) {
    uint64_t id = ob_cursor_id_for_put(cursor, 0);
    if (!id) { return printError(); }
    const char* hello = "Hello C!\0\0\0\0"; // Trailing zeros as padding (put rounds up to next %4 length)
    printf("Putting data at ID %ld\n", (long) id);
    size_t size = strlen(hello) + 1;
    if (ob_cursor_put(cursor, id, hello, size, 0)) { return printError(); }

    void* dataRead;
    size_t sizeRead;
    if (ob_cursor_get(cursor, id, &dataRead, &sizeRead)) { return printError(); }
    printf("Data read from ID %ld: %s\n", (long) id, (char*) dataRead);

    int rc = ob_cursor_get(cursor, id + 1, &dataRead, &sizeRead);
    if (rc != OB_NOT_FOUND) {
        printf("Get expected OB_NOT_FOUND, but got %d\n", rc);
        return 1;
    }

    uint64_t count = 0;
    if (ob_cursor_count(cursor, &count)) { return printError(); }
    printf("Count: %ld\n", (long) count);
    if (ob_cursor_remove(cursor, id)) { return printError(); }
    if (ob_cursor_count(cursor, &count)) { return printError(); }
    printf("Count after remove: %ld\n", (long) count);

    rc = ob_cursor_remove(cursor, id);
    if (rc != OB_NOT_FOUND) {
        printf("Remove expected OB_NOT_FOUND, but got %d\n", rc);
        return 1;
    }

    return OB_SUCCESS;
}

int testSimpleQueryNoData(OB_cursor* cursor) {
    OB_table_array* tableArray = ob_simple_query_string(cursor, 2, "dummy", (uint32_t) strlen("dummy"));
    if (!tableArray) {
        printf("Query failed\n");
        return -99;
    }
    if (tableArray->tables) {
        printf("Query tables value\n");
        return -98;
    }
    if (tableArray->size) {
        printf("Query table size value\n");
        return -98;
    }
    ob_table_array_destroy(tableArray);
    return OB_SUCCESS;
}

int testQueryNoData(OB_cursor* cursor) {
    OB_bytes_array* bytesArray = ob_query_by_string(cursor, 2, "dummy");
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
    ob_bytes_array_destroy(bytesArray);
    return OB_SUCCESS;
}

int testCursorMultiple(OB_cursor* cursor) {
    // Trailing zeros as padding (put rounds up to next %4 length)
    const char* data1 = "Apple\0\0\0\0";
    const char* data2 = "Banana\0\0\0\0";
    const char* data3 = "Mango\0\0\0\0";

    uint64_t id1 = ob_cursor_id_for_put(cursor, 0);
    uint64_t id2 = ob_cursor_id_for_put(cursor, 0);
    uint64_t id3 = ob_cursor_id_for_put(cursor, 0);
    if (ob_cursor_put(cursor, id1, data1, strlen(data1) + 1, 0)) { return printError(); }
    if (ob_cursor_put(cursor, id2, data2, strlen(data2) + 1, 0)) { return printError(); }
    if (ob_cursor_put(cursor, id3, data3, strlen(data3) + 1, 0)) { return printError(); }
    printf("Put at ID %ld, %ld, and %ld\n", id1, id2, id3);

    void* dataRead;
    size_t sizeRead;
    if (ob_cursor_first(cursor, &dataRead, &sizeRead)) { return printError(); }
    printf("Data1 read: %s\n", (char*) dataRead);
    if (ob_cursor_next(cursor, &dataRead, &sizeRead)) { return printError(); }
    printf("Data2 read: %s\n", (char*) dataRead);
    if (ob_cursor_next(cursor, &dataRead, &sizeRead)) { return printError(); }
    printf("Data3 read: %s\n", (char*) dataRead);

    int rc = ob_cursor_next(cursor, &dataRead, &sizeRead);
    if (rc != OB_NOT_FOUND) {
        printf("Next expected OB_NOT_FOUND, but got %d\n", rc);
        return 1;
    }

    if (ob_cursor_remove_all(cursor)) { return printError(); }
    uint64_t count = 0;
    if (ob_cursor_count(cursor, &count)) { return printError(); }
    printf("Count after remove all: %ld\n", (long) count);
    if (count) return (int) count;

    return OB_SUCCESS;
}

int testBoxStuff(OB_box* box) {
    uint64_t id = ob_box_id_for_put(box, 0);
    if (!id) { return printError(); }
    const char* hello = "Hello asnyc box!\0\0\0\0"; // Trailing zeros as padding (put rounds up to next %4 length)
    printf("Putting data at ID %ld\n", (long) id);
    size_t size = strlen(hello) + 1;
    if (ob_box_put_async(box, id, hello, size, 0)) { return printError(); }

    return OB_SUCCESS;
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

int testPutAndGetFlatObjects(OB_cursor* cursor) {
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

int testQueryFlatObjects(OB_cursor* cursor) {
    int rc;
    uint64_t id1 = 0, id2 = 0, id3 = 0;

    if ((rc = ob_cursor_remove_all(cursor))) goto err;

    if ((rc = put_foo(cursor, &id1, "foo"))) goto err;
    if ((rc = put_foo(cursor, &id2, "bar"))) goto err;
    if ((rc = put_foo(cursor, &id3, "foo"))) goto err;

    uint64_t count = 0;
    if ((rc = ob_cursor_count(cursor, &count))) goto err;
    assert(count == 3);

    OB_bytes_array* resultNone = ob_query_by_string(cursor, 2, "nothing here");
    assert(resultNone);
    assert(resultNone->size == 0);
    ob_bytes_array_destroy(resultNone);

    OB_bytes_array* resultFoo = ob_query_by_string(cursor, 2, "foo");
    assert(resultFoo);
    assert(resultFoo->size == 2);
    Foo_table_t foo1 = Foo_as_root(resultFoo->bytes[0].data);
    assert(Foo_id(foo1) == id1);
    assert(strcmp(Foo_text(foo1), "foo") == 0);
    Foo_table_t foo2 = Foo_as_root(resultFoo->bytes[1].data);
    assert(Foo_id(foo2) == id3);
    assert(strcmp(Foo_text(foo2), "foo") == 0);
    ob_bytes_array_destroy(resultFoo);

    OB_bytes_array* resultBar = ob_query_by_string(cursor, 2, "bar");
    assert(resultBar);
    assert(resultBar->size == 1);
    Foo_table_t bar = Foo_as_root(resultBar->bytes[0].data);
    assert(Foo_id(bar) == id2);
    assert(strcmp(Foo_text(bar), "bar") == 0);
    ob_bytes_array_destroy(resultBar);

    return 0;

    err:
    printf("testPutAndGetFlatObjects error: %d", rc);
    return rc;
}

int main(int argc, char* args[]) {
    int rc = testOpenWithNullBytesError();
    if (rc) return rc;

    OB_model* model = createModel();
    if (!model) { return printError(); }
    OB_store* store = ob_store_open(model, NULL);
    if (!store) { return printError(); }
    OB_txn* txn = ob_txn_begin(store);
    if (!txn) { return printError(); }
    OB_cursor* cursor = ob_cursor_create(txn, 1);
    if (!cursor) { return printError(); }

    // Clear any existing data
    if (ob_cursor_remove_all(cursor)) { return printError(); }

    rc = testSimpleQueryNoData(cursor);
    if (rc) return rc;
    rc = testQueryNoData(cursor);
    if (rc) return rc;

    rc = testCursorStuff(cursor);
    if (rc) return rc;
    rc = testCursorMultiple(cursor);
    if (rc) return rc;

    if ((rc = testFlatccRoundtrip())) return rc;
    if ((rc = testPutAndGetFlatObjects(cursor))) return rc;
    if ((rc = testQueryFlatObjects(cursor))) return rc;

    if (ob_cursor_destroy(cursor)) { return printError(); };
    if (ob_txn_commit(txn)) { return printError(); }
    if (ob_txn_destroy(txn)) { return printError(); }

    OB_box* box = ob_box_create(store, 1);
    if (!box) { return printError(); }
    if (ob_store_debug_flags(store, DebugFlags_LOG_ASYNC_QUEUE)) { return printError(); }
    rc = testBoxStuff(box);
    if (rc) return rc;
    if (ob_box_destroy(box)) { return printError(); }
    if (ob_store_await_async_completion(store)) { return printError(); }

    if (ob_store_close(store)) { return printError(); }

    return 0;
}