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

/* Load registry state from file */
int registry_load_state(ci_registry_t* registry, const char* filepath) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(filepath);

    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        /* File not existing is not an error for load */
        LOG_DEBUG("Registry state file not found: %s", filepath);
        return ARGO_SUCCESS;
    }

    /* Read entire file */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0) {
        fclose(fp);
        return ARGO_SUCCESS;
    }

    char* json = malloc(size + 1);
    if (!json) {
        fclose(fp);
        return E_SYSTEM_MEMORY;
    }

    size_t read_size = fread(json, 1, size, fp);
    json[read_size] = '\0';
    fclose(fp);

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
        int port = 0;
        int status = 0;

        /* Find name */
        const char* name_start = strstr(ptr, "\"name\":");
        if (name_start && name_start < entry_end) {
            name_start = strchr(name_start + 7, '"');
            if (name_start && name_start < entry_end) {
                name_start++;
                const char* name_end = strchr(name_start, '"');
                if (name_end && name_end < entry_end) {
                    size_t len = name_end - name_start;
                    if (len < REGISTRY_NAME_MAX) {
                        strncpy(name, name_start, len);
                    }
                }
            }
        }

        /* Find role */
        const char* role_start = strstr(ptr, "\"role\":");
        if (role_start && role_start < entry_end) {
            role_start = strchr(role_start + 7, '"');
            if (role_start && role_start < entry_end) {
                role_start++;
                const char* role_end = strchr(role_start, '"');
                if (role_end && role_end < entry_end) {
                    size_t len = role_end - role_start;
                    if (len < REGISTRY_ROLE_MAX) {
                        strncpy(role, role_start, len);
                    }
                }
            }
        }

        /* Find model */
        const char* model_start = strstr(ptr, "\"model\":");
        if (model_start && model_start < entry_end) {
            model_start = strchr(model_start + 8, '"');
            if (model_start && model_start < entry_end) {
                model_start++;
                const char* model_end = strchr(model_start, '"');
                if (model_end && model_end < entry_end) {
                    size_t len = model_end - model_start;
                    if (len < REGISTRY_MODEL_MAX) {
                        strncpy(model, model_start, len);
                    }
                }
            }
        }

        /* Find port */
        const char* port_field = strstr(ptr, "\"port\":");
        if (port_field && port_field < entry_end) {
            sscanf(port_field + 7, "%d", &port);
        }

        /* Find status */
        const char* status_field = strstr(ptr, "\"status\":");
        if (status_field && status_field < entry_end) {
            sscanf(status_field + 9, "%d", &status);
        }

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
