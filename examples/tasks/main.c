/*
 * Copyright 2018 ObjectBox Ltd. All rights reserved.
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <libgen.h>
#include <stdbool.h>

//region Utilities
int task_text(int argc, char** argv, char** outText) {
    int size = 0;
    size += argc - 2; // number of spaces between words
    for (int i = 1; i < argc; i++) {
        size += strlen(argv[i]);
    }

    *outText = (char*) malloc(sizeof(char) * (size + 1));
    if (!*outText) return -1;

    char* p = *outText;
    for (int i = 1; i < argc; i++) {
        strcpy(p, argv[i]);
        p += strlen(argv[i]);
        if (i != argc-1) strcpy(p++, " ");
    }

    return size;
}

uint64_t timestamp_now() {
    return (uint64_t) (time(NULL) * 1000);
}

void timestamp_parse(uint64_t timestamp, const char* date_format, size_t date_buffer_length, char** outString) {
    if (!timestamp) {
        // empty string
        memset(*outString, 0, date_buffer_length);
    } else {
        time_t time = (time_t) (timestamp / 1000);
        struct tm* tm_info = localtime(&time);
        strftime(*outString, date_buffer_length, date_format, tm_info);
    }
}

obx_err error_print() {
    printf("Unexpected error: %d, %d (%s)\n", obx_last_error_code(), obx_last_error_secondary(), obx_last_error_message());
    return obx_last_error_code();
}
//endregion

OBX_model* model_create(obx_schema_id task_entity_id) {
    OBX_model* model = obx_model_create();
    if (!model) {
        return NULL;
    }

    // create an entity for the Task
    // TASK UID must be globally unique and must not change over the life time of the entity (data loss/corruption)

    if (obx_model_entity(model, "Task", task_entity_id, 10001)
        || obx_model_property(model, "id", PropertyType_Long, 1, 100010001)
            || obx_model_property_flags(model, PropertyFlags_ID)
        || obx_model_property(model, "text", PropertyType_String, 2, 100010002)
        || obx_model_property(model, "date_created", PropertyType_Date, 3, 100010003)
        || obx_model_property(model, "date_finished", PropertyType_Date, 4, 100010004)

        // TASK that we are using the propertyId & the uid of the most recently added property
        || obx_model_entity_last_property_id(model, 4, 100010004)) {

        obx_model_free(model);
        return NULL;
    }

    obx_model_last_entity_id(model, task_entity_id, 10001);
    return model;
}

obx_err task_build(flatcc_builder_t* B, obx_id id, const char* text, uint64_t date_created, uint64_t date_finished) {
    obx_err rc;
    if ((rc = Task_start_as_root(B))) return rc;
    if ((rc = Task_id_add(B, id))) return rc;
    if ((rc = Task_text_create(B, text, strlen(text)))) return rc;
    if ((rc = Task_date_created_add(B, date_created))) return rc;
    if ((rc = Task_date_finished_add(B, date_finished))) return rc;
    flatbuffers_buffer_ref_t root = Task_end_as_root(B);
    return 0;
}

obx_err task_put(OBX_cursor* cursor, obx_id id, const char* text, uint64_t date_created, uint64_t date_finished) {
    // TASK the flatbuffer builder should be reused instead of created on demand, refer to the flatbuffer documentation for more details
    flatcc_builder_t builder;

    // Initialize the builder object.
    flatcc_builder_init(&builder);

    obx_err rc = 0;
    if ((rc = task_build(&builder, id, text, date_created, date_finished))) {
        printf("%s error: %d (task_build)\n", __FUNCTION__, rc);
    }

    if (!rc) {
        size_t size;
        void* buffer = flatcc_builder_get_direct_buffer(&builder, &size);

        if (!buffer) {
            rc = -1;
            printf("%s error: %d (could not get direct buffer)\n", __FUNCTION__, rc);

        } else {
            // write prepared buffer to the storage
            rc = obx_cursor_put(cursor, id, buffer, size, 0);
        }
    }

    flatcc_builder_clear(&builder);
    return rc;
}

obx_err task_create(OBX_cursor* cursor, char* text) {
    obx_err rc = 0;

    uint64_t count = 0;
    if ((rc = obx_cursor_count(cursor, &count))) goto finalize;

    // put a new task to the box
    {
        obx_id id = obx_cursor_id_for_put(cursor, 0);
        if (!id) {
            rc = (rc = obx_last_error_code()) ? rc : -1;
            goto finalize;
        }

        printf("New task: %ld - %s\n", (long) id, text);

        if ((rc = task_put(cursor, id, text, timestamp_now(), 0))) {
            printf("Failed to create the task\n");
            goto finalize;
        } else {
            printf("Successfully created the task\n");
        }
    }

    if ((rc = obx_cursor_count(cursor, &count))) goto finalize;
    printf("Count after insert = %ld\n", (long) count);

    finalize:
        free(text);
        return rc;
}

obx_err task_done(OBX_cursor* cursor, obx_id id) {
    obx_err rc = 0;

    void* data;
    size_t size;
    rc = obx_cursor_get(cursor, id, &data, &size);

    if (rc == OBX_NOT_FOUND) {
        printf("Task %ld not found\n", (long) id);

    } else if (rc != 0) {
        printf("Error occurred during task %ld selection: %d\n", (long) id, rc);

    } else {
        Task_table_t task = Task_as_root(data);

        if (Task_date_finished(task)) {
            printf("Task %ld has already been done\n", (long) id);
        } else {
            printf("Setting task %ld as done", (long) id);
            rc = task_put(cursor, Task_id(task), Task_text(task), Task_date_created(task), timestamp_now());
        }
    }

    return rc;
}

obx_err task_list(OBX_cursor* cursor, bool list_done) {
    obx_err rc = 0;

    void* data;
    size_t size;

    rc = obx_cursor_first(cursor, &data, &size);

    if (rc == OBX_NOT_FOUND) {
        printf("There are no tasks\n");
        return 0;
    }

    size_t date_buffer_length = 100;
    const char* date_format = "%Y-%m-%d %H:%M:%S";

    char* date_created = (char*) malloc(sizeof(char) * date_buffer_length);
    if (!date_created) return -1;
    memset(date_created, 0, date_buffer_length);

    char* date_finished = (char*) malloc(sizeof(char) * date_buffer_length);
    if (!date_finished) return -1;
    memset(date_finished, 0, date_buffer_length);

    printf("%3s  %-19s  %-19s  %s\n", "ID", "Created", "Finished", "Text");

    do {
        Task_table_t task = Task_as_root(data);

        if (Task_date_finished(task) && !list_done) continue;

        timestamp_parse(Task_date_created(task), date_format, date_buffer_length, &date_created);
        timestamp_parse(Task_date_finished(task), date_format, date_buffer_length, &date_finished);

        printf("%3ld  %-19s  %-19s  %s\n", (long) Task_id(task), date_created, date_finished, Task_text(task));

    } while (0 == (rc = obx_cursor_next(cursor, &data, &size)));

    free(date_created);
    free(date_finished);
    return (rc == OBX_NOT_FOUND) ? 0 : rc;
}


#define ACTION_NEW 1
#define ACTION_DONE 2
#define ACTION_LIST_OPEN 3
#define ACTION_LIST_DONE 4
#define ACTION_HELP 9

obx_err get_action(int argc, char* argv[]) {
    if (argc < 2) {
        return ACTION_LIST_OPEN;
    }

    if (strcmp("--done", argv[1]) == 0) {
        if (argc == 2) {
            // no arguments to "done" => list done items
            return ACTION_LIST_DONE;
        } else {
            return ACTION_DONE;
        }
    }

    if (strcmp("--help", argv[1]) == 0) {
        return ACTION_HELP;
    }

    return ACTION_NEW;
}

void usage(char* programPath) {
    printf("usage: %s \n", basename(programPath));
    const char* format = "    %-30s %s\n";
    printf(format, "text of a new task", "create a new task with the given text");
    printf(format, "", "(default) lists active tasks");
    printf(format, "--done", "lists active and done tasks");
    printf(format, "--done ID", "marks the task with the given ID as done");
    printf(format, "--help", "displays this help");
}

int main(int argc, char* argv[]) {
    obx_err rc = 0;

    // determine requested action
    int action = get_action(argc, argv);

    if (action == ACTION_HELP) {
        usage(argv[0]);
        return rc;
    }

    OBX_store* store = NULL;
    OBX_txn* txn = NULL;
    OBX_cursor* cursor = NULL;

    printf("Using libobjectbox version %s, core version: %s\n", obx_version_string(), obx_version_core_string());

    const obx_schema_id task_entity_id = 1; // "Task" as used in the model

    // Firstly, we need to create a model for our data and the store
    {
        OBX_model* model = model_create(task_entity_id);
        if (!model) goto handle_error;

        store = obx_store_open(model, NULL);
        if (!store) goto handle_error;

        // model is freed by the obx_store_open(), we can't access it anymore
    }

    txn = obx_txn_begin(store);
    if (!txn) goto handle_error;

    // get cursor to the entity data
    cursor = obx_cursor_create(txn, task_entity_id);
    if (!cursor) goto handle_error;

    switch (action) {
        case ACTION_NEW: {
            char* text;
            if (task_text(argc, argv, &text) <= 0) {
                printf("Could not process task text\n");
                rc = -1;
                goto handle_error;
            }

            if ((rc = task_create(cursor, text))) goto handle_error;
            break;
        }

        case ACTION_DONE: {
            obx_id id = (obx_id) atol(argv[2]);
            if (!id) {
                printf("Error parsing ID \"%s\" as a number\n", argv[2]);
                return -1;
            }

            if ((rc = task_done(cursor, id))) goto handle_error;
            break;
        }

        case ACTION_LIST_OPEN:
        case ACTION_LIST_DONE:
            if ((rc = task_list(cursor, action == ACTION_LIST_DONE))) goto handle_error;
            break;

        default:
            printf("Internal error - requested action not handled\n");
            break;
    }

    if ((rc = obx_cursor_close(cursor))) goto handle_error;
    if ((rc = obx_txn_commit(txn))) goto handle_error;
    if ((rc = obx_txn_close(txn))) goto handle_error;

    if ((rc = obx_store_await_async_completion(store))) goto handle_error;
    if ((rc = obx_store_close(store))) goto handle_error;

    return rc;

    // print error and cleanup on error
    handle_error:
        if (!rc) rc = -1;
        error_print();

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
