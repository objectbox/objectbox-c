// Code generated by ObjectBox; DO NOT EDIT.

#pragma once

#ifdef __cplusplus
#include <cstdbool>
#include <cstdint>
extern "C" {
#else
#include <stdbool.h>
#include <stdint.h>
#endif
#include "objectbox.h"

/// Initializes an ObjectBox model for all entities. 
/// The returned pointer may be NULL if the allocation failed. If the returned model is not NULL, you should check if   
/// any error occurred by calling obx_model_error_code() and/or obx_model_error_message(). If an error occurred, you're
/// responsible for freeing the resources by calling obx_model_free().
/// In case there was no error when setting the model up (i.e. obx_model_error_code() returned 0), you may configure 
/// OBX_store_options with the model by calling obx_opt_model() and subsequently opening a store with obx_store_open().
/// As soon as you call obx_store_open(), the model pointer is consumed and MUST NOT be freed manually.
static inline OBX_model* create_obx_model() {
    OBX_model* model = obx_model();
    if (!model) return NULL;
    
    obx_model_entity(model, "City", 1, 607353604830599953);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 3289333370985535139);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "name", OBXPropertyType_String, 2, 8840693632700366740);
    obx_model_property(model, "location", OBXPropertyType_FloatVector, 3, 6028438376669885699);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_hnsw_dimensions(model, 2);
    obx_model_property_index_hnsw_distance_type(model, OBXVectorDistanceType_Geo);
    obx_model_property_index_id(model, 1, 4616869581890712534);
    obx_model_entity_last_property_id(model, 3, 6028438376669885699);
    
    obx_model_last_entity_id(model, 1, 607353604830599953);
    obx_model_last_index_id(model, 1, 4616869581890712534);
    return model; // NOTE: the returned model will contain error information if an error occurred.
}

#ifdef __cplusplus
}
#endif
