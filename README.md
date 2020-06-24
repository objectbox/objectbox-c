ObjectBox C and C++ APIs
========================
[ObjectBox](https://objectbox.io) is a superfast database for objects.
This is the **ObjectBox runtime library** to run ObjectBox as an embedded database in your C or C++ application.

**Latest version: 0.9.1** (2020-06-23). See [changelog](CHANGELOG.md) for more details. 

In most cases you want to use the C and C++ APIs in combination with the **companion project [ObjectBox Generator](https://github.com/objectbox/objectbox-generator)**.
Like this, you get a convenient API which requires minimal code on your side to work with the database.

Here's a C++ code example that inserts a `Task` data object into the database: 

    obx::Box<Task> box(store);
    box.put({.text = "Buy milk"}); 
    
Note: `Task` is a `struct` representing a user defined data model - see [ObjectBox Generator](https://github.com/objectbox/objectbox-generator) for details.  

Some features
-------------
* ACID compliant object storage ("object" as in class/struct instances)
* Direct support for [FlatBuffers](https://google.github.io/flatbuffers/) data objects (aka "flatbuffers table") 
* Lightweight for smart devices; its binary size is only around 1 MB 
  (special feature reduced versions with 1/3 - 1/2 size are available on request)
* Zero-copy reads for highest possible performance; access tens of millions of objects on commodity hardware
* Secondary indexes based on object properties
* Async API for asynchronous puts, inserts, updates, removes
* Automatic model migration (no schema upgrade scripts etc.) 
* Powerful queries
* Relations to other objects (1:N and M:N)
* Optimized Time series types (TS edition only)
* Data synchronization across the network (sync edition only)

Usage and Installation
----------------------
The APIs come as single header file for C and C++:
 
  * C: [include/objectbox.h](include/objectbox.h)
  * C++: [include/objectbox-cpp.h](include/objectbox-cpp.h) (depends on objectbox.h)
  
Compile your code against it and use the binary library (.so, .dylib, .dll depending on the platform) to link against.
  
There are a couple of ways to get the library:

* Using the download.sh script (on Windows, use something like Git Bash to run it)
    * Either clone the repo and run `./download.sh`
    * ... or download [download.sh](download.sh) and run it in a terminal:<br> 
      `wget https://raw.githubusercontent.com/objectbox/objectbox-c/master/download.sh`<br>
      `chmod +x download.sh`<br>
      `./download.sh`
* Conan (wip, details coming later): https://bintray.com/objectbox/conan/objectbox-c%3Aobjectbox

Details on the download.sh script:

* It creates a "download" directory and a version dependent sub directory named like "libobjectbox-0.1-some-hex-hash"
* Inside the version dependent sub directory, you will find the directories "include" and "lib"
* The "lib" directory contains the binary library
* On systems supporting 'sudo', the download.sh script also asks you to install the library in /usr/local/lib.

C++ API
-------
The C++ API is built on top of the C API exposed by the library (e.g. you still need objectbox.h).
You can also use both APIs from your code if need be.
For example, you use the C++ `obx::Box` class for most database operations, but "break out" into the C API for a special function you need.  
Note that to use the `obx::Box` class, you also need the [ObjectBox Generator](https://github.com/objectbox/objectbox-generator) to generate binding code.
In short, it generates "boiler plate" source code and maintains some metadata around the data model.

Examples & API Documentation
----------------------------
Documentation is still on-going work.
For now, please refer to the [ObjectBox Generator](https://github.com/objectbox/objectbox-generator) for more details.

For an API reference check one of those:

* [include/objectbox.h](include/objectbox.h): single header file 
* [API reference docs](https://objectbox.io/docfiles/c/current/): online HTML docs (Doxygen) 

Current state / Changelog
-------------------------
The C API is a thin wrapper around a robust DB core, which is version 2.x and already used on million of devices.

**Beta notice:** the C API will become stable starting from version 1.0.
Until then, API improvements may result in breaking changes. For example, functions may still be renamed.

**[Changelog](CHANGELOG.md):** If you update from a previous version, please check the [changelog](CHANGELOG.md).
Besides new features, there may be breaking changes requiring modifications to your code. 

C API as the Foundation for Higher Languages
--------------------------------------------
The plain C API (without the Generator) also serves as a basis for ObjectBox bindings in higher languages.
For example, the official APIs for [Go](https://github.com/objectbox/objectbox-go), [Swift](https://github.com/objectbox/objectbox-swift), [Dart/Flutter](https://github.com/objectbox/objectbox-dart) and [Python](https://github.com/objectbox/objectbox-python) rely on the C API.
In the same way, you could create a ObjectBox API for another programming language, e.g. for JavaScript.
For the C API, data consists of bytes representing FlatBuffers tables, which you can build and read in your language of choice.

License
-------
    Copyright 2018-2020 ObjectBox Ltd. All rights reserved.
    
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
    
        http://www.apache.org/licenses/LICENSE-2.0
    
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

