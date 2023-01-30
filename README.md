ObjectBox Embedded Database for C and C++
=========================================
[ObjectBox](https://objectbox.io) is a superfast C and C++ database for embedded devices (mobile and IoT), desktop and server apps.
The out-of-the-box [Data Sync](https://objectbox.io/sync/) keeps data in sync across devices and any kind of backend/cloud reliably for occasionally connected devices.
ObjectBox Data Persistence and Data Sync follows an offline-first approach and can be used on-premise as well as with a cloud setup.

*********************************************************************************************************************************
Jobs: We're looking for a [C++ Developer](https://objectbox.io/jobs/objectbox-senior-c-plusplus-developer/) with a ❤️ for performant code
*********************************************************************************************************************************

This is the **ObjectBox runtime library** to run ObjectBox as an embedded database in your C or C++ application.

Here's a C++ example that inserts a `Task` data object (a plain user defined `struct`) into the database:
```c++
obx::Box<Task> box(store);
box.put({.text = "Buy milk"}); 
```

See [ObjectBox C and C++ docs](https://cpp.objectbox.io/) for API details.

**Latest version: 0.18.1** (2023-01-30).
See [changelog](CHANGELOG.md) for more details.

## Table of Contents:
- [Feature Highlights](#feature-highlights)
- [Usage and Installation](#usage-and-installation)
- [C++ API](#c-api)
- [Examples](#examples)
- [Documentation](#documentation)
- [Current state / Changelog](#current-state--changelog)
   - [Supported platforms](#supported-platforms)
- [C API as the Foundation for Higher Languages](#c-api-as-the-foundation-for-higher-languages)
- [Other languages/bindings](#other-languagesbindings)
- [How can I help ObjectBox?](#how-can-i-help-objectbox)
- [License](#license)

Feature Highlights
------------------
🏁 **High performance** on restricted devices, like IoT gateways, micro controllers, ECUs etc.\
🪂 **Resourceful** with minimal CPU, power and Memory usage for maximum flexibility and sustainability\
🔗 **Relations:** object links / relationships are built-in\
💻 **Multiplatform:** Linux, Windows, Android, iOS, macOS

🌱 **Scalable:** handling millions of objects resource-efficiently with ease\
💐 **Queries:** filter data as needed, even across relations\
🦮 **Statically typed:** compile time checks & optimizations\
📃 **Automatic schema migrations:** no update scripts needed

**And much more than just data persistence**\
👥 **[ObjectBox Sync](https://objectbox.io/sync/):** keeps data in sync between devices and servers\
🕒 **[ObjectBox TS](https://objectbox.io/time-series-database/):** time series extension for time based data

Some more technical details:

* Zero-copy reads for highest possible performance; access tens of millions of objects on commodity hardware
* Lightweight for smart devices; its binary size is only around 1 MB
* Direct support for [FlatBuffers](https://google.github.io/flatbuffers/) data objects (aka "flatbuffers table")
* Flex type to represent any FlexBuffers
* Secondary indexes based on object properties
* Async API for asynchronous puts, inserts, updates, removes
* Optimized for time series data (TS edition only)
* Data synchronization across the network (sync edition only)

Usage and Installation
----------------------
In most cases you want to use the C and C++ APIs in combination with the **[ObjectBox Generator](https://github.com/objectbox/objectbox-generator) tool**.
This way, you get a convenient C or C++ API which requires minimal code on your side to work with the database.

The APIs come as single header file for C and C++:

* C: [include/objectbox.h](include/objectbox.h)
* C++: [include/objectbox.hpp](include/objectbox.hpp) (depends on objectbox.h)

Compile your code against it and use the binary library (.so, .dylib, .dll depending on the platform) to link against.
Head over to [ObjectBox C and C++ installation docs](https://cpp.objectbox.io/installation) for step-by-step instructions.

C++ API
-------
The C++ API is built on top of the C API exposed by the library (e.g. you still need objectbox.h).
You can also use both APIs from your code if necessary.
For example, you use the C++ `obx::Box` class for most database operations, but "break out" into the C API for a special function you need.  
Note that to use the `obx::Box` class, you also need the [ObjectBox Generator](https://github.com/objectbox/objectbox-generator) to generate binding code.
Find more details how to use it the [Getting started](https://cpp.objectbox.io/getting-started) section of the docs.

Examples
--------
Have a look at the following TaskList example apps, depending on your programming language and preference:

* [C, cursor, no generated code](examples/c-cursor-no-gen) - plain C; using flatcc directly; without any generated code
* [C, with generated code](examples/c-gen) - plain C, using code generated by `objectbox-generator`
* [C++, with generated code](examples/cpp-gen) - C++, using code generated by `objectbox-generator`
  * also includes sync client application example

Documentation
-------------
* [C and C++ docs](https://cpp.objectbox.io/) - official ObjectBox C and C++ documentation
* [include/objectbox.h](include/objectbox.h) - C API header file contains docs as code comments
* [include/objectbox.hpp](include/objectbox.hpp) - C++ API header file contains docs as code comments
* [C and C++ API reference docs](https://objectbox.io/docfiles/c/current/) - online HTML docs (Doxygen)

Current state / Changelog
-------------------------
The C API is a thin wrapper around a robust DB core, which is version 3.x and already used on millions of devices.

**Beta notice:** the C API will become stable starting from version 1.0.
Until then, API improvements may result in breaking changes. For example, functions may still be renamed.

**[Changelog](CHANGELOG.md):** If you update from a previous version, please check the [changelog](CHANGELOG.md).
Besides new features, there may be breaking changes requiring modifications to your code.

### Supported platforms:
* Linux 64-bit
* Linux ARMv6hf (e.g. Raspberry PI Zero)
* Linux ARMv7hf (e.g. Raspberry PI 3/4)
* Linux ARMv8/AArch64 (e.g. Raspberry PI 3/4 with a 64 bit OS like Ubuntu)
* MacOS 64-bit
* Windows 32-bit
* Windows 64-bit

C API as the Foundation for Higher Languages
--------------------------------------------
The plain C API (without the Generator) also serves as a basis for ObjectBox bindings in higher languages.
For example, the official APIs for [Go](https://github.com/objectbox/objectbox-go), [Swift](https://github.com/objectbox/objectbox-swift), [Dart/Flutter](https://github.com/objectbox/objectbox-dart) and [Python](https://github.com/objectbox/objectbox-python) rely on the C API.
In the same way, you could create an ObjectBox API for another programming language, e.g. for JavaScript.
For the C API, data consists of bytes representing FlatBuffers tables, which you can build and read in your language of choice.

Other languages/bindings
------------------------
ObjectBox supports multiple platforms and languages.
Besides C/C++, ObjectBox also offers:

* [ObjectBox Java / Kotlin](https://github.com/objectbox/objectbox-java): runs on Android, desktop, and servers.
* [ObjectBox Swift](https://github.com/objectbox/objectbox-swift): build fast mobile apps for iOS (and macOS)
* [ObjectBox Dart/Flutter](https://github.com/objectbox/objectbox-dart): cross-platform for mobile and desktop apps
* [ObjectBox Go](https://github.com/objectbox/objectbox-go): great for data-driven tools and embedded server applications


How can I help ObjectBox?
---------------------------
Let us know what you love, what you don’t, what do you want to see next?

**We're looking forward to receiving your comments and requests:**

- Add [GitHub issues](https://github.com/ObjectBox/objectbox-java/issues)
- Upvote issues you find important by hitting the 👍/+1 reaction button
- Drop us a line via [@ObjectBox_io](https://twitter.com/ObjectBox_io/)
- ⭐ us, if you like what you see

Thank you! 🙏

Keep in touch: For general news on ObjectBox, [check our blog](https://objectbox.io/blog)!

License
-------
    Copyright 2018-2022 ObjectBox Ltd. All rights reserved.
    
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
    
        http://www.apache.org/licenses/LICENSE-2.0
    
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
