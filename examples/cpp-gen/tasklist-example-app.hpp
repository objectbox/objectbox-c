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

#pragma once

#include <chrono>
#include <cinttypes>
#include <iostream>

#include "objectbox-model.h"
#include "objectbox.hpp"
#include "tasklist.obx.hpp"

int processArgs(int argc, char* argv[], obx::Options& outOptions) {
    // Remember, argv[0] is application path

    const char* directory = nullptr;
    if (argc == 2) {
        directory = argv[1];
    } else if (argc == 3) {
        std::string paramName = argv[1];
        if (paramName == "-d" || paramName == "--directory") {
            directory = argv[2];
        } else {
            std::cerr << "Unknown argument " << paramName << ". Expected -d or --directory." << std::endl;
            return 1;
        }
    } else if (argc > 3) {
        std::cerr << "This app only takes zero, one or two arguments" << std::endl;
        return 1;
    }

    if (directory) {
        outOptions.directory(directory);
        std::cout << "Using DB directory " << directory << std::endl;
    }

    return 0;
}

class TasklistCmdlineApp {
    obx::Store& store;
    obx::Box<Task> taskBox;

    /// caching the query object is optimal so it's not constructed on each invocation
    obx::Query<Task> unfinishedTasksQuery;

public:
    TasklistCmdlineApp(obx::Store& obxStore)
        : store(obxStore),
          taskBox(obxStore),
          unfinishedTasksQuery(taskBox.query(Task_::date_finished.equals(0)).build()) {}

    int run() {
        std::cout << "Welcome to the ObjectBox tasks-list app example" << std::endl;
        printHelp();

        std::string input;
        std::string cmd;
        std::string arg;
        while (std::getline(std::cin, input)) {  // quit the program with ctrl-d
            if (input.empty()) continue;

            splitInput(input, cmd, arg);
            try {
                switch (getCommand(cmd)) {
                    case Command::New: {
                        Task object{};
                        object.text = arg;
                        object.date_created = millisSinceEpoch();
                        taskBox.put(object);
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
                            std::cout << "Task ID " << id << " marked as done at " << task->date_finished << std::endl;
                            taskBox.put(*task);
                        }
                        break;
                    }
                    case Command::List: {
                        std::vector<std::unique_ptr<Task>> list;

                        if (arg == "-a") {
                            list = taskBox.getAll();
                        } else if (arg.empty()) {
                            list = unfinishedTasksQuery.find();
                        } else {
                            std::cerr << "Unknown ls argument " << arg << std::endl;
                            printHelp();
                            break;
                        }

                        printf("%3s  %-14s  %-14s  %s\n", "ID", "Created", "Finished", "Text");
                        for (const auto& task : list) {
                            printf("%3" PRIu64 "  %-14s  %-14s  %s\n", task->id, fmtTime(task->date_created).c_str(),
                                   fmtTime(task->date_finished).c_str(), task->text.c_str());
                        }
                        break;
                    }
                    case Command::Exit:
                        return 0;
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

            } catch (const std::exception& e) {
                std::cerr << "Error executing " << input << std::endl << e.what();
                return 1;
            }
        }

        return 0;
    }

protected:
    void splitInput(const std::string& input, std::string& outCmd, std::string& outArg) {
        std::string::size_type pos = input.find(" ");
        if (pos == std::string::npos) {
            outCmd = input;
            outArg.clear();
        } else {
            outCmd = input.substr(0, pos);
            outArg = input.substr(pos + 1);
        }
    }

    enum class Command { New, Done, Exit, List, Help, Unknown };

    Command getCommand(const std::string& cmd) {
        if (cmd == "new") return Command::New;
        if (cmd == "done") return Command::Done;
        if (cmd == "exit") return Command::Exit;
        if (cmd == "ls") return Command::List;
        if (cmd == "help") return Command::Help;
        return Command::Unknown;
    }

    void printHelp() {
        std::cout << "Available commands are: " << std::endl
                  << "    ls [-a]        list tasks - unfinished or all (-a flag)" << std::endl
                  << "    new Task text  create a new task with the text 'Task text'" << std::endl
                  << "    done ID        mark task with the given ID as done" << std::endl
                  << "    exit           close the program" << std::endl
                  << "    help           display this help" << std::endl;
    }

    uint64_t millisSinceEpoch() {
        auto time = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
    }

    /// Formats the given UNIX timestamp as a human-readable time
    std::string fmtTime(uint64_t timestamp) {
        // NOTE: implement your fancy time formatting here...
        return std::to_string(timestamp);
    }
};