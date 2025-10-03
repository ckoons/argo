/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_JSON_H
#define ARGO_WORKFLOW_JSON_H

#include <stddef.h>

/* Forward declarations */
typedef struct jsmntok jsmntok_t;

/* Workflow JSON parsing constants */
#define WORKFLOW_JSON_MAX_TOKENS 512
#define WORKFLOW_JSON_MAX_DEPTH 10

/* Load JSON file into buffer
 *
 * Reads entire file and returns allocated buffer with contents.
 *
 * Parameters:
 *   path - File path to read
 *   out_size - Receives size of loaded data
 *
 * Returns:
 *   Allocated buffer (caller must free) on success
 *   NULL on failure
 */
char* workflow_json_load_file(const char* path, size_t* out_size);

/* Parse JSON string
 *
 * Tokenizes JSON using jsmn parser.
 *
 * Parameters:
 *   json - JSON string to parse
 *   tokens - Token array to fill
 *   max_tokens - Maximum tokens
 *
 * Returns:
 *   Number of tokens parsed (>= 0) on success
 *   Negative jsmn error code on failure
 */
int workflow_json_parse(const char* json, jsmntok_t* tokens, int max_tokens);

/* Find object field by name
 *
 * Searches for field in object and returns value token index.
 *
 * Parameters:
 *   json - JSON string
 *   tokens - Parsed tokens
 *   object_index - Index of object token to search
 *   field_name - Field name to find
 *
 * Returns:
 *   Token index of field value (>= 0) on success
 *   -1 if field not found
 */
int workflow_json_find_field(const char* json, jsmntok_t* tokens,
                             int object_index, const char* field_name);

/* Extract string from token
 *
 * Copies string value from token to buffer.
 *
 * Parameters:
 *   json - JSON string
 *   token - Token to extract
 *   buffer - Output buffer
 *   buffer_size - Size of buffer
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_TOO_LARGE if value too long
 */
int workflow_json_extract_string(const char* json, jsmntok_t* token,
                                 char* buffer, size_t buffer_size);

/* Extract integer from token
 *
 * Parses integer value from token.
 *
 * Parameters:
 *   json - JSON string
 *   token - Token to parse
 *   out_value - Receives parsed integer
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_INVALID if not valid integer
 */
int workflow_json_extract_int(const char* json, jsmntok_t* token, int* out_value);

#endif /* ARGO_WORKFLOW_JSON_H */
