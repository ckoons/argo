/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_memory.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_json.h"
#include "argo_file_utils.h"
#include "argo_limits.h"

/* Static ID counter */
static uint32_t g_next_memory_id = 1;

/* Create memory digest */
ci_memory_digest_t* memory_digest_create(size_t context_limit) {
    ci_memory_digest_t* digest = calloc(1, sizeof(ci_memory_digest_t));
    if (!digest) {
        argo_report_error(E_SYSTEM_MEMORY, "memory_digest_create", "");
        return NULL;
    }

    digest->max_allowed_size = (context_limit * MEMORY_MAX_PERCENTAGE) / PERCENTAGE_DIVISOR;
    digest->json_content = NULL;
    digest->json_size = 0;
    digest->suggestion_count = 0;
    digest->selected_count = 0;
    digest->breadcrumb_count = 0;
    digest->created = time(NULL);

    LOG_INFO("Created memory digest with max size %zu bytes", digest->max_allowed_size);
    return digest;
}

/* Destroy memory digest */
void memory_digest_destroy(ci_memory_digest_t* digest) {
    if (!digest) return;

    free(digest->json_content);
    free(digest->sunset_notes);
    free(digest->sunrise_brief);
    free(digest->index);

    /* Free breadcrumbs */
    for (int i = 0; i < digest->breadcrumb_count; i++) {
        free(digest->breadcrumbs[i]);
    }

    LOG_INFO("Destroyed memory digest");
    free(digest);
}

