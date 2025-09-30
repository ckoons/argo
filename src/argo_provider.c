/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_provider.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_ollama.h"
#include "argo_claude.h"
#include "argo_api_providers.h"

/* Create provider registry */
provider_registry_t* provider_registry_create(void) {
    provider_registry_t* registry = calloc(1, sizeof(provider_registry_t));
    if (!registry) {
        LOG_ERROR("Failed to allocate provider registry");
        return NULL;
    }

    LOG_INFO("Provider registry created");
    return registry;
}

/* Destroy provider registry */
void provider_registry_destroy(provider_registry_t* registry) {
    if (!registry) return;

    provider_entry_t* entry = registry->entries;
    while (entry) {
        provider_entry_t* next = entry->next;

        /* Cleanup provider */
        if (entry->provider && entry->provider->cleanup) {
            entry->provider->cleanup(entry->provider);
        }

        free(entry);
        entry = next;
    }

    free(registry->default_provider_name);
    free(registry);
}

/* Add provider to registry */
int provider_registry_add(provider_registry_t* registry,
                         ci_provider_t* provider,
                         provider_type_t type,
                         bool requires_activation) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(provider);

    /* Create entry */
    provider_entry_t* entry = calloc(1, sizeof(provider_entry_t));
    if (!entry) {
        return E_SYSTEM_MEMORY;
    }

    entry->provider = provider;
    entry->type = type;
    entry->status = PROVIDER_STATUS_UNKNOWN;
    entry->requires_activation = requires_activation;
    entry->activated = false;
    entry->last_check = 0;
    entry->error_count = 0;

    /* Add to list */
    entry->next = registry->entries;
    registry->entries = entry;
    registry->count++;

    LOG_INFO("Added provider: %s (type=%d, requires_activation=%d)",
            provider->name, type, requires_activation);
    return ARGO_SUCCESS;
}

/* Check if a specific provider is available */
int provider_registry_check_availability(provider_registry_t* registry,
                                        const char* provider_name) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(provider_name);

    provider_entry_t* entry = provider_registry_find(registry, provider_name);
    if (!entry) {
        return E_INPUT_INVALID;
    }

    /* If requires activation and not activated, mark unavailable */
    if (entry->requires_activation && !entry->activated) {
        entry->status = PROVIDER_STATUS_UNAVAILABLE;
        entry->last_check = time(NULL);
        return ARGO_SUCCESS;
    }

    /* Try to initialize and connect */
    ci_provider_t* provider = entry->provider;

    int result = provider->init(provider);
    if (result != ARGO_SUCCESS) {
        entry->status = PROVIDER_STATUS_ERROR;
        entry->error_count++;
        entry->last_check = time(NULL);
        LOG_WARN("Provider %s init failed: %d", provider_name, result);
        return result;
    }

    result = provider->connect(provider);
    if (result != ARGO_SUCCESS) {
        entry->status = PROVIDER_STATUS_UNAVAILABLE;
        entry->last_check = time(NULL);
        LOG_INFO("Provider %s unavailable", provider_name);
        return ARGO_SUCCESS;
    }

    /* Provider is available */
    entry->status = PROVIDER_STATUS_AVAILABLE;
    entry->last_check = time(NULL);
    entry->error_count = 0;
    LOG_INFO("Provider %s is available", provider_name);

    return ARGO_SUCCESS;
}

/* Discover all providers */
int provider_registry_discover_all(provider_registry_t* registry) {
    ARGO_CHECK_NULL(registry);

    LOG_INFO("Starting provider discovery...");

    int available = 0;
    provider_entry_t* entry = registry->entries;

    while (entry) {
        provider_registry_check_availability(registry, entry->provider->name);
        if (entry->status == PROVIDER_STATUS_AVAILABLE) {
            available++;
        }
        entry = entry->next;
    }

    registry->available_count = available;
    LOG_INFO("Provider discovery complete: %d/%d available", available, registry->count);

    return ARGO_SUCCESS;
}

