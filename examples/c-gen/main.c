/*
 * Copyright 2018-2020 ObjectBox Ltd. All rights reserved.
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

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "objectbox-model.h"
#include "objectbox.h"
#include "tasklist.obx.h"

#if !defined(_MSC_VER)
#include <libgen.h>
#endif

#define DATE_FORMAT_STRING "%Y-%m-%d %H:%M:%S"
#define DATE_BUFFER_LENGTH 100

// List of action codes returned by parse_action
#define ACTION_NEW 1
#define ACTION_DONE 2
#define ACTION_LIST_UNFINISHED 3
#define ACTION_LIST_ALL 4
#define ACTION_HELP 9

// Utility functions
int parse_action(int argc, char* argv[]);
int parse_text(int argc, char** argv, char** outText);
uint64_t timestamp_now();
void date_to_str(char* buff, uint64_t timestamp);

// functions to handle each requested action
void do_action_help(char* program_path);
int do_action_new(OBX_box* box, int argc, char* argv[]);
int do_action_done(OBX_box* box, int argc, char* argv[]);
int do_action_list(OBX_box* box, OBX_query* query);

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

    OBX_box* task_box = obx_box(store, Task_ENTITY_ID);

    switch (action) {
        case ACTION_NEW:
            rc = do_action_new(task_box, argc, argv);
            break;
        case ACTION_DONE:
            rc = do_action_done(task_box, argc, argv);
            break;
        case ACTION_LIST_UNFINISHED: {
            OBX_query_builder* qb = obx_query_builder(store, Task_ENTITY_ID);
            if (!qb) {
                rc = obx_last_error_code();
                break;
            }

            obx_qb_equals_int(qb, Task_PROP_ID_date_finished, 0);
            OBX_query* query = obx_query(qb);
            obx_qb_close(qb);
            if (!query) {
                rc = obx_last_error_code();
                break;
            }

            rc = do_action_list(task_box, query);
            obx_query_close(query);
            break;
        }
        case ACTION_LIST_ALL:
            rc = do_action_list(task_box, false);
            break;
        default:
            rc = 42;
            printf("Internal error - requested action not handled\n");
            break;
    }

    if (rc != 0) {
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
    OBX_model* model = create_obx_model();
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
    printf("    %-30s %s\n", "text of a new task", "create a new task with the given text");
    printf("    %-30s %s\n", "", "(default) lists active tasks");
    printf("    %-30s %s\n", "--list", "lists active and done tasks");
    printf("    %-30s %s\n", "--done ID", "marks the task with the given ID as done");
    printf("    %-30s %s\n", "--help", "displays this help");
}

//--------------------------------------------------------------------------------------------------------------------
// do_action_new - create a new task entity and add it to the database
//--------------------------------------------------------------------------------------------------------------------

int do_action_new(OBX_box* box, int argc, char* argv[]) {
    Task task = {0};

    // grab the task text from the command line
    if (parse_text(argc, argv, &task.text) <= 0) {
        return -1;
    }

    task.date_created = timestamp_now();

    obx_id id = Task_put(box, &task);

    if (id != 0) {
        printf("New task: %" PRIu64 " - %s\n", id, task.text);
    } else {
        printf("Failed to create the task\n");
    }

    free(task.text);  // malloc() by parse_text()
    return id == 0 ? obx_last_error_code() : 0;
}

//--------------------------------------------------------------------------------------------------------------------
// do_action_done - mark an extant task entity as done
//--------------------------------------------------------------------------------------------------------------------

int do_action_done(OBX_box* box, int argc, char* argv[]) {
    // grab the id from the command line
    assert(argc == 2);
    obx_id id = (obx_id) atol(argv[2]);
    if (!id) {
        printf("Error parsing ID \"%s\" as a number\n", argv[2]);
        return -1;
    }

    Task* task = Task_get(box, id);

    // First, we read the entity back from the cursor
    if (task == NULL) {
        printf("Task %ld not found\n", (long) id);
        return 1;
    }

    obx_err rc = 0;
    if (task->date_finished != 0) {
        printf("Task %ld has already been done\n", (long) id);
    } else {
        // It's not been marked done. Rebuild the entity, marked as done, and use the cursor to overwrite it
        printf("Setting task %ld as done\n", (long) id);
        task->date_finished = timestamp_now();

        if (!Task_put(box, task)) {
            printf("Failed to mark the task as done\n");
            rc = obx_last_error_code();
        }
    }

    Task_free(task);
    return rc;
}

//--------------------------------------------------------------------------------------------------------------------
// do_action_list - list all the task entities, open or done, depending on list_open
//--------------------------------------------------------------------------------------------------------------------

int do_action_list(OBX_box* box, OBX_query* query) {
    OBX_txn* txn = NULL;
    OBX_bytes_array* list = NULL;
    obx_err rc = 0;

    // Note that this time we are using a read transaction
    txn = obx_txn_read(obx_box_store(box));
    if (!txn) {
        printf("Failed to start a transaction\n");
        rc = obx_last_error_code();
        goto clean_up;
    }

    if (query) {
        list = obx_query_find(query);
    } else {
        list = obx_box_get_all(box);
    }

    if (!list) {
        printf("Failed to list the tasks\n");
        rc = obx_last_error_code();
        goto clean_up;
    }

    // A nice header for the table
    printf("%3s  %-19s  %-19s  %s\n", "ID", "Created", "Finished", "Text");

    char date_created[DATE_BUFFER_LENGTH];
    char date_finished[DATE_BUFFER_LENGTH];

    size_t i;
    for (i = 0; i < list->count; i++) {
        Task* task = Task_new_from_flatbuffer(list->bytes[i].data, list->bytes[i].size);

        date_to_str(date_created, task->date_created);
        date_to_str(date_finished, task->date_finished);

        printf("%3" PRIu64 "  %-19s  %-19s  %s\n", task->id, date_created, date_finished, task->text);
        Task_free(task);
    }

clean_up:
    if (list) obx_bytes_array_free(list);
    if (txn) obx_txn_close(txn);
    return rc;
}

//--------------------------------------------------------------------------------------------------------------------
// Utility functions
//--------------------------------------------------------------------------------------------------------------------

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

    *outText = (char*) malloc(sizeof(char) * (unsigned long)(size + 1));
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
        return ACTION_LIST_UNFINISHED;
    }

    if (strcmp("--done", argv[1]) == 0) {
        if (argc != 3) {
            return ACTION_HELP;
        } else {
            return ACTION_DONE;
        }
    }

    if (strcmp("--list", argv[1]) == 0) {
        return ACTION_LIST_ALL;
    }

    if (strcmp("--help", argv[1]) == 0 || strcmp("--usage", argv[1]) == 0) {
        return ACTION_HELP;
    }

    return ACTION_NEW;
}
