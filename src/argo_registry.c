/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Project includes */
#include "argo_registry.h"
#include "argo_json.h"
#include "argo_socket.h"
#include "argo_error.h"
#include "argo_log.h"

/* Port configuration defaults */
#define DEFAULT_BASE_PORT 9000
#define DEFAULT_PORT_MAX 50
#define BUILDER_OFFSET 0
#define COORDINATOR_OFFSET 10
#define REQUIREMENTS_OFFSET 20
#define ANALYSIS_OFFSET 30
#define RESERVED_OFFSET 40

/* Create registry */
ci_registry_t* registry_create(void) {
    ci_registry_t* registry = calloc(1, sizeof(ci_registry_t));
    if (!registry) {
        argo_report_error(E_SYSTEM_MEMORY, "registry_create",
                         "Failed to allocate registry");
        return NULL;
    }

    registry->entries = NULL;
    registry->count = 0;
    registry->initialized = true;

    /* Set default port configuration */
    registry->port_config.base_port = DEFAULT_BASE_PORT;
    registry->port_config.port_max = DEFAULT_PORT_MAX;
    registry->port_config.builder_offset = BUILDER_OFFSET;
    registry->port_config.coordinator_offset = COORDINATOR_OFFSET;
    registry->port_config.requirements_offset = REQUIREMENTS_OFFSET;
    registry->port_config.analysis_offset = ANALYSIS_OFFSET;
    registry->port_config.reserved_offset = RESERVED_OFFSET;

    LOG_INFO("Registry created with base port %d", DEFAULT_BASE_PORT);
    return registry;
}

/* Destroy registry */
void registry_destroy(ci_registry_t* registry) {
    if (!registry) return;

    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        ci_registry_entry_t* next = entry->next;
        free(entry);
        entry = next;
    }

    LOG_INFO("Registry destroyed, had %d CIs", registry->count);
    free(registry);
}

/* Load configuration (stub for now) */
int registry_load_config(ci_registry_t* registry, const char* config_path) {
    if (!registry) return E_INPUT_NULL;
    (void)config_path;
    return ARGO_SUCCESS;
}

/* Add CI to registry */
int registry_add_ci(ci_registry_t* registry,
                   const char* name,
                   const char* role,
                   const char* model,
                   int port) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(name);
    ARGO_CHECK_NULL(role);
    ARGO_CHECK_NULL(model);

    if (registry->count >= REGISTRY_MAX_CIS) {
        argo_report_error(E_PROTOCOL_QUEUE, "registry_add_ci", name);
        return E_PROTOCOL_QUEUE;
    }

    /* Check if already exists */
    if (registry_find_ci(registry, name)) {
        argo_report_error(E_INPUT_INVALID, "registry_add_ci", name);
        return E_INPUT_INVALID;
    }

    /* Create entry */
    ci_registry_entry_t* entry = calloc(1, sizeof(ci_registry_entry_t));
    if (!entry) {
        return E_SYSTEM_MEMORY;
    }

    strncpy(entry->name, name, REGISTRY_NAME_MAX - 1);
    strncpy(entry->role, role, REGISTRY_ROLE_MAX - 1);
    strncpy(entry->model, model, REGISTRY_MODEL_MAX - 1);
    strncpy(entry->host, "localhost", REGISTRY_HOST_MAX - 1);
    entry->port = port;
    entry->socket_fd = -1;
    entry->status = CI_STATUS_OFFLINE;
    entry->registered_at = time(NULL);
    entry->last_heartbeat = time(NULL);

    /* Add to list */
    entry->next = registry->entries;
    registry->entries = entry;
    registry->count++;

    LOG_INFO("Registered CI: %s (role=%s, model=%s, port=%d)",
             name, role, model, port);

    return ARGO_SUCCESS;
}

/* Remove CI from registry */
int registry_remove_ci(ci_registry_t* registry, const char* name) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(name);

    ci_registry_entry_t** pp = &registry->entries;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            ci_registry_entry_t* entry = *pp;
            *pp = entry->next;
            registry->count--;
            LOG_INFO("Unregistered CI: %s", name);
            free(entry);
            return ARGO_SUCCESS;
        }
        pp = &(*pp)->next;
    }

    argo_report_error(E_INPUT_INVALID, "registry_remove_ci", name);
    return E_INPUT_INVALID;
}

