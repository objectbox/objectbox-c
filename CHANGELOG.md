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