/* Find provider by name */
provider_entry_t* provider_registry_find(provider_registry_t* registry,
                                        const char* provider_name) {
    if (!registry || !provider_name) return NULL;

    provider_entry_t* entry = registry->entries;
    while (entry) {
        if (strcmp(entry->provider->name, provider_name) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

/* Find all available providers */
provider_entry_t** provider_registry_find_available(provider_registry_t* registry,
                                                   int* count) {
    if (!registry || !count) return NULL;

    /* Count available */
    int available = 0;
    provider_entry_t* entry = registry->entries;
    while (entry) {
        if (entry->status == PROVIDER_STATUS_AVAILABLE) {
            available++;
        }
        entry = entry->next;
    }

    if (available == 0) {
        *count = 0;
        return NULL;
    }

    /* Allocate array */
    provider_entry_t** result = malloc(available * sizeof(provider_entry_t*));
    if (!result) {
        *count = 0;
        return NULL;
    }

    /* Fill array */
    int index = 0;
    entry = registry->entries;
    while (entry) {
        if (entry->status == PROVIDER_STATUS_AVAILABLE) {
            result[index++] = entry;
        }
        entry = entry->next;
    }

    *count = available;
    return result;
}

/* Set default provider */
int provider_registry_set_default(provider_registry_t* registry,
                                 const char* provider_name) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(provider_name);

    provider_entry_t* entry = provider_registry_find(registry, provider_name);
    if (!entry) {
        LOG_ERROR("Provider %s not found", provider_name);
        return E_INPUT_INVALID;
    }

    if (entry->status != PROVIDER_STATUS_AVAILABLE) {
        LOG_ERROR("Provider %s not available", provider_name);
        return E_CI_NO_PROVIDER;
    }

    /* Set default */
    free(registry->default_provider_name);
    registry->default_provider_name = strdup(provider_name);
    registry->default_provider = entry->provider;

    LOG_INFO("Set default provider: %s", provider_name);
    return ARGO_SUCCESS;
}

/* Get default provider */
ci_provider_t* provider_registry_get_default(provider_registry_t* registry) {
    if (!registry) return NULL;
    return registry->default_provider;
}

/* Get provider by name */
ci_provider_t* provider_registry_get(provider_registry_t* registry,
                                    const char* provider_name) {
    provider_entry_t* entry = provider_registry_find(registry, provider_name);
    if (!entry) return NULL;
    return entry->provider;
}

/* Activate provider */
int provider_registry_activate(provider_registry_t* registry,
                              const char* provider_name) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(provider_name);

    provider_entry_t* entry = provider_registry_find(registry, provider_name);
    if (!entry) {
        return E_INPUT_INVALID;
    }

    if (!entry->requires_activation) {
        LOG_WARN("Provider %s doesn't require activation", provider_name);
        return ARGO_SUCCESS;
    }

    entry->activated = true;
    LOG_INFO("Activated provider: %s", provider_name);

    /* Re-check availability now that it's activated */
    return provider_registry_check_availability(registry, provider_name);
}

/* Deactivate provider */
int provider_registry_deactivate(provider_registry_t* registry,
                                const char* provider_name) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(provider_name);

    provider_entry_t* entry = provider_registry_find(registry, provider_name);
    if (!entry) {
        return E_INPUT_INVALID;
    }

    entry->activated = false;
    entry->status = PROVIDER_STATUS_UNAVAILABLE;

    LOG_INFO("Deactivated provider: %s", provider_name);
    return ARGO_SUCCESS;
}

/* Check if provider is activated */
bool provider_registry_is_activated(provider_registry_t* registry,
                                   const char* provider_name) {
    provider_entry_t* entry = provider_registry_find(registry, provider_name);
    if (!entry) return false;
    return entry->activated;
}

