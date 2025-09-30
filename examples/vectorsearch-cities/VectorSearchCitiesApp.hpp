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

#pragma once

#include <chrono>
#include <cinttypes>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "city.obx.hpp"
#include "objectbox-model.h"
#include "objectbox.hpp"

/// Demonstrates ObjectBox vector search in an interactive console application.
/// It imports cities from a CSV file, and allows to search for cities by name or location.
/// Users interact with simple commands like `name <city>` or `geo <lat>,<long>`.
/// It's also possible to import our own data via a CSV file.
class VectorSearchCitiesApp {
    // for string delimiter
    enum class Command { Import, Add, SearchByName, SearchByGeoLocation, RemoveAll, Exit, List, Help, Unknown };

    obx::Store& store;
    obx::Box<City> cityBox;

    /// The query to select cities by name (standard text-based search).
    /// As a member, the query is just build once and can be used multiple times.
    obx::Query<City> queryCityByName_;

    /// The query to select cities by location (vector search).
    /// As a member, the query is just build once and can be used multiple times.
    obx::Query<City> queryCityByLocation_;

public:
    explicit VectorSearchCitiesApp(obx::Store& obxStore)
        : store(obxStore),
          cityBox(obxStore),
          queryCityByName_(cityBox.query(City_::name.startsWith("", false)).build()),
          queryCityByLocation_(cityBox.query(City_::location.nearestNeighbors({}, 1)).build()) {}

    /// If no cities are present in the DB, imports data from cities.csv
    void checkImportData() {
        uint64_t count = cityBox.count();
        if (count == 0) {
            bool ok = importData("cities.csv");
            if (!ok) {
                std::cout << "NOTE: The initial import from cities.csv failed.\n"
                             "Maybe try to locate the files and import it manually?\n";
            }
        } else {
            std::cout << "Will not load cities.csv; we already have " << count << " cities" << std::endl;
        }
    }

    /// The main loop: reads commands from the console and delegates them to processCommand().
    int run() {
        std::cout << "Welcome to the ObjectBox VectorSearch Cities app example" << std::endl;
        printHelp();

        std::string input;
        std::vector<std::string> args;
        while (std::getline(std::cin, input)) {  // quit the program with ctrl-d
            if (input.empty()) continue;

            splitInput(input, args);
            try {
                Command command = getCommand(args[0]);
                if (command == Command::Exit) return 0;
                processCommand(command, args);
            } catch (const std::exception& e) {
                std::cerr << "Error executing " << input << std::endl << e.what();
                return 1;
            }
        }

        return 0;
    }

protected:
    /// This is the central function processing the commands.
    /// It demonstrates ObjectBox database API usage, e.g. putting and reading objects.
    void processCommand(Command command, const std::vector<std::string>& args) {
        switch (command) {
            case Command::Import: {
                if (args.size() == 2) {
                    bool success = importData(args[1].c_str());
                    if (!success) {
                        std::cerr << "Error: CVS file not found: " << args[1] << std::endl;
                    }
                } else {
                    std::cerr << "Missing arguments for import " << std::endl;
                    printHelp();
                }
                break;
            }
            case Command::Add: {
                if (args.size() == 4) {
                    City object{};
                    object.name = args[1];
                    object.location = toLocation(args[2], args[3]);
                    cityBox.put(object);
                    std::cout << "Added city: " << object.id << " - " << object.name << std::endl;
                } else {
                    std::cerr << "Missing arguments for new " << std::endl;
                    printHelp();
                }
                break;
            }
            case Command::List: {
                if (args.size() >= 1 && args.size() <= 2) {
                    if (args.size() == 1) {
                        dump(cityBox.getAll());
                    } else {
                        dump(cityBox.query(City_::name.startsWith(args[1])).build().find());
                    }
                } else {
                    std::cerr << "Unknown ls arguments." << std::endl;
                    printHelp();
                    break;
                }
                break;
            }
            case Command::SearchByName: {
                if (args.size() >= 2 && args.size() <= 3) {
                    int64_t numResults = (args.size() == 3) ? atoi(args[2].c_str()) : 5;
                    queryCityByName_.setParameter(City_::name, args[1]);
                    std::unique_ptr<City> result = queryCityByName_.findFirst();
                    // Occasionally, users might import cities.csv multiple time; we just take the first match
                    if (result) {
                        queryCityByLocation_.setParameter(City_::location, result->location);
                        queryCityByLocation_.setParameterMaxNeighbors(City_::location, numResults);
                        std::vector<std::pair<City, double>> citiesAndScores = queryCityByLocation_.findWithScores();
                        dump(citiesAndScores);
                    } else {
                        std::cerr << "Unknown City " << args[1] << std::endl;
                    }
                } else {
                    std::cerr << "city-neighbors: wrong arguments." << std::endl;
                    printHelp();
                    break;
                }
                break;
            }
            case Command::SearchByGeoLocation: {
                if (args.size() >= 3 && args.size() <= 4) {
                    std::vector<float> location = toLocation(args[1], args[2]);
                    int64_t numResults = (args.size() == 4) ? atoi(args[3].c_str()) : 5;
                    queryCityByLocation_.setParameter(City_::location, location);
                    queryCityByLocation_.setParameterMaxNeighbors(City_::location, numResults);
                    std::vector<std::pair<City, double>> citiesAndScores = queryCityByLocation_.findWithScores();
                    dump(citiesAndScores);
                } else {
                    std::cerr << "neighbors: syntax error" << std::endl;
                    printHelp();
                    break;
                }
                break;
            }
            case Command::RemoveAll:
                if (args.size() == 1) {
                    uint64_t removedCount = cityBox.removeAll();
                    std::cout << "removeAll removed " << removedCount << " cities" << std::endl;
                } else {
                    std::cerr << "removeAll does not take any parameters" << std::endl;
                }
                break;
            case Command::Exit:
                assert(false);  // Should be checked by the caller already
                break;
            case Command::Help:
                printHelp();
                break;
            case Command::Unknown:
            default:
                std::cerr << "Unknown command " << args.at(0) << std::endl;
                fflush(stderr);
                printHelp();
                break;
        }
    }

