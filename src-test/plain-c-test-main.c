#include <inttypes.h>
#include <string.h>

#include "c_test_builder.h"
#include "c_test_objects.h"
#include "objectbox.h"

#define FOO_entity 1
#define FOO_prop_id 1
#define FOO_prop_text 2

#define BAR_entity 2
#define BAR_rel_foo 1
#define BAR_prop_id 1
#define BAR_prop_text 2
#define BAR_prop_id_foo 3

obx_err printError() {
    printf("Unexpected error: %d, %d (%s)\n", obx_last_error_code(), obx_last_error_secondary(),
           obx_last_error_message());
    return obx_last_error_code();
}

OBX_model* createModel() {
    OBX_model* model = obx_model();
    if (!model) {
        return NULL;
    }
    obx_uid uid = 1000;
    obx_uid fooUid = uid++;
    obx_uid fooIdUid = uid++;
    obx_uid fooTextUid = uid++;

    if (obx_model_entity(model, "Foo", FOO_entity, fooUid) ||
        obx_model_property(model, "id", OBXPropertyType_Long, FOO_prop_id, fooIdUid) ||
        obx_model_property_flags(model, OBXPropertyFlags_ID) ||
        obx_model_property(model, "text", OBXPropertyType_String, FOO_prop_text, fooTextUid) ||
        obx_model_entity_last_property_id(model, FOO_prop_text, fooTextUid)) {
        obx_model_free(model);
        return NULL;
    }

    obx_uid barUid = uid++;
    obx_uid barIdUid = uid++;
    obx_uid barTextUid = uid++;
    obx_uid barFooIdUid = uid++;
    obx_uid relUid = uid++;
    obx_schema_id relIndex = 1;
    obx_uid relIndexUid = uid++;

    if (obx_model_entity(model, "Bar", BAR_entity, barUid) ||
        obx_model_property(model, "id", OBXPropertyType_Long, BAR_prop_id, barIdUid) ||
        obx_model_property_flags(model, OBXPropertyFlags_ID) ||
        obx_model_property(model, "text", OBXPropertyType_String, BAR_prop_text, barTextUid) ||
        obx_model_property(model, "fooId", OBXPropertyType_Relation, BAR_prop_id_foo, barFooIdUid) ||
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

obx_err testCursorStuff(OBX_cursor* cursor) {
    obx_id id = obx_cursor_id_for_put(cursor, 0);
    if (!id) return printError();

    flatcc_builder_t builder;

    if (create_foo(&builder, id, "bar")) {
        printf("Failed to create an object");
        flatcc_builder_clear(&builder);
        return 1;
    }

    size_t size;
    void* buffer = flatcc_builder_get_direct_buffer(&builder, &size);
    if (!buffer) {
        printf("Can't get FBB buffer");
        flatcc_builder_clear(&builder);
        return 1;
    }

    if (obx_cursor_put_new(cursor, id, buffer, size)) {
        flatcc_builder_clear(&builder);
        return printError();
    } else {
        flatcc_builder_clear(&builder);
    }

    const void* dataRead;
    size_t sizeRead;
    if (obx_cursor_get(cursor, id, &dataRead, &sizeRead)) return printError();
    printf("%zu data bytes read from ID %ld\n", sizeRead, (long)id);

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

int main(int argc, char* args[]) {
    printf("Testing libobjectbox version %s, core version: %s\n", obx_version_string(), obx_version_core_string());
    printf("Byte array support: %d\n", obx_supports_bytes_array());

    OBX_store* store = NULL;
    OBX_txn* txn = NULL;
    OBX_cursor* cursor = NULL;
    OBX_cursor* cursor_bar = NULL;

    // Firstly, we need to create a model for our data and the store
    {
        OBX_model* model = createModel();
        if (!model) goto handle_error;

        OBX_store_options* opt = obx_opt();
        obx_opt_model(opt, model);
        store = obx_store_open(opt);
        if (!store) goto handle_error;

        // model is freed by the obx_store_open(), we can't access it anymore
    }

    txn = obx_txn_write(store);
    if (!txn) goto handle_error;

    cursor = obx_cursor(txn, FOO_entity);
    if (!cursor) goto handle_error;
    cursor_bar = obx_cursor(txn, BAR_entity);
    if (!cursor_bar) goto handle_error;

    // Clear any existing data
    if (obx_cursor_remove_all(cursor_bar)) goto handle_error;
    if (obx_cursor_remove_all(cursor)) goto handle_error;

    int rc;

    if ((rc = testCursorStuff(cursor))) goto handle_error;

    if ((rc = testFlatccRoundtrip())) goto handle_error;
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