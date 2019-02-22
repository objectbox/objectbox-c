0.5.0 (2019-02-21)
------------------
* additions:
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
         
* changes:
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