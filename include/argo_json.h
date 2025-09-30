/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_JSON_H
#define ARGO_JSON_H

#include <stddef.h>

/* JSON parsing constants */
#define JSON_MAX_FIELD_DEPTH 5
#define JSON_FIELD_NAME_SIZE 64

/* Extract a string field from JSON
 *
 * Searches for "field_name" in json string and extracts the quoted string value.
 * Handles nested fields by searching from start position.
 *
 * Parameters:
 *   json - JSON string to parse
 *   field_name - Name of field to find (e.g., "text", "content")
 *   out_value - Pointer to receive allocated string (caller must free)
 *   out_len - Pointer to receive length of extracted string
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_PROTOCOL_FORMAT if field not found or malformed
 *   E_SYSTEM_MEMORY if allocation fails
 */
int json_extract_string_field(const char* json, const char* field_name,
                              char** out_value, size_t* out_len);

/* Extract a nested string field from JSON
 *
 * Searches for a path of fields (e.g., ["candidates", "content", "text"])
 * and extracts the final string value.
 *
 * Parameters:
 *   json - JSON string to parse
 *   field_path - Array of field names to traverse
 *   path_depth - Number of fields in path
 *   out_value - Pointer to receive allocated string (caller must free)
 *   out_len - Pointer to receive length of extracted string
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_PROTOCOL_FORMAT if field not found or malformed
 *   E_SYSTEM_MEMORY if allocation fails
 */
int json_extract_nested_string(const char* json, const char** field_path,
                               int path_depth, char** out_value, size_t* out_len);

#endif /* ARGO_JSON_H */