/* Find CI by name */
ci_registry_entry_t* registry_find_ci(ci_registry_t* registry,
                                      const char* name) {
    if (!registry || !name) return NULL;

    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/* Get port offset for role */
static int get_role_offset(ci_registry_t* registry, const char* role) {
    if (strcmp(role, "builder") == 0) {
        return registry->port_config.builder_offset;
    } else if (strcmp(role, "coordinator") == 0) {
        return registry->port_config.coordinator_offset;
    } else if (strcmp(role, "requirements") == 0) {
        return registry->port_config.requirements_offset;
    } else if (strcmp(role, "analysis") == 0) {
        return registry->port_config.analysis_offset;
    }
    return registry->port_config.reserved_offset;
}

/* Allocate port for role */
int registry_allocate_port(ci_registry_t* registry, const char* role) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(role);

    int offset = get_role_offset(registry, role);
    int base = registry->port_config.base_port + offset;

    /* Find first available port in role's range */
    for (int i = 0; i < 10; i++) {
        int port = base + i;
        if (registry_is_port_available(registry, port)) {
            return port;
        }
    }

    argo_report_error(E_PROTOCOL_QUEUE, "registry_allocate_port", role);
    return -1;
}

/* Get port for role instance */
int registry_get_port_for_role(ci_registry_t* registry,
                              const char* role,
                              int instance) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(role);
    ARGO_CHECK_RANGE(instance, 0, 9);

    int offset = get_role_offset(registry, role);
    return registry->port_config.base_port + offset + instance;
}

/* Check if port is available */
bool registry_is_port_available(ci_registry_t* registry, int port) {
    if (!registry) return false;

    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (entry->port == port) {
            return false;
        }
        entry = entry->next;
    }
    return true;
}

/* Find CI by role (first match) */
ci_registry_entry_t* registry_find_by_role(ci_registry_t* registry,
                                          const char* role) {
    if (!registry || !role) return NULL;

    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (strcmp(entry->role, role) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/* Find all CIs by role */
ci_registry_entry_t** registry_find_all_by_role(ci_registry_t* registry,
                                               const char* role,
                                               int* count) {
    if (!registry || !role || !count) return NULL;

    /* Count matches */
    int match_count = 0;
    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (strcmp(entry->role, role) == 0) {
            match_count++;
        }
        entry = entry->next;
    }

    if (match_count == 0) {
        *count = 0;
        return NULL;
    }

    /* Allocate result array */
    ci_registry_entry_t** results = malloc(match_count * sizeof(ci_registry_entry_t*));
    if (!results) {
        *count = 0;
        return NULL;
    }

    /* Fill array */
    int idx = 0;
    entry = registry->entries;
    while (entry && idx < match_count) {
        if (strcmp(entry->role, role) == 0) {
            results[idx++] = entry;
        }
        entry = entry->next;
    }

    *count = match_count;
    return results;
}

/* Find available CI for role */
ci_registry_entry_t* registry_find_available(ci_registry_t* registry,
                                            const char* role) {
    if (!registry || !role) return NULL;

    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (strcmp(entry->role, role) == 0 &&
            entry->status == CI_STATUS_READY) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/* Update CI status */
int registry_update_status(ci_registry_t* registry,
                          const char* name,
                          ci_status_t status) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(name);

    ci_registry_entry_t* entry = registry_find_ci(registry, name);
    if (!entry) {
        return E_INPUT_INVALID;
    }

    entry->status = status;
    return ARGO_SUCCESS;
}

/* Record heartbeat */
int registry_heartbeat(ci_registry_t* registry, const char* name) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(name);

    ci_registry_entry_t* entry = registry_find_ci(registry, name);
    if (!entry) {
        return E_INPUT_INVALID;
    }

    entry->last_heartbeat = time(NULL);
    return ARGO_SUCCESS;
}

/* Check health of all CIs */
int registry_check_health(ci_registry_t* registry) {
    ARGO_CHECK_NULL(registry);

    time_t now = time(NULL);
    int stale_count = 0;

    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (entry->status != CI_STATUS_OFFLINE &&
            (now - entry->last_heartbeat) > 60) {
            LOG_WARN("CI %s heartbeat stale (%lds ago)",
                    entry->name, (long)(now - entry->last_heartbeat));
            stale_count++;
        }
        entry = entry->next;
    }

    return stale_count;
}

