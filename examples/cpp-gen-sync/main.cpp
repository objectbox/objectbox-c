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

#include "tasklist-example-app.hpp"


using namespace obx;

int main(int argc, char* argv[]) {
    // this example expect sync-server to be running locally
    std::string syncServerURL = "ws://127.0.0.1:9999";

    std::cout << "This is a simple example of a ObjectBox Sync client application." << std::endl
              << "To execute this example yourself, you need to start a sync-server locally:" << std::endl
              << "    ./sync-server --model objectbox-model.json -d server-db --unsecured-no-authentication"
              << " --bind " + syncServerURL << std::endl
              << "Note: update the --model argument path to the model file from this example directory." << std::endl
              << "You can launch multiple instances of this program in parallel in two windows, each with" << std::endl
              << "a separate database by starting each with a different `--directory dirname` argument." << std::endl
              << "The clients automatically connect to the sync-server (URL specified in main-sync.cpp)." << std::endl
              << "See sync in action: create tasks on one client and refresh the list on the other." << std::endl;
    std::cout << "---------------------------------------------------------------------------------------" << std::endl;

    // create_obx_model() provided by objectbox-model.h
    // obx interface contents provided by objectbox.hpp
    Store::Options storeOptions(create_obx_model());

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