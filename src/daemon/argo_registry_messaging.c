/* © 2025 Casey Koons All rights reserved */

/* Registry messaging - message creation, parsing, sending, and broadcasting */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

/* Project includes */
#include "argo_registry.h"
#include "argo_json.h"
#include "argo_socket.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"

/* Free message memory */
void message_free(ci_message_t* msg) {
    if (!msg) return;

    free(msg->type);
    free(msg->thread_id);
    free(msg->content);

    /* Clear pointers to prevent double-free */
    msg->type = NULL;
    msg->thread_id = NULL;
    msg->content = NULL;
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
        argo_report_error(E_CI_NO_PROVIDER, "registry_send_message", ERR_MSG_CI_NOT_FOUND);
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
        argo_report_error(E_PROTOCOL_FORMAT, "registry_send_message", ERR_MSG_INVALID_MESSAGE);
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
        argo_report_error(result, "registry_send_message", ERR_FMT_FROM_TO, from_ci, to_ci);
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

/* Create message */
ci_message_t* message_create(const char* from, const char* to,
                            const char* type, const char* content) {
    ci_message_t* msg = NULL;

    if (!from || !to || !type || !content) return NULL;

    msg = calloc(1, sizeof(ci_message_t));
    if (!msg) return NULL;

    strncpy(msg->from, from, REGISTRY_NAME_MAX - 1);
    strncpy(msg->to, to, REGISTRY_NAME_MAX - 1);
    msg->timestamp = time(NULL);

    msg->type = strdup(type);
    if (!msg->type) goto cleanup;

    msg->content = strdup(content);
    if (!msg->content) goto cleanup;

    return msg;

cleanup:
    if (msg) {
        free(msg->type);
        free(msg->content);
        free(msg);
    }
    return NULL;
}

/* Destroy message */
void message_destroy(ci_message_t* message) {
    if (!message) return;
    free(message->type);
    free(message->thread_id);
    free(message->content);
    free(message->metadata.priority);
    free(message);
}

/* Convert message to JSON */
char* message_to_json(ci_message_t* message) {
    if (!message) return NULL;

    char* json = malloc(MESSAGE_JSON_BUFFER_SIZE);
    if (!json) return NULL;

    /* Build base message */
    int len = snprintf(json, MESSAGE_JSON_BUFFER_SIZE,
            "{\"from\":\"%s\",\"to\":\"%s\",\"timestamp\":%ld,"
            "\"type\":\"%s\",\"content\":\"%s\"",
            message->from, message->to, (long)message->timestamp,
            message->type, message->content ? message->content : "");

    /* Add optional thread_id */
    if (message->thread_id) {
        len += snprintf(json + len, MESSAGE_JSON_BUFFER_SIZE - len,
                       ",\"thread_id\":\"%s\"", message->thread_id);
    }

    /* Add metadata if present */
    if (message->metadata.priority || message->metadata.timeout_ms > 0) {
        len += snprintf(json + len, MESSAGE_JSON_BUFFER_SIZE - len, ",\"metadata\":{");

        int metadata_added = 0;
        if (message->metadata.priority) {
            len += snprintf(json + len, MESSAGE_JSON_BUFFER_SIZE - len,
                           "\"priority\":\"%s\"", message->metadata.priority);
            metadata_added = 1;
        }

        if (message->metadata.timeout_ms > 0) {
            if (metadata_added) {
                len += snprintf(json + len, MESSAGE_JSON_BUFFER_SIZE - len, ",");
            }
            len += snprintf(json + len, MESSAGE_JSON_BUFFER_SIZE - len,
                           "\"timeout_ms\":%d", message->metadata.timeout_ms);
        }

        len += snprintf(json + len, MESSAGE_JSON_BUFFER_SIZE - len, "}");
    }

    /* Close JSON object */
    snprintf(json + len, MESSAGE_JSON_BUFFER_SIZE - len, "}");

    return json;
}

/* Parse message from JSON */
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
    char* ts_start = strstr(json, REGISTRY_JSON_TIMESTAMP);
    if (ts_start) {
        char* endptr = NULL;
        long ts = strtol(ts_start + strlen(REGISTRY_JSON_TIMESTAMP), &endptr, 10);
        if (endptr != ts_start + strlen(REGISTRY_JSON_TIMESTAMP) && ts >= 0) {
            msg->timestamp = (time_t)ts;
        } else {
            msg->timestamp = time(NULL);
        }
    } else {
        msg->timestamp = time(NULL);
    }

    /* Extract timeout_ms */
    char* timeout_start = strstr(json, REGISTRY_JSON_TIMEOUT);
    if (timeout_start) {
        char* endptr = NULL;
        long timeout = strtol(timeout_start + strlen(REGISTRY_JSON_TIMEOUT), &endptr, 10);
        if (endptr != timeout_start + strlen(REGISTRY_JSON_TIMEOUT) && timeout >= 0 && timeout <= INT_MAX) {
            msg->metadata.timeout_ms = (int)timeout;
        }
    }

    return msg;
}
