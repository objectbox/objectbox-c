ObjectBox C and C++ examples
============================

These are examples for ObjectBox C and C++.

Each example is self-contained in its own subdirectory and can be built using CMake.
For convenience, a `build.sh` script is provided in each example directory,
which puts all build files into a separate directory ("build/").
See the [build and run](#build-and-run) section below for details.

## Tasks Examples

There are four examples console applications that implement a simple task list.
Tasks can be added, viewed, and marked as finished to demonstrate typical ObjectBox database operations.

The examples for C++ are:

- `tasks`: C++ application
- `tasks-sync`: C++ application with sync enabled (requires a ObjectBox Sync server)

The examples for C are:

- `c-tasks`: C application
- `c-tasks-lowlevel`: C application, but with lesser used lower-level APIs; not the best to start with

## Vector Search Example

There's a C++ example for vector search in the [vectorsearch-cities](vectorsearch-cities/README.md) directory.

## Build and run

Prerequisites are CMake 3.14+ and a C/C++ compiler.
All examples follow the same setup, and thus we document it only once here.

### Build with the provided shell script

This is the simplest way on Linux and macOS from the command line.

* Typically, you `cd` into an example directory and run `./build.sh` (each example has its own `build.sh`).
* Once the build is done, you can run the example: the executable is in the `build/` directory and its path is printed to the console during the build.
* Run `./build.sh run` to build and run the example in one step.
* The `./build.sh` also accepts `--clear` as the first argument to clear the build directory before building.

### Build within IDEs (CMake)

If you work with a IDE, you can typically just open each example as a CMake project.
The IDE will setup everything for you.

### Build with CMake

If you prefer to use CMake directly (e.g. on a Windows terminal), you can do so as follows:

```
cmake -S . -B build
cmake --build build
```

And then run the built executable:

```
build/objectbox-examples-... # replace ... with the example name
```

## Next steps

If you want, you can copy an example as a starting point for your own project.
Pick the directory of the example that fits best for your needs.

Links and documentation:

* [ObjectBox homepage](https://objectbox.io/)
* [ObjectBox C and C++ docs](https://cpp.objectbox.io/)
* [ObjectBox Sync docs](https://sync.objectbox.io/)
* [ObjectBox vector search docs](https://docs.objectbox.io/on-device-vector-search)

