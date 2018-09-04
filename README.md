ObjectBox C API
===============
ObjectBox is a superfast database for objects.
Using this C API, you can us ObjectBox as an embedded database in your C/C++ application.
In this embedded mode, it runs within your application process.

Some features
-------------
* Object storage based on [FlatBuffers](https://google.github.io/flatbuffers/)
* Zero-copy reads
* Secondary indexes based on object properties
* Simple get/put API
* Asynchronous puts
* Automatic model migration (no schema upgrade scripts etc.) 
* (Coming soon: Powerful queries) 
* (Coming soon: Relations to other objects) 

Foundation for Higher Languages
-------------------------------
The C API also serves as a basis for ObjectBox bindings in higher languages.
For example, we provide a Go binding using the C APIs.
In the same way you could create your own ObjectBox Python API if you wanted.
The C API is bytes based, so you can build and read Flatbuffers table in your language of choice.

Usage and Installation
----------------------
The C API comes as a single header in the [include/objectbox.h](include/objectbox.h) file.
Compile your code against it and use the binary library to link against.
  
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
TODO

Current state
-------------
The C API is not as convenient as the [Java/Kotlin APIs](https://docs.objectbox.io/),
which deeply integrate into the language using e.g. [@Entity annotations](https://docs.objectbox.io/entity-annotations).
Instead, the C API is leaves more tasks to the developer.
For example, you have to [create a FlatBuffers schema](https://google.github.io/flatbuffers/flatbuffers_guide_writing_schema.html) and build a corresponding ObjectBox model separately.
While we might combine the two and provide more convenience in the future, the current version requires some boiler plate code.