/* Send message */
int registry_send_message(ci_registry_t* registry,
                         const char* from_ci,
                         const char* to_ci,
                         const char* message_json) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(from_ci);
    ARGO_CHECK_NULL(to_ci);
    ARGO_CHECK_NULL(message_json);

    /* Find recipient CI */
    ci_registry_entry_t* to_entry = registry_find_ci(registry, to_ci);
    if (!to_entry) {
        argo_report_error(E_CI_NO_PROVIDER, "registry_send_message", to_ci);
        return E_CI_NO_PROVIDER;
    }

    /* Check if recipient is online */
    if (to_entry->status != CI_STATUS_READY && to_entry->status != CI_STATUS_BUSY) {
        LOG_WARN("Recipient CI %s is not ready (status: %d)", to_ci, to_entry->status);
        return E_CI_DISCONNECTED;
    }

    /* Parse the message to get full structure */
    ci_message_t* msg = message_from_json(message_json);
    if (!msg) {
        argo_report_error(E_PROTOCOL_FORMAT, "registry_send_message",
                         "Failed to parse JSON");
        return E_PROTOCOL_FORMAT;
    }

    /* Update statistics */
    ci_registry_entry_t* from_entry = registry_find_ci(registry, from_ci);
    if (from_entry) {
        from_entry->messages_sent++;
    }
    to_entry->messages_received++;

    /* Send via socket */
    int result = socket_send_message(msg, NULL, NULL);

    message_destroy(msg);

    if (result != ARGO_SUCCESS) {
        char details[128];
        snprintf(details, sizeof(details), "from %s to %s", from_ci, to_ci);
        argo_report_error(result, "registry_send_message", details);
        if (to_entry) {
            to_entry->errors_count++;
            to_entry->last_error = time(NULL);
        }
        return result;
    }

    LOG_DEBUG("Message delivered from %s to %s", from_ci, to_ci);
    return ARGO_SUCCESS;
}

/* Broadcast message */
int registry_broadcast_message(ci_registry_t* registry,
                              const char* from_ci,
                              const char* role_filter,
                              const char* message_json) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(from_ci);
    ARGO_CHECK_NULL(message_json);

    if (!registry->entries) {
        LOG_WARN("No CIs registered for broadcast");
        return ARGO_SUCCESS;
    }

    int sent_count = 0;
    int error_count = 0;

    /* Iterate through all CIs */
    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        /* Skip sender */
        if (strcmp(entry->name, from_ci) == 0) {
            entry = entry->next;
            continue;
        }

        /* Check role filter */
        if (role_filter && strcmp(entry->role, role_filter) != 0) {
            entry = entry->next;
            continue;
        }

        /* Skip offline CIs */
        if (entry->status != CI_STATUS_READY && entry->status != CI_STATUS_BUSY) {
            entry = entry->next;
            continue;
        }

        /* Send message to this CI */
        int result = registry_send_message(registry, from_ci, entry->name, message_json);
        if (result == ARGO_SUCCESS) {
            sent_count++;
        } else {
            error_count++;
            LOG_WARN("Failed to broadcast to %s", entry->name);
        }

        entry = entry->next;
    }

    LOG_DEBUG("Broadcast from %s to role '%s': sent to %d CIs, %d errors",
             from_ci, role_filter ? role_filter : "all", sent_count, error_count);

    return (sent_count > 0) ? ARGO_SUCCESS : E_CI_NO_PROVIDER;
}

/* Message creation and parsing (stubs for now) */
ci_message_t* message_create(const char* from, const char* to,
                            const char* type, const char* content) {
    if (!from || !to || !type || !content) return NULL;

    ci_message_t* msg = calloc(1, sizeof(ci_message_t));
    if (!msg) return NULL;

    strncpy(msg->from, from, REGISTRY_NAME_MAX - 1);
    strncpy(msg->to, to, REGISTRY_NAME_MAX - 1);
    msg->timestamp = time(NULL);
    msg->type = strdup(type);
    msg->content = strdup(content);

    return msg;
}

void message_destroy(ci_message_t* message) {
    if (!message) return;
    free(message->type);
    free(message->thread_id);
    free(message->content);
    free(message->metadata.priority);
    free(message);
}