/* Add memory item */
int memory_add_item(ci_memory_digest_t* digest,
                   memory_type_t type,
                   const char* content,
                   const char* creator_ci) {
    int result = ARGO_SUCCESS;
    memory_item_t* item = NULL;

    ARGO_CHECK_NULL(digest);
    ARGO_CHECK_NULL(content);

    if (digest->selected_count >= MEMORY_MAX_ITEMS) {
        argo_report_error(E_PROTOCOL_QUEUE, "memory_add_item", "");
        return E_PROTOCOL_QUEUE;
    }

    item = calloc(1, sizeof(memory_item_t));
    if (!item) {
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    item->content = strdup(content);
    if (!item->content) {
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    item->id = g_next_memory_id++;
    item->type = type;
    item->content_size = strlen(content);
    item->created = time(NULL);
    item->relevance.score = 1.0f;
    item->relevance.last_accessed = time(NULL);
    item->relevance.access_count = 0;

    if (creator_ci) {
        item->creator_ci = strdup(creator_ci);
        if (!item->creator_ci) {
            result = E_SYSTEM_MEMORY;
            goto cleanup;
        }
    }

    digest->selected[digest->selected_count++] = item;

    LOG_DEBUG("Added memory item %u (type=%d, size=%zu)",
             item->id, type, item->content_size);

    return ARGO_SUCCESS;

cleanup:
    if (item) {
        free(item->content);
        free(item->creator_ci);
        free(item);
    }
    return result;
}

/* Add breadcrumb */
int memory_add_breadcrumb(ci_memory_digest_t* digest,
                         const char* breadcrumb) {
    ARGO_CHECK_NULL(digest);
    ARGO_CHECK_NULL(breadcrumb);

    if (digest->breadcrumb_count >= MEMORY_BREADCRUMB_MAX) {
        argo_report_error(E_PROTOCOL_QUEUE, "memory_add_breadcrumb", "");
        return E_PROTOCOL_QUEUE;
    }

    digest->breadcrumbs[digest->breadcrumb_count++] = strdup(breadcrumb);

    LOG_DEBUG("Added breadcrumb: %s", breadcrumb);
    return ARGO_SUCCESS;
}

/* Suggest relevant memories (basic implementation) */
int memory_suggest_relevant(ci_memory_digest_t* digest,
                           const char* task_context,
                           int max_suggestions) {
    ARGO_CHECK_NULL(digest);
    (void)task_context;
    (void)max_suggestions;

    LOG_DEBUG("Memory suggestion requested");
    return ARGO_SUCCESS;
}

/* Suggest by type */
int memory_suggest_by_type(ci_memory_digest_t* digest,
                          memory_type_t type,
                          int max_suggestions) {
    ARGO_CHECK_NULL(digest);

    int found = 0;
    for (int i = 0; i < digest->selected_count && found < max_suggestions; i++) {
        memory_item_t* item = digest->selected[i];
        if (item && item->type == type) {
            if (digest->suggestion_count < MEMORY_SUGGESTION_MAX) {
                digest->suggested[digest->suggestion_count++] = item;
                found++;
            }
        }
    }

    LOG_DEBUG("Suggested %d items of type %d", found, type);
    return found;
}

/* Select memory item */
int memory_select_item(ci_memory_digest_t* digest,
                      uint32_t memory_id) {
    ARGO_CHECK_NULL(digest);

    for (int i = 0; i < digest->selected_count; i++) {
        if (digest->selected[i] && digest->selected[i]->id == memory_id) {
            digest->selected[i]->relevance.access_count++;
            digest->selected[i]->relevance.last_accessed = time(NULL);
            LOG_DEBUG("Selected memory item %u", memory_id);
            return ARGO_SUCCESS;
        }
    }

    argo_report_error(E_INPUT_INVALID, "memory_mark_selected", "item %u", memory_id);
    return E_INPUT_INVALID;
}

/* Select suggested memories */
int memory_select_suggested(ci_memory_digest_t* digest,
                           int suggestion_indices[],
                           int count) {
    ARGO_CHECK_NULL(digest);
    ARGO_CHECK_NULL(suggestion_indices);

    int selected = 0;
    for (int i = 0; i < count; i++) {
        int idx = suggestion_indices[i];
        if (idx >= 0 && idx < digest->suggestion_count) {
            memory_item_t* item = digest->suggested[idx];
            if (item) {
                item->relevance.ci_marked_important = true;
                selected++;
            }
        }
    }

    LOG_DEBUG("Selected %d suggested memories", selected);
    return selected;
}

/* Set sunset notes */
int memory_set_sunset_notes(ci_memory_digest_t* digest,
                           const char* notes) {
    ARGO_CHECK_NULL(digest);
    ARGO_CHECK_NULL(notes);

    free(digest->sunset_notes);
    digest->sunset_notes = strdup(notes);

    LOG_DEBUG("Set sunset notes (%zu bytes)", strlen(notes));
    return ARGO_SUCCESS;
}

/* Set sunrise brief */
int memory_set_sunrise_brief(ci_memory_digest_t* digest,
                            const char* brief) {
    ARGO_CHECK_NULL(digest);
    ARGO_CHECK_NULL(brief);

    free(digest->sunrise_brief);
    digest->sunrise_brief = strdup(brief);

    LOG_DEBUG("Set sunrise brief (%zu bytes)", strlen(brief));
    return ARGO_SUCCESS;
}

/* JSON serialization  */
char* memory_digest_to_json(ci_memory_digest_t* digest) {
    if (!digest) return NULL;

    char* json = malloc(MEMORY_JSON_BUFFER_SIZE);
    if (!json) return NULL;

    int offset = 0;
    offset += snprintf(json + offset, MEMORY_JSON_BUFFER_SIZE - offset,
                      "{\"session_id\":\"%s\",\"ci_name\":\"%s\","
                      "\"created\":%ld,\"item_count\":%d,",
                      digest->session_id, digest->ci_name,
                      (long)digest->created, digest->selected_count);

    /* Add sunset notes */
    offset += snprintf(json + offset, MEMORY_JSON_BUFFER_SIZE - offset,
                      "\"sunset_notes\":");
    if (digest->sunset_notes) {
        json[offset++] = '"';
        size_t temp_offset = offset;
        json_escape_string(json, MEMORY_JSON_BUFFER_SIZE, &temp_offset, digest->sunset_notes);
        offset = temp_offset;
        json[offset++] = '"';
    } else {
        offset += snprintf(json + offset, MEMORY_JSON_BUFFER_SIZE - offset, "null");
    }
    json[offset++] = ',';
    json[offset] = '\0';

    /* Add sunrise brief */
    offset += snprintf(json + offset, MEMORY_JSON_BUFFER_SIZE - offset,
                      "\"sunrise_brief\":");
    if (digest->sunrise_brief) {
        json[offset++] = '"';
        size_t temp_offset = offset;
        json_escape_string(json, MEMORY_JSON_BUFFER_SIZE, &temp_offset, digest->sunrise_brief);
        offset = temp_offset;
        json[offset++] = '"';
    } else {
        offset += snprintf(json + offset, MEMORY_JSON_BUFFER_SIZE - offset, "null");
    }

    /* Add breadcrumbs */
    offset += snprintf(json + offset, MEMORY_JSON_BUFFER_SIZE - offset,
                      ",\"breadcrumbs\":[");

    for (int i = 0; i < digest->breadcrumb_count; i++) {
        offset += snprintf(json + offset, MEMORY_JSON_BUFFER_SIZE - offset,
                          "%s\"%s\"", i > 0 ? "," : "",
                          digest->breadcrumbs[i]);
    }

    offset += snprintf(json + offset, MEMORY_JSON_BUFFER_SIZE - offset, "]}");

    return json;
}

/* Deserialize from JSON  */
ci_memory_digest_t* memory_digest_from_json(const char* json,
                                           size_t context_limit) {
    if (!json) return NULL;

    ci_memory_digest_t* digest = memory_digest_create(context_limit);
    if (!digest) return NULL;

    /* Parse session_id */
    const char* session_start = strstr(json, MEMORY_JSON_SESSION_ID);
    if (session_start) {
        session_start += strlen(MEMORY_JSON_SESSION_ID);
        const char* session_end = strchr(session_start, '"');
        if (session_end) {
            size_t len = session_end - session_start;
            if (len < sizeof(digest->session_id)) {
                strncpy(digest->session_id, session_start, len);
            }
        }
    }

    /* Parse ci_name */
    const char* ci_start = strstr(json, MEMORY_JSON_CI_NAME);
    if (ci_start) {
        ci_start += strlen(MEMORY_JSON_CI_NAME);
        const char* ci_end = strchr(ci_start, '"');
        if (ci_end) {
            size_t len = ci_end - ci_start;
            if (len < sizeof(digest->ci_name)) {
                strncpy(digest->ci_name, ci_start, len);
            }
        }
    }

    /* Parse sunset_notes */
    const char* sunset_start = strstr(json, MEMORY_JSON_SUNSET_NOTES);
    if (sunset_start) {
        sunset_start += strlen(MEMORY_JSON_SUNSET_NOTES);
        const char* sunset_end = strchr(sunset_start, '"');
        if (sunset_end) {
            size_t len = sunset_end - sunset_start;
            digest->sunset_notes = strndup(sunset_start, len);
        }
    }

    /* Parse sunrise_brief */
    const char* sunrise_start = strstr(json, MEMORY_JSON_SUNRISE_BRIEF);
    if (sunrise_start) {
        sunrise_start += strlen(MEMORY_JSON_SUNRISE_BRIEF);
        const char* sunrise_end = strchr(sunrise_start, '"');
        if (sunrise_end) {
            size_t len = sunrise_end - sunrise_start;
            digest->sunrise_brief = strndup(sunrise_start, len);
        }
    }

    /* Parse breadcrumbs */
    const char* breadcrumbs_start = strstr(json, MEMORY_JSON_BREADCRUMBS);
    if (breadcrumbs_start) {
        const char* ptr = breadcrumbs_start + strlen(MEMORY_JSON_BREADCRUMBS);
        while (*ptr && *ptr != ']' && digest->breadcrumb_count < MEMORY_BREADCRUMB_MAX) {
            /* Skip whitespace and commas */
            while (*ptr && (*ptr == ' ' || *ptr == ',' || *ptr == '\n' || *ptr == '\t')) ptr++;
            if (*ptr != '"') break;
            ptr++;

            const char* end = strchr(ptr, '"');
            if (!end) break;

            size_t len = end - ptr;
            digest->breadcrumbs[digest->breadcrumb_count] = strndup(ptr, len);
            if (digest->breadcrumbs[digest->breadcrumb_count]) {
                digest->breadcrumb_count++;
            }

            ptr = end + 1;
        }
    }

    return digest;
}

/* Calculate memory size */
size_t memory_calculate_size(ci_memory_digest_t* digest) {
    if (!digest) return 0;

    size_t total = 0;

    for (int i = 0; i < digest->selected_count; i++) {
        if (digest->selected[i]) {
            total += digest->selected[i]->content_size;
        }
    }

    if (digest->sunset_notes) {
        total += strlen(digest->sunset_notes);
    }

    if (digest->sunrise_brief) {
        total += strlen(digest->sunrise_brief);
    }

    return total;
}

/* Check size limit */
bool memory_check_size_limit(ci_memory_digest_t* digest) {
    if (!digest) return false;

    size_t current_size = memory_calculate_size(digest);
    return current_size <= digest->max_allowed_size;
}

/* Calculate relevance  */
float memory_calculate_relevance(memory_item_t* item,
                                const char* current_task) {
    if (!item) return 0.0f;
    (void)current_task;

    return item->relevance.score;
}

/* Update relevance */
int memory_update_relevance(memory_item_t* item,
                           float new_score) {
    ARGO_CHECK_NULL(item);

    if (new_score < 0.0f || new_score > 1.0f) {
        return E_INPUT_RANGE;
    }

    item->relevance.score = new_score;
    return ARGO_SUCCESS;
}

/* Decay relevance */
int memory_decay_relevance(ci_memory_digest_t* digest,
                          float decay_factor) {
    ARGO_CHECK_NULL(digest);

    for (int i = 0; i < digest->selected_count; i++) {
        if (digest->selected[i]) {
            digest->selected[i]->relevance.score *= decay_factor;
        }
    }

    LOG_DEBUG("Applied relevance decay factor %.2f", decay_factor);
    return ARGO_SUCCESS;
}

/* Save to file  */
int memory_save_to_file(ci_memory_digest_t* digest,
                       const char* filepath) {
    ARGO_CHECK_NULL(digest);
    ARGO_CHECK_NULL(filepath);

    char* json = memory_digest_to_json(digest);
    if (!json) {
        return E_SYSTEM_MEMORY;
    }

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        free(json);
        return E_SYSTEM_FILE;
    }

    fputs(json, fp);
    fclose(fp);
    free(json);

    LOG_INFO("Saved memory digest to %s", filepath);
    return ARGO_SUCCESS;
}

/* Load from file  */
ci_memory_digest_t* memory_load_from_file(const char* filepath,
                                         size_t context_limit) {
    if (!filepath) return NULL;

    /* Read entire file */
    char* json = NULL;
    size_t file_size = 0;
    int result = file_read_all(filepath, &json, &file_size);
    if (result != ARGO_SUCCESS) {
        argo_report_error(E_SYSTEM_FILE, "memory_load_from_file", filepath);
        return NULL;
    }

    if (file_size == 0) {
        free(json);
        argo_report_error(E_SYSTEM_FILE, "memory_load_from_file", "File is empty");
        return NULL;
    }

    /* Parse JSON */
    ci_memory_digest_t* digest = memory_digest_from_json(json, context_limit);
    free(json);

    if (digest) {
        LOG_INFO("Loaded memory digest from %s", filepath);
    } else {
        argo_report_error(E_PROTOCOL_FORMAT, "memory_load_from_file", "JSON parse failed");
    }

    return digest;
}

/* Print summary */
void memory_print_summary(ci_memory_digest_t* digest) {
    if (!digest) return;

    printf("Memory Digest Summary:\n");
    printf("  Session: %s (CI: %s)\n", digest->session_id, digest->ci_name);
    printf("  Items: %d/%d\n", digest->selected_count, MEMORY_MAX_ITEMS);
    printf("  Breadcrumbs: %d/%d\n", digest->breadcrumb_count, MEMORY_BREADCRUMB_MAX);
    printf("  Size: %zu/%zu bytes (%.1f%%)\n",
           memory_calculate_size(digest),
           digest->max_allowed_size,
           (memory_calculate_size(digest) * (double)PERCENTAGE_DIVISOR) / digest->max_allowed_size);
}

/* Print item */
void memory_print_item(memory_item_t* item) {
    if (!item) return;

    const char* types[] = {
        "FACT", "DECISION", "APPROACH", "ERROR", "SUCCESS", "BREADCRUMB", "RELATIONSHIP"
    };

    printf("  [%u] %s: %s (relevance=%.2f, accessed=%d times)\n",
           item->id, types[item->type], item->content,
           item->relevance.score, item->relevance.access_count);
}

/* Validate digest */
int memory_validate_digest(ci_memory_digest_t* digest) {
    ARGO_CHECK_NULL(digest);

    if (!memory_check_size_limit(digest)) {
        argo_report_error(E_PROTOCOL_SIZE, "memory_validate_digest", "");
        return E_PROTOCOL_SIZE;
    }

    if (digest->selected_count > MEMORY_MAX_ITEMS) {
        argo_report_error(E_PROTOCOL_QUEUE, "memory_validate_digest", "");
        return E_PROTOCOL_QUEUE;
    }

    LOG_DEBUG("Memory digest validation passed");
    return ARGO_SUCCESS;
}