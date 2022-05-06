ObjectBox C and C++ API Changelog
=================================

0.16.0 (2022-05-06)
-------------------
* Allow UTF-8 for database directories on Windows (available for other platforms before)
* Various internal improvements

### C++

* Promoted `Options` to a top level class, as nested classes cannot be declared forward
* New `#define` to disable FlatBuffers includes to simplify new project setup
* Rename `Exception` to `DbException`
* Minor improvements

0.15.2 (2022-02-15)
-------------------
* Add store cloning
* Fix attaching to a reopened store

0.15.1 (2022-01-25)
-------------------
* Fix non-unique indexes triggering unique constraint violations in corner cases (introduced in 0.15.0)
* Minor performance improvements with hashed indexes
* Admin UI now supports multiple sessions to the same host using different ports (session ID via HTTP request)

### Sync

* Performance improvements for compression and decompression

0.15.0 (2021-12-09)
-------------------
* New "Flex" data type that can contain data of various types like integers, floating points, strings, lists and maps 
* New query conditions for Flex lists to find a specific element
* New query conditions for Flex maps to find elements with a specific key or key/value pair
* New unique on-conflict strategy: replace conflicting objects (OBXPropertyFlags_UNIQUE_ON_CONFLICT_REPLACE)
* New functions to attach to existing stores using only the file path (in the same process)
* New APIs for ObjectBox Admin, the web based UI (formerly known as Object Browser): obx_admin_*
* Minor performance improvements for indexed access
* Major performance improvements for tree/GraphQL queries
* ARM binaries are now built for minimal size reducing the library size significantly
* New "no_reader_thread_locals" store option
* Enable debug logging (requires a special build)
* API: Type for query offsets and limits was changed from uint64_t to size_t
* API: rarely used obx_txn_mark_success() was removed; use obx_txn_success()
* API: feature checks consolidated to only use obx_has_feature()
* Many internal improvements
* Core version 3.0.1-2021-12-09

### Sync

* New API for embedded server mode: obx_sync_server_* (implementation available on request)

0.14.0 (2021-05-13)
-------------------
* change `obx_query_prop_count()` to respect case-sensitivity setting when counting distinct strings
* change `OBXSyncCredentialsType` values to start at 1
* add `obx_query_find_first()` to get a first object matching the query
* add `obx_query_find_unique()` to get the only object matching the query
* add `obx_async_put_object4()` accepting a put_mode
* updated ARMv7hf toolchain, now requires GLIBC v2.28 (e.g. Raspbian 10, Ubuntu 20.04)
* semi-internal Dart APIs: add query streaming and finalizers

0.13.0 (2021-03-16)
-------------------
* add Sync binary library variants for all supported platforms
* add MacOS universal binary library, supporting Intel x64 and Apple Silicon arm64
* split Sync symbols out of objectbox.h/pp into objectbox-sync.h/pp
* add Sync server-time getter, listener and local-to-server diff info - to access server time info on clients
* add Sync heartbeat interval configuration and an option to send one immediately
* semi-internal: update Dart/Flutter SDK to v2.0