    static void splitInput(const std::string& input, std::vector<std::string>& outArgs, char delim = ' ') {
        outArgs.clear();
        std::string::size_type pos, pos_start = 0;
        while ((pos = input.find(delim, pos_start)) != std::string::npos) {
            std::string arg = input.substr(pos_start, pos - pos_start);
            pos_start = pos + 1;
            outArgs.push_back(arg);
            delim = ',';
        }
        outArgs.push_back(input.substr(pos_start));
    }

    Command getCommand(const std::string& cmd) const {
        if (cmd == "import") return Command::Import;
        if (cmd == "add") return Command::Add;
        if (cmd == "name") return Command::SearchByName;
        if (cmd == "geo") return Command::SearchByGeoLocation;
        if (cmd == "removeAll") return Command::RemoveAll;
        if (cmd == "exit" || cmd == "quit") return Command::Exit;
        if (cmd == "ls" || cmd == "list") return Command::List;
        if (cmd == "help" || cmd == "?") return Command::Help;
        return Command::Unknown;
    }

    void printHelp() const {
        std::cout << "Available commands are:\n"
                  << "    import <filepath>          Import CSV data (try cities.csv)\n"
                  << "    ls [<prefix>]              List cities (with common <prefix> if set)\n"
                  << "    name <city>[,<n>]          Search <n> cities to nearest to the given <city> name/prefix\n"
                  << "                               (<n> defaults to 5; try `name Berlin` or `name berl`)\n"
                  << "    geo <lat>,<long>[,<n>]     Search <n> cities nearest to the given geo location\n"
                  << "                               (<n> defaults to 5; try `geo 50,10`)\n"
                  << "    add <city>,<lat>,<long>    add location\n"
                  << "    removeAll                  remove all existing data\n"
                  << "    exit                       close the program\n"
                  << "    help                       display this help" << std::endl;
    }

    static std::vector<float> toLocation(const std::string& latitude, const std::string& longitude) {
        return {float(std::atof(latitude.c_str())), float(std::atof(longitude.c_str()))};
    }

    static void dump(const City& city) {
        printf("%3" PRIu64 "  %-18s  %-9.2f %-9.2f\n", city.id, city.name.c_str(), city.location[0], city.location[1]);
    }

    static void dump(const std::vector<std::unique_ptr<City>>&& list) {
        printf("%3s  %-18s  %-18s \n", "ID", "Name", "Location");
        for (const auto& city : list) {
            dump(*city);
        }
    }

    static void dump(const std::vector<City>& list) {
        printf("%3s  %-18s  %-18s \n", "ID", "Name", "Location");
        for (const auto& city : list) {
            dump(city);
        }
    }

    static void dump(const std::pair<City, double>& pair) {
        printf("%3" PRIu64 "  %-18s  %-9.2f %-9.2f %5.2f\n", pair.first.id, pair.first.name.c_str(),
               pair.first.location[0], pair.first.location[1], pair.second);
    }

    static void dump(const std::vector<std::pair<City, double>>& list) {
        printf("%3s  %-18s  %-19s %-10s\n", "ID", "Name", "Location", "Score");
        for (const auto& city : list) {
            dump(city);
        }
    }

    bool importData(const char* path) {
        std::ifstream ifs(path);
        if (!ifs.good()) {
            return false;
        }
        obx::Transaction tx = store.tx(obx::TxMode::WRITE);
        size_t count = 0;
        for (;;) {
            std::string line;
            std::vector<std::string> cols;
            std::getline(ifs, line);
            if (!ifs.good()) break;
            splitInput(line, cols, ',');
            City object;
            object.id = 0;
            object.name = cols[0];
            object.location = toLocation(cols[1], cols[2]);
            cityBox.put(object);
            count++;
        }
        tx.success();
        std::cout << "Imported " << count << " entries from " << path << std::endl;
        return true;
    }
};