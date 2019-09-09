ObjectBox C API
===============
ObjectBox is a superfast database for objects.
Using this C API, you can us ObjectBox as an embedded database in your C/C++ application.
In this embedded mode, it runs within your application process.

**Latest version: 0.7.0** (2019-09-09). Please note that this release comes with breaking [changes](CHANGELOG.md), see also below. 

Some features
-------------
* ACID compliant object storage based on [FlatBuffers](https://google.github.io/flatbuffers/)
* Lightweight for smart devices with less than 1 MB binary size
  (special feature reduced versions with 1/3 - 1/2 size are available on request)
* Zero-copy reads
* Secondary indexes based on object properties
* Simple get/put API
* Async API for asynchronous puts, inserts, updates, removes
* Automatic model migration (no schema upgrade scripts etc.) 
* Powerful queries
* Relations to other objects (1:N and M:N)

Foundation for Higher Languages
-------------------------------
The C API also serves as a basis for ObjectBox bindings in higher languages.
For example, the official APIs for [Go](https://github.com/objectbox/objectbox-go), [Swift](https://github.com/objectbox/objectbox-swift), [Dart/Flutter](https://github.com/objectbox/objectbox-dart) and [Python](https://github.com/objectbox/objectbox-python) rely on the C API.
In the same way, you could create a ObjectBox API for another programming language, e.g. for JavaScript.
For the C API, data consists of bytes representing FlatBuffers tables, which you can build and read in your language of choice.

Usage and Installation
----------------------
The C API comes as a single header in the [include/objectbox.h](include/objectbox.h) file.
Compile your code against it and use the binary library (.so, .dylib, .dll depending on the platform) to link against.
  
There are a couple of ways to get the library:

* Using the download.sh script (on Windows, use something like Git Bash to run it)
    * Either clone the repo and run `./download.sh`
    * ... or just download [download.sh](download.sh), run `chmod +x download.sh` and `./download.sh`
* Conan (wip, details coming later): https://bintray.com/objectbox/conan/objectbox-c%3Aobjectbox

Details on the download.sh script:

* It creates a "download" directory and a version dependent sub directory named like "libobjectbox-0.1-some-hex-hash"
* Inside the version dependent sub directory, you will find the directories "include" and "lib"
* The "lib" directory contains the binary library
* On systems supporting 'sudo', the download.sh script also asks you to install the library in /usr/local/lib.

Examples & API Documentation
----------------------------
Documentation is still on-going work.
To get started, please have a look at the [tasks example](examples/tasks).

Current state / Changelog
-------------------------
**Beta notice:** the C API become stable starting from version 1.0.
Until then, API improvements may result in breaking changes, for example functions may be renamed.

**[Changelog](CHANGELOG.md):** If you update from a previous version, please check the [changelog](CHANGELOG.md).
Besides new features, there may be breaking changes, which might require changes to your code. 

The C API is a thin wrapper around a robust DB core, which is version 2.x and already used on million of devices.

**Object convenience:** The C API is currently not as convenient as the APIs for [Go](https://golang.objectbox.io/), [Swift](https://swift.objectbox.io/) or [Java/Kotlin](https://docs.objectbox.io/),
which deeply integrate into the language using e.g. [@Entity annotations](https://docs.objectbox.io/entity-annotations).
Instead, the C API leaves more tasks to the developer.
For example, you have to [create a FlatBuffers schema](https://google.github.io/flatbuffers/flatbuffers_guide_writing_schema.html) and build a corresponding ObjectBox model separately.
While we might combine the two and provide more convenience in the future, the current version requires some boiler plate code.

Changelog
---------
[CHANGELOG.md](CHANGELOG.md)

License
-------
    Copyright 2018-2019 ObjectBox Ltd. All rights reserved.
    
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
    
        http://www.apache.org/licenses/LICENSE-2.0
    
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

