/*
 * Copyright 2018-2024 ObjectBox Ltd. All rights reserved.
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

#include "VectorSearchCitiesApp.hpp"
#include "objectbox.hpp"

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
    std::cout << "** ObjectBox database (https://objectbox.io/) C++ vector search example (cities). **\n"
                 "C++ docs: https://cpp.objectbox.io/ | https://docs.objectbox.io/on-device-vector-search\n"
          << std::endl;

    if (!obx_has_feature(OBXFeature_VectorSearch)) {
        std::cerr << "Vector search is not supported in this edition.\n"
                     "Please ensure to get ObjectBox with vector search enabled."
                  << std::endl;
        return 1;
    }

    // Hint: create_obx_model() is provided by objectbox-model.h, which is a (pre)generated source file
    Options options;
    options.model(create_obx_model());

    if (int err = processArgs(argc, argv, options)) {
        return err;
    }

    Store store(options);
    VectorSearchCitiesApp app(store);
    app.checkImportData();
    return app.run();
}