char* message_to_json(ci_message_t* message) {
    if (!message) return NULL;

    char* json = malloc(8192);
    if (!json) return NULL;

    /* Build base message */
    int len = snprintf(json, 8192,
            "{\"from\":\"%s\",\"to\":\"%s\",\"timestamp\":%ld,"
            "\"type\":\"%s\",\"content\":\"%s\"",
            message->from, message->to, (long)message->timestamp,
            message->type, message->content ? message->content : "");

    /* Add optional thread_id */
    if (message->thread_id) {
        len += snprintf(json + len, 8192 - len,
                       ",\"thread_id\":\"%s\"", message->thread_id);
    }

    /* Add metadata if present */
    if (message->metadata.priority || message->metadata.timeout_ms > 0) {
        len += snprintf(json + len, 8192 - len, ",\"metadata\":{");

        int metadata_added = 0;
        if (message->metadata.priority) {
            len += snprintf(json + len, 8192 - len,
                           "\"priority\":\"%s\"", message->metadata.priority);
            metadata_added = 1;
        }

        if (message->metadata.timeout_ms > 0) {
            if (metadata_added) {
                len += snprintf(json + len, 8192 - len, ",");
            }
            len += snprintf(json + len, 8192 - len,
                           "\"timeout_ms\":%d", message->metadata.timeout_ms);
        }

        len += snprintf(json + len, 8192 - len, "}");
    }

    /* Close JSON object */
    snprintf(json + len, 8192 - len, "}");

    return json;
}

ci_message_t* message_from_json(const char* json) {
    if (!json) return NULL;

    ci_message_t* msg = calloc(1, sizeof(ci_message_t));
    if (!msg) return NULL;

    /* Extract required fields */
    char* from = NULL;
    char* to = NULL;
    char* type = NULL;
    char* content = NULL;
    size_t len = 0;

    if (json_extract_string_field(json, "from", &from, &len) != ARGO_SUCCESS) {
        free(msg);
        return NULL;
    }
    strncpy(msg->from, from, REGISTRY_NAME_MAX - 1);
    free(from);

    if (json_extract_string_field(json, "to", &to, &len) != ARGO_SUCCESS) {
        free(msg);
        return NULL;
    }
    strncpy(msg->to, to, REGISTRY_NAME_MAX - 1);
    free(to);

    if (json_extract_string_field(json, "type", &type, &len) != ARGO_SUCCESS) {
        free(msg);
        return NULL;
    }
    msg->type = type;  /* Keep allocated */

    if (json_extract_string_field(json, "content", &content, &len) != ARGO_SUCCESS) {
        free(msg->type);
        free(msg);
        return NULL;
    }
    msg->content = content;  /* Keep allocated */

    /* Extract optional fields */
    char* thread_id = NULL;
    if (json_extract_string_field(json, "thread_id", &thread_id, &len) == ARGO_SUCCESS) {
        msg->thread_id = thread_id;
    }

    char* priority = NULL;
    if (json_extract_string_field(json, "priority", &priority, &len) == ARGO_SUCCESS) {
        msg->metadata.priority = priority;
    }

    /* Extract timestamp (simple - just look for the number) */
    char* ts_start = strstr(json, "\"timestamp\":");
    if (ts_start) {
        msg->timestamp = atol(ts_start + 12);
    } else {
        msg->timestamp = time(NULL);
    }

    /* Extract timeout_ms */
    char* timeout_start = strstr(json, "\"timeout_ms\":");
    if (timeout_start) {
        msg->metadata.timeout_ms = atoi(timeout_start + 13);
    }

    return msg;
}

/* Socket operations (stubs) */
int registry_connect_ci(ci_registry_t* registry, const char* name) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(name);
    return ARGO_SUCCESS;
}

int registry_disconnect_ci(ci_registry_t* registry, const char* name) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(name);
    return ARGO_SUCCESS;
}

bool registry_is_connected(ci_registry_t* registry, const char* name) {
    if (!registry || !name) return false;

    ci_registry_entry_t* entry = registry_find_ci(registry, name);
    return entry && entry->socket_fd >= 0;
}

/* Print status */
void registry_print_status(ci_registry_t* registry) {
    if (!registry) return;

    printf("Registry Status: %d CIs\n", registry->count);
    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        registry_print_entry(entry);
        entry = entry->next;
    }
}

void registry_print_entry(ci_registry_entry_t* entry) {
    if (!entry) return;

    const char* status_names[] = {
        "OFFLINE", "STARTING", "READY", "BUSY", "ERROR", "SHUTDOWN"
    };

    printf("  %s (%s): %s on %s:%d [%s]\n",
           entry->name, entry->role, entry->model,
           entry->host, entry->port,
           status_names[entry->status]);
}

/* State save/load (stubs) */
int registry_save_state(ci_registry_t* registry, const char* filepath) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(filepath);
    return ARGO_SUCCESS;
}

int registry_load_state(ci_registry_t* registry, const char* filepath) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(filepath);
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