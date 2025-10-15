/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_YAML_H
#define ARGO_YAML_H

#include <stddef.h>

/* YAML key-value pair callback */
typedef void (*yaml_kv_callback_t)(const char* key, const char* value, void* userdata);

/* Simple YAML parser for key: value pairs */
int yaml_parse_simple(const char* content, yaml_kv_callback_t callback, void* userdata);

/* Parse YAML file directly */
int yaml_parse_file(const char* path, yaml_kv_callback_t callback, void* userdata);

/* Utility: Get single value by key from YAML content */
int yaml_get_value(const char* content, const char* key, char* value, size_t value_size);

#endif /* ARGO_YAML_H */