/* Assign provider to CI */
int provider_assign_ci(provider_registry_t* provider_reg,
                      ci_registry_t* ci_reg,
                      const char* ci_name,
                      const char* provider_name) {
    ARGO_CHECK_NULL(provider_reg);
    ARGO_CHECK_NULL(ci_reg);
    ARGO_CHECK_NULL(ci_name);
    ARGO_CHECK_NULL(provider_name);

    /* Find CI */
    ci_registry_entry_t* ci = registry_find_ci(ci_reg, ci_name);
    if (!ci) {
        LOG_ERROR("CI %s not found", ci_name);
        return E_INPUT_INVALID;
    }

    /* Find provider */
    provider_entry_t* entry = provider_registry_find(provider_reg, provider_name);
    if (!entry) {
        LOG_ERROR("Provider %s not found", provider_name);
        return E_INPUT_INVALID;
    }

    if (entry->status != PROVIDER_STATUS_AVAILABLE) {
        LOG_ERROR("Provider %s not available", provider_name);
        return E_CI_NO_PROVIDER;
    }

    /* Update CI's model to provider's model */
    strncpy(ci->model, entry->provider->model, REGISTRY_MODEL_MAX - 1);

    LOG_INFO("Assigned provider %s to CI %s", provider_name, ci_name);
    return ARGO_SUCCESS;
}

/* Assign provider with specific model */
int provider_assign_ci_with_model(provider_registry_t* provider_reg,
                                  ci_registry_t* ci_reg,
                                  const char* ci_name,
                                  const char* provider_name,
                                  const char* model) {
    ARGO_CHECK_NULL(model);

    /* First do basic assignment */
    int result = provider_assign_ci(provider_reg, ci_reg, ci_name, provider_name);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Update model */
    ci_registry_entry_t* ci = registry_find_ci(ci_reg, ci_name);
    strncpy(ci->model, model, REGISTRY_MODEL_MAX - 1);

    LOG_INFO("Assigned provider %s (model %s) to CI %s", provider_name, model, ci_name);
    return ARGO_SUCCESS;
}

/* Create message */
provider_message_t* provider_message_create(const char* type,
                                           const char* ci_name,
                                           const char* content) {
    provider_message_t* msg = calloc(1, sizeof(provider_message_t));
    if (!msg) return NULL;

    msg->type = type ? strdup(type) : NULL;
    msg->ci_name = ci_name ? strdup(ci_name) : NULL;
    msg->content = content ? strdup(content) : NULL;
    msg->timestamp = time(NULL);
    msg->sequence = 0;

    return msg;
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
    if (!json) return NULL;

    provider_message_t* msg = calloc(1, sizeof(provider_message_t));
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
}

/* Print status */
void provider_registry_print_status(provider_registry_t* registry) {
    if (!registry) return;

    printf("\nProvider Registry Status\n");
    printf("===============================================\n");
    printf("Total providers: %d\n", registry->count);
    printf("Available: %d\n", registry->available_count);
    if (registry->default_provider_name) {
        printf("Default: %s\n", registry->default_provider_name);
    }
    printf("-----------------------------------------------\n");

    provider_entry_t* entry = registry->entries;
    while (entry) {
        provider_entry_print(entry);
        entry = entry->next;
    }
}

/* Print entry */
void provider_entry_print(const provider_entry_t* entry) {
    if (!entry || !entry->provider) return;

    const char* status_str[] = { "UNKNOWN", "AVAILABLE", "UNAVAILABLE", "ERROR" };
    const char* type_str[] = { "LOCAL", "CLI", "API" };

    printf("  %s (%s) - %s\n",
           entry->provider->name,
           type_str[entry->type],
           status_str[entry->status]);

    printf("    Model: %s\n", entry->provider->model);
    printf("    Streaming: %s\n", entry->provider->supports_streaming ? "yes" : "no");

    if (entry->requires_activation) {
        printf("    Requires activation: %s\n",
               entry->activated ? "ACTIVATED" : "NOT ACTIVATED");
    }

    if (entry->error_count > 0) {
        printf("    Errors: %d\n", entry->error_count);
    }
}

/* Health check */
int provider_registry_health_check(provider_registry_t* registry) {
    ARGO_CHECK_NULL(registry);

    int unhealthy = 0;

    provider_entry_t* entry = registry->entries;
    while (entry) {
        if (entry->status == PROVIDER_STATUS_ERROR) {
            unhealthy++;
        }
        entry = entry->next;
    }

    return unhealthy;
}
