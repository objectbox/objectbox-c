/*
 * Copyright 2018-2025 ObjectBox Ltd. All rights reserved.
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

#pragma once

#include <chrono>
#include <cinttypes>
#include <ctime>
#include <iostream>

#include "objectbox-model.h"
#include "objectbox-sync.hpp"
#include "objectbox.hpp"
#include "tasklist.obx.hpp"

/// The ObjectBox tasks-list app example enabled for sync.
/// It uses a simple command-line interface with commands like `new`, `done`, `ls`, `exit`, `help`.
/// The main interaction is done in the processCommand() method.
class TasklistCmdlineApp : public obx::SyncChangeListener {
    /// String commands are parsed into these enum values.
    enum class Command { New, Done, Exit, List, Help, Unknown };

    obx::Store& store;
    obx::Box<Task> taskBox;

    /// The query to select only tasks that are not finished yet.
    /// As a member, the query is just build once and can be used multiple times.
    obx::Query<Task> unfinishedTasksQuery;

public:
    explicit TasklistCmdlineApp(obx::Store& obxStore)
        : store(obxStore),
          taskBox(obxStore),
          unfinishedTasksQuery(taskBox.query(Task_::date_finished.equals(0)).build()) {}

    /// This is the central function processing the commands.
    /// It demonstrates ObjectBox database API usage, e.g. putting and reading objects.
    Command processCommand(const std::string& cmd, const std::string& arg) {
        Command command = getCommand(cmd);
        switch (command) {
            case Command::New: {
                Task object{};  // Initialized with default values, e.g. 0 for ID and date fields.
                object.text = arg;
                object.date_created = millisSinceEpoch();
                taskBox.put(object);
                // Note: after put(), the object has an ID assigned, so we can print it.
                std::cout << "New task: " << object.id << " - " << object.text << std::endl;
                break;
            }
            case Command::Done: {
                obx_id id = std::stoull(arg);
                std::unique_ptr<Task> task = taskBox.get(id);
                if (!task) {
                    std::cerr << "Task ID " << arg << " not found" << std::endl;
                } else if (task->date_finished != 0) {
                    std::cerr << "Task ID " << id << " is already done" << std::endl;
                } else {
                    task->date_finished = millisSinceEpoch();
                    std::cout << "Task ID " << id << " marked as done at " << fmtTime(task->date_finished) << std::endl;
                    taskBox.put(*task);
                }
                break;
            }
            case Command::List: {
                std::vector<std::unique_ptr<Task>> tasks;
                if (arg == "-a") {
                    tasks = taskBox.getAll();
                } else if (arg.empty()) {
                    tasks = unfinishedTasksQuery.findUniquePtrs();
                } else {
                    std::cerr << "Unknown ls argument " << arg << std::endl;
                    printHelp();
                    break;
                }
                listTasks(tasks);
                break;
            }
            case Command::Exit:
                break;  // Handled by caller (we return the parsed command)
            case Command::Help:
                printHelp();
                break;
            case Command::Unknown:
            default:
                std::cerr << "Unknown command " << cmd << std::endl;
                fflush(stderr);
                printHelp();
                break;
        }
        return command;
    }

    /// The main loop: reads commands from the console and delegates them to processCommand().
    int run() {
        printHelp();

        std::string input;
        std::string cmd;
        std::string arg;
        while (std::getline(std::cin, input)) {  // quit the program with ctrl-d
            if (input.empty()) continue;

            splitInput(input, cmd, arg);
            try {
                Command command = processCommand(cmd, arg);
                if (command == Command::Exit) break;
            } catch (const std::exception& e) {
                std::cerr << "Error executing " << input << std::endl << e.what();
                return 1;
            }
        }

        return 0;
    }

    /// This is a callback from ObjectBox Sync when the task list has changed.
    void changed(const std::vector<obx::SyncChange>& changes) noexcept override {
        std::cout << "Task list has changed (synced):" << std::endl;
        std::vector<std::unique_ptr<Task>> list = taskBox.getAll();
        listTasks(list);
    }

protected:
    void splitInput(const std::string& input, std::string& outCmd, std::string& outArg) const {
        std::string::size_type pos = input.find(" ");
        if (pos == std::string::npos) {
            outCmd = input;
            outArg.clear();
        } else {
            outCmd = input.substr(0, pos);
            outArg = input.substr(pos + 1);
        }
    }

    Command getCommand(const std::string& cmd) const {
        if (cmd == "new") return Command::New;
        if (cmd == "done") return Command::Done;
        if (cmd == "exit") return Command::Exit;
        if (cmd == "ls") return Command::List;
        if (cmd == "help") return Command::Help;
        return Command::Unknown;
    }

    void printHelp() const {
        std::cout << "Available commands are: " << std::endl
                  << "    ls [-a]        list tasks - unfinished or all (-a flag)" << std::endl
                  << "    new Task text  create a new task with the text 'Task text'" << std::endl
                  << "    done ID        mark task with the given ID as done" << std::endl
                  << "    exit           close the program" << std::endl
                  << "    help           display this help" << std::endl;
    }

    uint64_t millisSinceEpoch() const {
        auto time = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
    }

    /// Formats the given UNIX timestamp as a human-readable time
    std::string fmtTime(uint64_t timestamp) const {
        if (timestamp == 0) return "";

        auto time_point = std::chrono::system_clock::from_time_t(timestamp / 1000);
        auto time_t = std::chrono::system_clock::to_time_t(time_point);

        char buffer[20];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
        return std::string(buffer);
    }

    void listTasks(std::vector<std::unique_ptr<Task>>& list) const {
        printf("%4s  %-20s  %-20s  %s\n", "ID", "Created", "Finished", "Text");
        for (const auto& task : list) {
            printf("%4" PRIu64 "  %-20s  %-20s  %s\n", task->id, fmtTime(task->date_created).c_str(),
                   fmtTime(task->date_finished).c_str(), task->text.c_str());
        }
    }
};