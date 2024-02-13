[TOC]

Introduction
============
[ObjectBox](https://objectbox.io/) is a fast database for data objects.

Installation and examples
------------------------- 
See [Installation docs](https://cpp.objectbox.io/installation) and check the [project's GitHub repository](https://github.com/objectbox/objectbox-c) for additional info and examples including the usage of flatcc.

Headers
-------------
* ObjectBox C99 support is provided by a single header file: **objectbox.h**
* ObjectBox C++11 support is available in an additional header: **objectbox.hpp**

Basic concepts
--------------
* Objects are the entities persisted in the database
* Objects are [FlatBuffers](https://google.github.io/flatbuffers/)
* Objects are "grouped" by their type; e.g. there is a Box (or "Cursor") for each type
* Objects are addressed using a 64 bit integer ID (`obx_id`)
* There is no query language; queries are build using a [query builder](\ref OBX_query_builder)
* Objects are stored on disk by default (ACID), or in-memory ("memory:" directory prefix)

See [docs](https://cpp.objectbox.io) for more information on how to use ObjectBox in C and C++

API Naming conventions
----------------------
* C methods: obx_thing_action()
* C structs: OBX_thing {}
* C error codes: OBX_ERROR_REASON
* C++ namespace: obx::

Essential types
-----------------
Check the docs for the following types:

* [OBX_store](\ref OBX_store) and [obx::Store](\ref obx::Store): the "database"; "opens" data files in a given directory
* [OBX_box](\ref OBX_box) and [obx::Box](\ref obx::Box): object operations like put and get  
* [OBX_query_builder](\ref OBX_query_builder) and [obx::QueryBuilder](\ref obx::QueryBuilder): used to construct queries using "query conditions"  
* [OBX_query](\ref OBX_query) and [obx::Query](\ref obx::Query): the product of a query builder can be executed to find objects matching the previously defined conditions  

Essential Links/Readings
------------------------
* High-level docs and examples: https://cpp.objectbox.io
* Entity (object) IDs: https://docs.objectbox.io/advanced/object-ids
* Meta model and UIDs: https://docs.objectbox.io/advanced/meta-model-ids-and-uids
