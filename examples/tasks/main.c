/*
 * Copyright 2018-2019 ObjectBox Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "objectbox.h"

// Flatbuffers builder
#include "task_builder.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if !defined(_MSC_VER)
#include <libgen.h>
#endif

#define DATE_FORMAT_STRING "%Y-%m-%d %H:%M:%S"
#define DATE_BUFFER_LENGTH 100

// For this example, the database schema will contain one entity type, "Task,"
// and we'll give it the arbitrary local id 1
static const obx_schema_id TASK_SCHEMA_ENTITY_ID = 1;

// List of action codes returned by parse_action
#define ACTION_NEW 1
#define ACTION_DONE 2
#define ACTION_LIST_OPEN 3
#define ACTION_LIST_DONE 4
#define ACTION_HELP 9

// Utility functions
OBX_model* model_create();
int parse_action(int argc, char* argv[]);
int parse_text(int argc, char** argv, char** outText);
uint64_t timestamp_now();
int task_build(void** out_buff, size_t* out_size, obx_id id, const char* text, uint64_t date_created,
               uint64_t date_finished);
void date_to_str(char* buff, uint64_t timestamp);

// functions to handle each requested action
void do_action_help(char* program_path);
int do_action_new(OBX_store* store, int argc, char* argv[]);
int do_action_done(OBX_store* store, int argc, char* argv[]);
int do_action_list(OBX_store* store, bool list_open);

//--------------------------------------------------------------------------------------------------------------------
// main - parse the action from the command line. If required, open an ObjectBox store,
// call the corresponding action function and then clean up the store, printing out
// the error code, if an error has occurred
//--------------------------------------------------------------------------------------------------------------------

OBX_store* store_open();

int main(int argc, char* argv[]) {
    obx_err rc = 0;

    // print version of ObjectBox in use
    printf("Using libobjectbox version %s, core version: %s\n", obx_version_string(), obx_version_core_string());

    // determine requested action
    int action = parse_action(argc, argv);

    // early out if we're just printing the usage
    if (action == ACTION_HELP) {
        do_action_help(argv[0]);
        return rc;
    }

    // An OBX_store represents the database, so let's open one
    OBX_store* store = store_open();
    if (store == NULL) {
        printf("Could not open store: %s (%d)\n", obx_last_error_message(), obx_last_error_code());
        return 1;
    }

    switch (action) {
        case ACTION_NEW:
            rc = do_action_new(store, argc, argv);
            break;
        case ACTION_DONE:
            rc = do_action_done(store, argc, argv);
            break;
        case ACTION_LIST_OPEN:
            rc = do_action_list(store, true);
            break;
        case ACTION_LIST_DONE:
            rc = do_action_list(store, false);
            break;
        default:
            rc = 42;
            printf("Internal error - requested action not handled\n");
            break;
    }

    if (obx_last_error_code()) {
        printf("Last error: %s (%d)\n", obx_last_error_message(), obx_last_error_code());
    }

    obx_store_close(store);

    return rc;
}

//--------------------------------------------------------------------------------------------------------------------
// Opening a store. The store requires a model that describes the schema of the database.
//--------------------------------------------------------------------------------------------------------------------

OBX_store* store_open() {
    // Firstly, create our model
    OBX_model* model = model_create();
    if (!model) {
        return NULL;
    }

    // As we're not doing anything fancy here, we'll just use the default options...
    OBX_store_options* opt = obx_opt();

    // ...but we need to set our model in the options for the store.
    obx_opt_model(opt, model);

    // And open the store. Note that the model is freed by obx_store_open(), even
    // in the case of failure, so we don't need to free it here
    return obx_store_open(opt);
}

//--------------------------------------------------------------------------------------------------------------------
// do_action_help - print out the expected usage of the example
//--------------------------------------------------------------------------------------------------------------------

void do_action_help(char* program_path) {
#ifndef _MSC_VER  // Windows is not UNIX
    program_path = basename(program_path);
#endif
    printf("usage: %s\n", program_path);
    const char* format = "    %-30s %s\n";
    printf(format, "text of a new task", "create a new task with the given text");
    printf(format, "", "(default) lists active tasks");
    printf(format, "--list", "lists active and done tasks");
    printf(format, "--done ID", "marks the task with the given ID as done");
    printf(format, "--help", "displays this help");
}

//--------------------------------------------------------------------------------------------------------------------
// do_action_new - create a new task entity and add it to the database
//--------------------------------------------------------------------------------------------------------------------

int do_action_new(OBX_store* store, int argc, char* argv[]) {
    char* text = NULL;
    void* buff = NULL;
    size_t size = 0;
    OBX_txn* txn = NULL;
    OBX_cursor* cursor = NULL;

    // grab the task text from the command line
    if (parse_text(argc, argv, &text) <= 0) {
        return -1;
    }

    // All access to the database is performed through a transaction. In this case, as
    // we're adding a new entity, we need a write transaction
    txn = obx_txn_write(store);
    if (!txn) {
        goto clean_up;
    }

    // Within the transaction, access to the data objects is performed through a
    // cursor instance, which is tied to our "Task" entity objects
    cursor = obx_cursor(txn, TASK_SCHEMA_ENTITY_ID);
    if (!cursor) {
        goto clean_up;
    }

    // Get an ID for our soon-to-be-created task entity
    obx_id id = obx_cursor_id_for_put(cursor, 0);
    if (!id) {
        goto clean_up;
    }

    // Create a flatbuffers representation of the entity. Note that task_build pads the
    // buffer to 4 byte aligned, as required by ObjectBox
    if (task_build(&buff, &size, id, text, timestamp_now(), 0)) {
        goto clean_up;
    }

    // And add it to the database. ObjectBox requires that the entities' data buffers are padded
    // to be a whole multiple of four bytes. As task_build does not do that, we use obx_cursor_put_padded
    // which pads the buffer if necessary. If the buffer were guaranteed to a whole multiple of 4 bytes in
    // size, then we could use obx_cursor_put instead
    if (obx_cursor_put_padded(cursor, id, buff, size, 0)) {
        goto clean_up;
    }

clean_up:
    if (!obx_last_error_code()) {
        printf("New task: %" PRIu64 " - %s\n", id, text);
    } else {
        printf("Failed to create the task\n");
    }

    if (cursor) {
        obx_cursor_close(cursor);
    }

    if (txn && !obx_last_error_code()) {
        obx_txn_success(txn);
    }

    if (txn) {
        obx_txn_close(txn);
    }

    free(text);
    free(buff);

    return obx_last_error_code();
}

//--------------------------------------------------------------------------------------------------------------------
// do_action_done - mark an extant task entity as done
//--------------------------------------------------------------------------------------------------------------------

int do_action_done(OBX_store* store, int argc, char* argv[]) {
    void* old_data = NULL;
    size_t old_size = 0;
    void* new_data = NULL;
    size_t new_size = 0;
    OBX_txn* txn = NULL;
    OBX_cursor* cursor = NULL;

    // grab the id from the command line
    obx_id id = atol(argv[2]);
    if (!id) {
        printf("Error parsing ID \"%s\" as a number\n", argv[2]);
        return -1;
    }

    txn = obx_txn_write(store);
    if (!txn) goto clean_up;

    cursor = obx_cursor(txn, TASK_SCHEMA_ENTITY_ID);
    if (!cursor) goto clean_up;

    // First, we read the entity back from the cursor
    if (obx_cursor_get(cursor, id, &old_data, &old_size)) {
        goto clean_up;
    }

    // grab the flat buffers representation and check to see if it's already marked done
    Task_table_t task = Task_as_root(old_data);

    if (Task_date_finished(task)) {
        printf("Task %ld has already been done\n", (long) id);
    } else {
        // It's not been marked done. Rebuild the entity, marked as done, and use the cursor to overwrite it
        printf("Setting task %ld as done\n", (long) id);
        if (task_build(&new_data, &new_size, Task_id(task), Task_text(task), Task_date_created(task),
                       timestamp_now())) {
            goto clean_up;
        }

        if (obx_cursor_put_padded(cursor, id, new_data, new_size, 0)) {
            goto clean_up;
        }
    }

clean_up:
    if (obx_last_error_code()) {
        printf("Failed to mark the task as done\n");
    }

    if (cursor) {
        obx_cursor_close(cursor);
    }

    if (txn && !obx_last_error_code()) {
        obx_txn_success(txn);
    }

    if (txn) {
        obx_txn_close(txn);
    }

    // old_data belongs to the database, do not free it here
    if (new_data) free(new_data);

    return obx_last_error_code();
}

//--------------------------------------------------------------------------------------------------------------------
// do_action_list - list all the task entities, open or done, depending on list_open
//--------------------------------------------------------------------------------------------------------------------

int do_action_list(OBX_store* store, bool list_open) {
    OBX_txn* txn = NULL;
    OBX_cursor* cursor = NULL;

    // Note that this time we are using a read transaction
    txn = obx_txn_read(store);
    if (!txn) goto clean_up;

    cursor = obx_cursor(txn, TASK_SCHEMA_ENTITY_ID);
    if (!cursor) goto clean_up;

    // A nice header for the table
    printf("%3s  %-19s  %-19s  %s\n", "ID", "Created", "Finished", "Text");

    // grab the first entity from the cursor
    void* data;
    size_t size;
    bool found = false;

    int rc = obx_cursor_first(cursor, &data, &size);
    while (!rc) {
        Task_table_t task = Task_as_root(data);

        if ((Task_date_finished(task) == 0) == list_open) {
            found = true;

            char date_created[DATE_BUFFER_LENGTH];
            date_to_str(date_created, Task_date_created(task));

            char date_finished[DATE_BUFFER_LENGTH];
            date_to_str(date_finished, Task_date_finished(task));

            printf("%3ld  %-19s  %-19s  %s\n", (long) Task_id(task), date_created, date_finished, Task_text(task));
        }

        // move the cursor to the next entity
        rc = obx_cursor_next(cursor, &data, &size);
    }

    if (rc == OBX_NOT_FOUND) {
        if (!found) {
            printf("There are no tasks\n");
        }
    } else {
        printf("Failed to list the tasks\n");
    }

clean_up:
    if (cursor) obx_cursor_close(cursor);
    if (txn) obx_txn_close(txn);

    return obx_last_error_code();
}

//--------------------------------------------------------------------------------------------------------------------
// Utility functions
//--------------------------------------------------------------------------------------------------------------------

OBX_model* model_create() {
    OBX_model* model = obx_model();
    if (!model) {
        return NULL;
    }

    obx_model_entity(model, "Task", TASK_SCHEMA_ENTITY_ID, 10001);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 100010001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "text", OBXPropertyType_String, 2, 100010002);
    obx_model_property(model, "date_created", OBXPropertyType_Date, 3, 100010003);
    obx_model_property(model, "date_finished", OBXPropertyType_Date, 4, 100010004);
    obx_model_entity_last_property_id(model, 4, 100010004);
    obx_model_last_entity_id(model, TASK_SCHEMA_ENTITY_ID, 10001);

    if (obx_model_error_code(model)) {
        obx_model_free(model);
        return NULL;
    }

    return model;
}

int parse_text(int argc, char** argv, char** outText) {
    int size = 0;
    int i;  // Avoid "‘for’ loop initial declarations are only allowed in C99 or C11 mode" with older compilers

    size += argc - 2;  // number of spaces between words
    for (i = 1; i < argc; i++) {
        size += (int) strlen(argv[i]);
    }
    assert(size >= 0);
    if (size == 0) {
        printf("No task text given\n");
        return -1;
    }

    *outText = (char*) malloc(sizeof(char) * (size + 1));
    if (!*outText) {
        printf("Could not process task text\n");
        return -1;
    }

    char* p = *outText;
    for (i = 1; i < argc; i++) {
        strcpy(p, argv[i]);
        p += strlen(argv[i]);
        if (i != argc - 1) strcpy(p++, " ");
    }

    return size;
}

int task_build(void** out_buff, size_t* out_size, obx_id id, const char* text, uint64_t date_created,
               uint64_t date_finished) {
    flatcc_builder_t builder;

    // Initialize the builder object.
    flatcc_builder_init(&builder);

    obx_err rc = Task_start_as_root(&builder);
    if (!rc) rc = Task_id_add(&builder, id);
    if (!rc) rc = Task_text_create(&builder, text, strlen(text));
    if (!rc) rc = Task_date_created_add(&builder, date_created);
    if (!rc) rc = Task_date_finished_add(&builder, date_finished);
    if (!rc) Task_end_as_root(&builder);

    if (!rc) {
        void* buffer = flatcc_builder_get_direct_buffer(&builder, out_size);

        if (!buffer) {
            printf("%s error: (could not get direct buffer)\n", __FUNCTION__);
            return -1;
        }

        *out_buff = malloc(*out_size);
        if (*out_buff == NULL) {
            printf("%s error: (could not copy direct buffer)\n", __FUNCTION__);
            return -1;
        }

        memcpy(*out_buff, buffer, *out_size);
    }

    flatcc_builder_clear(&builder);
    return rc;
}

uint64_t timestamp_now() { return (uint64_t)(time(NULL) * 1000); }

void date_to_str(char* buff, uint64_t timestamp) {
    if (!timestamp) {
        // empty string
        buff[0] = '\0';
    } else {
        time_t time = (time_t)(timestamp / 1000);
        struct tm* tm_info = localtime(&time);
        strftime(buff, DATE_BUFFER_LENGTH, DATE_FORMAT_STRING, tm_info);
    }
}

obx_err parse_action(int argc, char* argv[]) {
    if (argc < 2) {
        return ACTION_LIST_OPEN;
    }

    if (strcmp("--done", argv[1]) == 0) {
        if (argc != 3) {
            return ACTION_HELP;
        } else {
            return ACTION_DONE;
        }
    }

    if (strcmp("--list", argv[1]) == 0) {
        return ACTION_LIST_DONE;
    }

    if (strcmp("--help", argv[1]) == 0 || strcmp("--usage", argv[1]) == 0) {
        return ACTION_HELP;
    }

    return ACTION_NEW;
}
