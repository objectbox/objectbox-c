#include "c_test_objects.h"

int create_foo(flatcc_builder_t* B, uint64_t id, char* text) {
    int rc;
    if ((rc = flatcc_builder_init(B))) return rc;
    if ((rc = Foo_start_as_root(B))) return rc;
    if ((rc = Foo_id_add(B, id))) return rc;
    rc = Foo_text_create(B, text, strlen(text));
    if (rc) return rc;
    flatbuffers_buffer_ref_t root = Foo_end_as_root(B);
    return 0;
}

Foo_table_t get_foo(OB_cursor* cursor, uint64_t id) {
    void* data;
    size_t size;
    int rc = ob_cursor_get(cursor, id, &data, &size);
    if (rc == 404) return NULL; // No special treatment at the moment if not found
    if (rc) return NULL;
    assert(data);
    assert(size);

    Foo_table_t table = Foo_as_root(data);
    assert(table);
    return table;
}

int put_foo(OB_cursor* cursor, uint64_t* idInOut, char* text) {
    flatcc_builder_t builder;
    uint64_t id = *idInOut;
    int checkForPreviousValueFlag = id == 0 ? 0 : 1;

    id = ob_cursor_id_for_put(cursor, id);
    if (!id) { return -1; }

    int rc = create_foo(&builder, id, text);
    if (rc) goto err;

    size_t size;
    void* buffer = flatcc_builder_get_direct_buffer(&builder, &size);
    if (!buffer) goto err;
    rc = ob_cursor_put(cursor, id, buffer, size, checkForPreviousValueFlag);
    if (rc) goto err;
    *idInOut = id;
    return 0;

    err:
    flatcc_builder_clear(&builder);
    if (rc == 0) return -1;
    else return rc;
}