0.12.0 (2021-02-05)
-------------------
* add Linux ARMv8 (aarch64) native binary library
* add `obx_sync_*` APIs (actual functionality is only available in a sync-enabled version; see https://objectbox.io/sync/)  
* add `obx_has_feature()`, and `OBXFeature` enum: please use these instead of the following, now deprecated functions:
  `obx_supports_bytes_array()`, `obx_supports_time_series()`, and `obx_sync_available()`
* add `obx_remove_db_files()` to delete database files in a given directory  
* add optional `OBXEntityFlags_SHARED_GLOBAL_IDS` for `SYNC_ENABLED` entities  
* semi-internal: add custom async callback APIs for Dart/Flutter language binding

0.11.0 (2020-11-12)
-------------------
* update CMakeLists.txt to simplify integration for users, e.g. with `FetchContent`,
  see the updated [installation docs](https://cpp.objectbox.io/installation#objectbox-library)
* rename `objectbox-cpp.h` to `objectbox.hpp`
* change cursor and box read functions `get/first/current/next` `void**` argument to `const void**`
* change multiple Query and QueryBuilder functions `int count` argument to `size_t count`
* change observer signatures (`obx_err` return value and `size_t count` argument)
* change C++ Store Options to a "builder" pattern and expose all available options 
* new obx_model_entity_flags()
* new obx_opt_async_*() to configure Async box behavior
* new C++ AsyncBox and Box::async() to expose asynchronous operations
* new QueryBuilder greater-or-equal/less-or-equal functions for integers and floats
* new obx_query_offset_limit() setter for offset and limit in a single call
* new obx_sync_available() to check whether the loaded runtime library supports [ObjectBox Sync](https://objectbox.io/sync)   
* clean up linter warnings in the examples and `objectbox.h(pp)`

0.10.0 (2020-08-13)
-------------------
C++ API queries and model classes for more feature-rich generated code.
C-API cleanup & docs updates, including the following changes (some of them breaking due to renames):

##### Misc
* new property type: DateNano for datetime with nanosecond precision
* new store options obx_opt_*()
  * validate_on_open - to validate the database during openining
  * put_padding_mode - configure the padding used by your flatbuffers implementation
  * use_previous_commit - roll-back the database to the previously committed version
  * read_only - open the database in the read only mode
  * debug_flags - configure debug logging

##### Cursor
* remove obx_cursor2(), use obx_cursor() in combination with obx_store_entity_id()
* rename obx_cursor_put_mode() to obx_cursor_put4()
* obx_cursor_put() drops the last parameter `checkForPreviousValue` and introduced a complementary obx_cursor_put_new()
* remove obx_cursor_put_padded()
* rename obx_cursor_backlink_bytes() to obx_cursor_backlinks()
* rename obx_cursor_ts_limits() to obx_cursor_ts_min_max()
* rename obx_cursor_ts_limits_range() to obx_cursor_ts_min_max_range()
  
##### Box  
* rename obx_box_put() to obx_box_put5()
* rename obx_box_put_object() to obx_box_put_object4()
* change obx_box_put_many to fail when any of the individual inserts/updates fails and new obx_box_put_many5() to override this behavior
* new obx_box_store() to get access to OBX_store* owning the given box
* new obx_box_insert() obx_box_update() for for insert and update semantics, same as put mode arg in obx_box_put5()
* new obx_box_put() without a mode argument (defaults to PUT) 
* new obx_box_put_object() without a mode argument (defaults to PUT) 
* new obx_box_ts_min_max() for time-series databases
* new obx_box_ts_min_max_range() for time-series databases

##### Async
* rename obx_async_put_mode() to obx_async_put5()
* rename obx_async_id_put() to obx_async_put_object()
* rename obx_async_id_insert() to obx_async_insert_object()

##### Query builder
* obx_qb_{type}_{operation}() function naming changes to obx_qb_{operation}_{type}(), e.g. obx_qb_int_equal() becomes obx_qb_equals_int()
* obx_qb_{operation}_{type}() functions taking multiple arguments have an "s" at the end, indicating plural, e.g. obx_qb_in_int64s()
* operation `equal` becomes `equals`, `greater` becomes `greater_than`, `less` becomes `less_than` 
* obx_qb_greater_than_string() drops `with_equal` argument in favor of the new obx_qb_greater_or_equal_string() function
* obx_qb_less_than_string() drops `with_equal` argument in favor of the new obx_qb_less_or_equal_string() function
* change obx_qb_in_strings() argument `const char* values[]` changes to `const char* const values[]`, 
  i.e. const array of const char pointers
* obx_qb_greater_than_bytes() drops `with_equal` argument in favor of the new obx_qb_greater_or_equal_bytes() function
* obx_qb_less_than_bytes() drops `with_equal` argument in favor of the new obx_qb_less_or_equal_bytes() function

##### Query
Limit and offset are now part of the query state instead of function arguments, therefore:
* new obx_query_offset() and obx_query_limit() set persistent offset/limit for all future calls to other query functions
* obx_query_find(), obx_query_visit(), obx_query_find_ids(), as well as the cursor alternatives, 
  all drop `offset` and `limit` arguments
* note: some query functions, such as count/remove don't support non-zero offset/limit yet.
* obx_query_{type}_param() function naming changes to obx_query_param_{type}(), e.g. obx_query_string_param() 
  becomes obx_query_param_string() or obx_query_param_strings() for the plural variant 
* obx_query_{type}_param_alias() function naming changes to obx_query_param_alias_{type}(), e.g. obx_query_string_param_alias() 
  becomes obx_query_param_alias_string()
* obx_query_param_strings() and obx_query_param_alias_strings() argument `const char* values[]`  
  changes to `const char* const values[]`, i.e. const array of const char pointers
* obx_query_prop_{type}_find() function naming changes to obx_query_prop_find_{type}s(), with an "s" indicating the return 
  type is plural, e.g. obx_query_prop_find_strings
* remove deprecated obx_query_prop_distinct_string()

0.9.2 (2020-06-30)
------------------
* Rename obx_cursor_ts_limits() to obx_cursor_ts_min_max()
* Add C++ APIs for query links and TS min/max

0.9.1 (2020-06-23)
------------------
* C++ interface improvements:
  * Box::getOptional() overloads returning std::optional
  * Box::put() overloads taking vectors of std::unique_ptr and std::optional
  * Query methods: find(), findIds(), count(), remove(), ...
  * Ensure double-free can't happen (added explicit copy & move constructors)
* Fixed Windows exported symbols - recently added APIs were missing
* New obx_cursor_put_object4() to allow passing a PutMode
* Make *_close() functions consistently accept nullptr 

0.9.0 (2020-06-18)
------------------
* C++ API added: see include/objectbox-cpp.h
* Initial time series support (ObjectBox TS only)
* New "put object" functions that e.g. handle ID assignment
* Several internal improvements, e.g. query links are resolved faster 

0.8.2 (2020-01-13)
------------------
* Fix ARM build incorrectly returning TRUE in obx_supports_bytes_array()
  
0.8.1 (2019-12-12)
------------------
* Bug fix for obx_box_rel_get_ids(), which did return wrong IDs in some cases (please update!)
* Several refinements on property queries, for example:
  * Maintain floating point point semantics on aggregates, e.g. infinity + 1 == infinity (no overflow)
  * Allow aggregates on Date type properties
  * Return negative counts if results were obtained using a short cut (API change to int64_t*)

0.8.0 (2019-12-05)
------------------
* Property queries compute sums and averages more precisely (improved algorithms and wider types)
* Property queries now consider unsigned types
* Added an additional out parameter for count obx_query_prop_*() 
* Added put alternatives to "put" with cursor: obx_cursor_insert() and obx_cursor_update()
* Added obx_query_clone() to allow clones to run in parallel on separate threads

0.7.2 (2019-10-30)
------------------
* new obx_store_wrap() (use with Java version 2.4.1) 
* Minor fixes

0.7.1 (2019-10-16)
------------------
* Query performance improvements
* Improved API docs
* Minor fixes

0.7.0 (2019-09-09)
------------------
* Added observers to listen to data changes (e.g. obx_observe())
* Added obx_last_error_pop() to get and reset the last error
* Added obx_box_rel_get_backlink_ids() to get backlinks of standalone relations
* iOS and macOS related improvements; e.g. obx_posix_sem_prefix_set() for sand-boxed macOS apps
* Better resilience for passing in NULL for many functions
* Improved API docs
    * Quite a few functions got comments
    * Online API docs (Doxygen)  
* Minor fixes

### (Breaking) Changes

* **obx_txn_success(txn) now also closes the transaction making a subsequent obx_txn_close(txn) unnecessary and illegal.**
  Please adjust your code using one of two possibilities (depending on your code flow):
   * drop `obx_txn_close(txn)` after calling `obx_txn_success(txn)`, or
   * replace `obx_txn_success(txn)` with `obx_txn_mark_success(txn, true)` if you want to keep `obx_txn_close(txn)`
* Function renames to drop "create" postfix:
  obx_query_builder(), obx_query(), obx_cursor(), obx_model(), obx_bytes_array(), obx_id_array()
* Property queries API clean up using a OBX_query_prop struct 

0.6.0 (2019-07-15)
------------------
* Box API is now on a par with Cursor API.
  The Box API is usually preferred because it comes with implicit transaction management. 
  * Boxes don't have to be closed anymore (obx_box_close() removed) 
  * Basic operations like obx_box_get(), obx_box_put_many(), obx_box_remove()
  * Relation operations
* Transactions can now be created recursively.
  The most outer transaction defines the transaction scope for the database.
* New async API (OBX_async struct) including put, insert, update and remove operations
* Added property queries with results referring to single property (not complete objects).
  Note: APIs will change with next release.
* Several smaller improvements; e.g.
  obx_query_* functions don't require a cursor anymore, added obx_store_await_async_submitted(), ...

### (Breaking) Changes

* The OBX_store_options struct is now private
  * Build OBX_store_options using obx_opt()
  * Set options using obx_opt_*()
* Transactions functions were renamed and changed to recursive semantics. E.g.:
  * Create transactions with obx_txn_write() or obx_txn_read()
  * Mark as successful using obx_txn_success() instead of committing
* Renamed obx_box_create() to obx_box()
* Renamed obx_query_* functions to obx_query_cursor_*

0.5.0 (2019-02-21)
------------------
Additions

* unsigned support for numeric types using `OBXPropertyFlags_UNSIGNED`
* string vector support (FlatBuffers `[string]`)
* `qb_strings_contain()` checks whether a string vector contains a given string
* `remove_db_files()` removes the database in the given directory
* `supports_bytes_array()` provides info if you can use functions to get whole data sets at once
* `model_relation()` to define standalone many-to-many relations between two entities
* `cursor_rel_put()`, `remove()`, `ids()` to add, remove & get standalone relations
* `qb_link_property()`, `qb_backlink_property()` to query across property based (to-one) relation links
* `qb_link_standalone()`, `qb_backlink_standalone()` to query across standalone many-to-many relation links
* `cursor_put_padded()` in case your FlatBuffers data is not divisible by 4
* `cursor_seek()` to point to the the element with given id (and also check if that element exists)
* `corsor_current()` to get the data of the current element (similar to `cursor_first()`, `next()`)
* `query_visit()` to traverse over the query results and call the provided callback on each item
         
Changes:

* prefix defined constants (enums) with OBX
* `model_*` functions now always return the first error (as it may be the cause of other follow-up errors as well)
* `box_put_async()` now takes timeout in milliseconds (previously it was a fixed number)
* `query_find()` & query_find_ids() now also take offset & limit as arguments (pass 0s for previous behavior)
* renamed `qb_parameter_alias()` to `qb_param_alias()`
* renamed `query_describe_parameters()` to `query_describe_params()`
* renamed `query_to_string()` to `query_describe()`
* `query_*_param()` and `query_*_params_in()` now take `entity_id` as an argument (you can pass 0 for queries without relation links)

0.4.1 (2018-12-17)
------------------
* Works on Ubuntu 16+

0.4.0 (2018-11-27)
------------------
* Add obx_cursor_count_max and obx_cursor_is_empty can be a high-performance alternative to obx_cursor_count
* Preemptive throttling for async puts

0.3.0 (2018-11-12)
------------------
* ARM support (v6 and v7)
* New typedefs : obx_id, obx_schema_id, obx_uid, obx_err
* Added obx_cursor_get_all as an alternative to iterating over all objects

0.2.0 (2018-09-12)
------------------
* Added queries (obx_qb_* and obx_query_*)
* Added property based relations one-to-many relations

0.1.0 (2018-09-04)
------------------
Initial release