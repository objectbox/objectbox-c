#include <inttypes.h>
#include <string.h>

#include "objectbox-model.h"
#include "c_test.obx.h"

obx_err printError() {
    printf("Unexpected error: %d, %d (%s)\n", obx_last_error_code(), obx_last_error_secondary(),
           obx_last_error_message());
    return obx_last_error_code();
}

int put_foo(OBX_cursor* cursor, uint64_t* idInOut, char* text) {
    flatcc_builder_t builder;
    flatcc_builder_init(&builder);

    uint64_t id = *idInOut;
    int checkForPreviousValueFlag = id == 0 ? 0 : 1;

    id = obx_cursor_id_for_put(cursor, id);
    if (!id) { return -1; }

    int rc = 0;
    size_t size;
    void* buffer;

    Foo foo = {.id = id, .text = text};
    if (!Foo_to_flatbuffer(&builder, &foo, &buffer, &size)) goto err;

    rc = obx_cursor_put(cursor, id, buffer, size, checkForPreviousValueFlag);
    if (rc) goto err;
    flatcc_builder_clear(&builder);
    *idInOut = id;
    return 0;

    err:
    flatcc_builder_clear(&builder);
    if (rc == 0) return -1;
    else return rc;
}

Foo* get_foo(OBX_cursor* cursor, uint64_t id) {
    void* data;
    size_t size;
    int rc = obx_cursor_get(cursor, id, &data, &size);
    if (rc == OBX_NOT_FOUND) return NULL; // No special treatment at the moment if not found
    if (rc) return NULL;
    return Foo_new_from_flatbuffer(data, size);
}

obx_err testCursorStuff(OBX_cursor* cursor) {
    obx_id id = 0;
    int rc;

    if ((rc = put_foo(cursor, &id, "bar"))) return rc;

    void* dataRead;
    size_t sizeRead;
    if (obx_cursor_get(cursor, id, &dataRead, &sizeRead)) return printError();
    printf("%zu data bytes read from ID %ld\n", sizeRead, (long)id);

    rc = obx_cursor_get(cursor, id + 1, &dataRead, &sizeRead);
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

obx_err testPutAndGetFlatObjects(OBX_cursor* cursor) {
    obx_id id = 0;
    if (put_foo(cursor, &id, "bar")) return -1;

    Foo* foo = get_foo(cursor, id);
    if (!foo) return -1;

    assert(foo->id == id);
    assert(strcmp(foo->text, "bar") == 0);

    Foo_free(&foo);
    return 0;
}

int main(int argc, char* args[]) {
    printf("Testing libobjectbox version %s, core version: %s\n", obx_version_string(), obx_version_core_string());
    printf("Byte array support: %d\n", obx_supports_bytes_array());

    OBX_store* store = NULL;
    OBX_txn* txn = NULL;
    OBX_cursor* cursor = NULL;
    OBX_cursor* cursor_bar = NULL;
    int rc = 0;

    // Firstly, we need to create a model for our data and the store
    {
        OBX_model* model = create_obx_model();
        if (!model) goto handle_error;
        if (obx_model_error_code(model)) goto handle_error;

        OBX_store_options* opt = obx_opt();
        obx_opt_model(opt, model);
        store = obx_store_open(opt);
        if (!store) goto handle_error;

        // model is freed by the obx_store_open(), we can't access it anymore
    }

    txn = obx_txn_write(store);
    if (!txn) goto handle_error;

    cursor = obx_cursor(txn, Foo_ENTITY_ID);
    if (!cursor) goto handle_error;
    cursor_bar = obx_cursor(txn, Bar_ENTITY_ID);
    if (!cursor_bar) goto handle_error;

    // Clear any existing data
    if (obx_cursor_remove_all(cursor_bar)) goto handle_error;
    if (obx_cursor_remove_all(cursor)) goto handle_error;

    if ((rc = testCursorStuff(cursor))) goto handle_error;

    if ((rc = testPutAndGetFlatObjects(cursor))) goto handle_error;

    // TODO fix double close in handle_error if a close returns an error
    if (obx_cursor_close(cursor)) goto handle_error;
    if (obx_cursor_close(cursor_bar)) goto handle_error;
    if (obx_txn_success(txn)) goto handle_error;
    if (!obx_store_await_async_completion(store)) goto handle_error;
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
    if (store) {
        obx_store_await_async_completion(store);
        obx_store_close(store);
    }
    return rc;
}