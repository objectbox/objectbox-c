[TOC]

Introduction
============
[ObjectBox](https://objectbox.io/) is a fast database for data objects.

Installation and examples
------------------------- 
Check the [ObjectBox C repository](https://github.com/objectbox/objectbox-c) for additional info and examples including the usage of flatcc.

Single header
-------------
The entire ObjectBox C API is defined in a single header file:

**objectbox.h**

Basic concepts
--------------
* Objects are the entities persisted in the database
* Objects are [FlatBuffers](https://google.github.io/flatbuffers/)
* Objects are "grouped" by their type; e.g. there is a Box (or "Cursor") for each type
* Objects are addressed using a 64 bit integer ID (`obx_id`)
* There is no query language; queries are build using a [query builder](\ref OBX_query_builder)

API Naming conventions
----------------------
* methods: obx_thing_action()
* structs: OBX_thing {}
* error codes: OBX_ERROR_REASON

Essential types
-----------------
Check the docs for the following types:

* [OBX_store](\ref OBX_store): the "database"; "opens" data files in a given directory
* [OBX_box](\ref OBX_box): object operations like put and get  
* [OBX_query_builder](\ref OBX_query_builder): used to construct queries using "query conditions"  
* [OBX_query](\ref OBX_query): the product of a query builder can be executed to find objects matching the previously defined conditions  

Essential Links/Readings
------------------------
* Entity (object) IDs: https://docs.objectbox.io/advanced/object-ids
* Meta model and UIDs: https://docs.objectbox.io/advanced/meta-model-ids-and-uids
