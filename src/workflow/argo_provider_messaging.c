/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_provider.h"
#include "argo_limits.h"

/* Create message */
provider_message_t* provider_message_create(const char* type,
                                           const char* ci_name,
                                           const char* content) {
    provider_message_t* msg = calloc(1, sizeof(provider_message_t));
    if (!msg) return NULL;

    if (type) {
        msg->type = strdup(type);
        if (!msg->type) goto cleanup;
    }

    if (ci_name) {
        msg->ci_name = strdup(ci_name);
        if (!msg->ci_name) goto cleanup;
    }

    if (content) {
        msg->content = strdup(content);
        if (!msg->content) goto cleanup;
    }

    msg->timestamp = time(NULL);
    msg->sequence = 0;

    return msg;

cleanup:
    free(msg->type);
    free(msg->ci_name);
    free(msg->content);
    free(msg);
    return NULL;
}

/* Destroy message */
void provider_message_destroy(provider_message_t* message) {
    if (!message) return;

    free(message->type);
    free(message->ci_name);
    free(message->content);
    free(message->context);
    free(message);
}

/* Convert message to JSON */
char* provider_message_to_json(const provider_message_t* message) {
    if (!message) return NULL;

    /* Allocate buffer */
    char* json = malloc(PROVIDER_MESSAGE_BUFFER_SIZE);
    if (!json) return NULL;

    int len = snprintf(json, PROVIDER_MESSAGE_BUFFER_SIZE,
        "{"
        "\"type\":\"%s\","
        "\"ci_name\":\"%s\","
        "\"content\":\"%s\","
        "\"timestamp\":%ld,"
        "\"sequence\":%d",
        message->type ? message->type : "",
        message->ci_name ? message->ci_name : "",
        message->content ? message->content : "",
        (long)message->timestamp,
        message->sequence);

    /* Add context if present */
    if (message->context) {
        len += snprintf(json + len, PROVIDER_MESSAGE_BUFFER_SIZE - len,
            ",\"context\":\"%s\"", message->context);
    }

    snprintf(json + len, PROVIDER_MESSAGE_BUFFER_SIZE - len, "}");

    return json;
}

/* Parse message from JSON */
provider_message_t* provider_message_from_json(const char* json) {
    provider_message_t* msg = NULL;

    if (!json) return NULL;

    msg = calloc(1, sizeof(provider_message_t));
    if (!msg) return NULL;

    /* Simple extraction - find fields */
    char buffer[PROVIDER_MESSAGE_FIELD_SIZE];

    /* Type */
    char* type_start = strstr(json, "\"type\":\"");
    if (type_start) {
        type_start += PROVIDER_JSON_FIELD_OFFSET_TYPE;
        char* type_end = strchr(type_start, '"');
        if (type_end) {
            size_t len = type_end - type_start;
            if (len < sizeof(buffer)) {
                strncpy(buffer, type_start, len);
                buffer[len] = '\0';
                msg->type = strdup(buffer);
                if (!msg->type) goto cleanup;
            }
        }
    }

    /* CI name */
    char* ci_start = strstr(json, "\"ci_name\":\"");
    if (ci_start) {
        ci_start += PROVIDER_JSON_FIELD_OFFSET_CI_NAME;
        char* ci_end = strchr(ci_start, '"');
        if (ci_end) {
            size_t len = ci_end - ci_start;
            if (len < sizeof(buffer)) {
                strncpy(buffer, ci_start, len);
                buffer[len] = '\0';
                msg->ci_name = strdup(buffer);
                if (!msg->ci_name) goto cleanup;
            }
        }
    }

    /* Content */
    char* content_start = strstr(json, "\"content\":\"");
    if (content_start) {
        content_start += PROVIDER_JSON_FIELD_OFFSET_CONTENT;
        char* content_end = strchr(content_start, '"');
        if (content_end) {
            size_t len = content_end - content_start;
            if (len < sizeof(buffer)) {
                strncpy(buffer, content_start, len);
                buffer[len] = '\0';
                msg->content = strdup(buffer);
                if (!msg->content) goto cleanup;
            }
        }
    }

    /* Timestamp */
    char* ts_start = strstr(json, "\"timestamp\":");
    if (ts_start) {
        msg->timestamp = atol(ts_start + PROVIDER_JSON_FIELD_OFFSET_TIMESTAMP);
    }

    /* Sequence */
    char* seq_start = strstr(json, "\"sequence\":");
    if (seq_start) {
        msg->sequence = atoi(seq_start + PROVIDER_JSON_FIELD_OFFSET_SEQUENCE);
    }

    return msg;

cleanup:
    if (msg) {
        free(msg->type);
        free(msg->ci_name);
        free(msg->content);
        free(msg);
    }
    return NULL;
}
