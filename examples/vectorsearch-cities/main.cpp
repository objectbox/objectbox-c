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

#define OBX_CPP_FILE  // Signals objectbox.hpp to add function definitions

#include "VectorSearchCitiesApp.hpp"
#include "objectbox.hpp"

using namespace obx;

int main(int argc, char* argv[]) {
    if (!obx_has_feature(OBXFeature_VectorSearch)) {
        std::cerr << "Vector search is not supported in this edition.\n"
                     "Please ensure to get ObjectBox with vector search enabled."
                  << std::endl;
        return 1;
    }

    // Hint: create_obx_model() is provided by objectbox-model.h, which is a (pre)generated source file
    Options options(create_obx_model());

    if (int err = processArgs(argc, argv, options)) {
        return err;
    }

    Store store(options);
    VectorSearchCitiesApp app(store);
    app.checkImportData();
    return app.run();
}
