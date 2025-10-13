/* © 2025 Casey Koons All rights reserved */

/* Registry persistence - save/load state and statistics */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_registry.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"
#include "argo_file_utils.h"

/* Save registry state to file */
int registry_save_state(ci_registry_t* registry, const char* filepath) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(filepath);

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        argo_report_error(E_SYSTEM_FILE, "registry_save_state",
                         ERR_FMT_FAILED_TO_OPEN, filepath);
        return E_SYSTEM_FILE;
    }

    /* Write JSON header */
    fprintf(fp, "{\n");
    fprintf(fp, "  \"version\": 1,\n");
    fprintf(fp, "  \"count\": %d,\n", registry->count);
    fprintf(fp, "  \"entries\": [\n");

    /* Write each CI entry - walk the linked list */
    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"name\": \"%s\",\n", entry->name);
        fprintf(fp, "      \"role\": \"%s\",\n", entry->role);
        fprintf(fp, "      \"model\": \"%s\",\n", entry->model);
        fprintf(fp, "      \"host\": \"%s\",\n", entry->host);
        fprintf(fp, "      \"port\": %d,\n", entry->port);
        fprintf(fp, "      \"status\": %d,\n", entry->status);
        fprintf(fp, "      \"registered_at\": %ld\n", (long)entry->registered_at);
        fprintf(fp, "    }%s\n", entry->next ? "," : "");
        entry = entry->next;
    }

    /* Write JSON footer */
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);
    LOG_INFO("Saved registry state to %s (%d CIs)", filepath, registry->count);
    return ARGO_SUCCESS;
}

/* Helper: Extract string field from JSON entry */
static int extract_string_field(const char* ptr, const char* entry_end, const char* field_name,
                                char* out_buffer, size_t buffer_size) {
    char search_str[64];
    snprintf(search_str, sizeof(search_str), "\"%s\":", field_name);

    const char* field_start = strstr(ptr, search_str);
    if (!field_start || field_start >= entry_end) {
        return -1;
    }

    field_start = strchr(field_start + strlen(search_str), '"');
    if (!field_start || field_start >= entry_end) {
        return -1;
    }

    field_start++;  /* Skip opening quote */
    const char* field_end = strchr(field_start, '"');
    if (!field_end || field_end >= entry_end) {
        return -1;
    }

    size_t len = field_end - field_start;
    if (len >= buffer_size) {
        len = buffer_size - 1;
    }

    strncpy(out_buffer, field_start, len);
    out_buffer[len] = '\0';
    return 0;
}

/* Helper: Extract integer field from JSON entry */
static int extract_int_field(const char* ptr, const char* entry_end, const char* field_name) {
    char search_str[64];
    snprintf(search_str, sizeof(search_str), "\"%s\":", field_name);

    const char* field_start = strstr(ptr, search_str);
    if (!field_start || field_start >= entry_end) {
        return 0;
    }

    int value = 0;
    sscanf(field_start + strlen(search_str), "%d", &value);
    return value;
}

/* Load registry state from file */
int registry_load_state(ci_registry_t* registry, const char* filepath) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(filepath);

    /* Read entire file */
    char* json = NULL;
    size_t file_size = 0;
    int result = file_read_all(filepath, &json, &file_size);
    if (result != ARGO_SUCCESS) {
        /* File not existing is not an error for load */
        LOG_DEBUG("Registry state file not found: %s", filepath);
        return ARGO_SUCCESS;
    }

    if (file_size == 0) {
        free(json);
        return ARGO_SUCCESS;
    }

    /* Parse JSON to extract CI entries */
    const char* entries_start = strstr(json, "\"entries\":");
    if (!entries_start) {
        free(json);
        LOG_WARN("No entries field in registry file");
        return ARGO_SUCCESS;
    }

    /* Find start of entries array */
    const char* ptr = strchr(entries_start, '[');
    if (!ptr) {
        free(json);
        return ARGO_SUCCESS;
    }
    ptr++;

    /* Parse each entry */
    int loaded_count = 0;
    while (*ptr && *ptr != ']') {
        /* Skip whitespace */
        while (*ptr && (*ptr == ' ' || *ptr == '\n' || *ptr == '\t' || *ptr == ',')) ptr++;
        if (*ptr != '{') break;

        /* Find entry end */
        const char* entry_end = strchr(ptr, '}');
        if (!entry_end) break;

        /* Extract fields within this entry bounds */
        char name[REGISTRY_NAME_MAX] = {0};
        char role[REGISTRY_ROLE_MAX] = {0};
        char model[REGISTRY_MODEL_MAX] = {0};

        extract_string_field(ptr, entry_end, "name", name, sizeof(name));
        extract_string_field(ptr, entry_end, "role", role, sizeof(role));
        extract_string_field(ptr, entry_end, "model", model, sizeof(model));

        int port = extract_int_field(ptr, entry_end, "port");
        int status = extract_int_field(ptr, entry_end, "status");

        /* Add CI to registry */
        if (name[0] && role[0] && model[0] && port > 0) {
            int result = registry_add_ci(registry, name, role, model, port);
            if (result == ARGO_SUCCESS) {
                /* Restore status */
                ci_registry_entry_t* entry = registry_find_ci(registry, name);
                if (entry) {
                    entry->status = (ci_status_t)status;
                }
                loaded_count++;
            }
        }

        /* Move to next entry */
        ptr = entry_end + 1;
    }

    LOG_INFO("Loaded registry state from %s (%d CIs)", filepath, loaded_count);

    free(json);
    return ARGO_SUCCESS;
}

/* Get statistics */
registry_stats_t* registry_get_stats(ci_registry_t* registry) {
    if (!registry) return NULL;

    registry_stats_t* stats = calloc(1, sizeof(registry_stats_t));
    if (!stats) return NULL;

    stats->total_cis = registry->count;
    stats->online_cis = 0;
    stats->busy_cis = 0;
    stats->total_messages = 0;
    stats->total_errors = 0;

    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (entry->status != CI_STATUS_OFFLINE) {
            stats->online_cis++;
        }
        if (entry->status == CI_STATUS_BUSY) {
            stats->busy_cis++;
        }
        stats->total_messages += entry->messages_sent + entry->messages_received;
        stats->total_errors += entry->errors_count;
        entry = entry->next;
    }

    return stats;
}

void registry_free_stats(registry_stats_t* stats) {
    free(stats);
}
