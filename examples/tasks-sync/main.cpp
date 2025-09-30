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

#define OBX_CPP_FILE // this "materializes" implementations from objectbox.hpp

#include "TasklistCmdlineApp.hpp"

using namespace obx;

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

int main(int argc, char* argv[]) {
    // this example expect sync-server to be running locally
    std::string syncServerURL = "ws://127.0.0.1:9999";

    std::cout << "** ObjectBox database (https://objectbox.io/) Sync client example (tasks). **\n"
                 "Get a free Sync Server trial at https://sync.objectbox.io/.\n"
                 "You can launch multiple instances of this program in parallel in separate windows,\n"
                 "each with a separate database by starting with a different `--directory dirname` argument.\n"
                 "The clients automatically connect to the sync-server (URL specified in main.cpp).\n"
                 "See sync in action: create tasks on one client and refresh the list on the other.\n"
                 "Sync docs: https://sync.objectbox.io/ | C++ docs: https://cpp.objectbox.io/\n"
                 "---------------------------------------------------------------------------------------"
              << std::endl;

    // create_obx_model() provided by objectbox-model.h
    // obx interface contents provided by objectbox.hpp
    Options storeOptions(create_obx_model());

    if (int err = processArgs(argc, argv, storeOptions)) {
        return err;
    }

    Store store(storeOptions);

    // Note: server is expected to be set up with no authentication for this demo
    std::shared_ptr<SyncClient> client = Sync::client(store, syncServerURL, SyncCredentials::none());
    client->start();

    auto app = std::make_shared<TasklistCmdlineApp>(store);
    client->setChangeListener(app);
    return app->run();
}