/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_PROVIDER_H
#define ARGO_PROVIDER_H

#include <stddef.h>
#include <stdbool.h>
#include "argo_ci.h"
#include "argo_registry.h"

/* Provider types */
typedef enum {
    PROVIDER_TYPE_LOCAL,      /* Local service (Ollama) */
    PROVIDER_TYPE_CLI,         /* Command-line interface (Claude Code) */
    PROVIDER_TYPE_API          /* REST API (Claude, OpenAI, Gemini, etc.) */
} provider_type_t;

/* Provider status */
typedef enum {
    PROVIDER_STATUS_UNKNOWN,
    PROVIDER_STATUS_AVAILABLE,
    PROVIDER_STATUS_UNAVAILABLE,
    PROVIDER_STATUS_ERROR
} provider_status_t;

/* Provider registry entry */
typedef struct provider_entry {
    ci_provider_t* provider;
    provider_type_t type;
    provider_status_t status;
    time_t last_check;
    int error_count;
    bool requires_activation;     /* True for paid APIs */
    bool activated;               /* User explicitly activated */
    struct provider_entry* next;
} provider_entry_t;

/* Provider registry */
typedef struct {
    provider_entry_t* entries;
    int count;
    int available_count;
    char* default_provider_name;
    ci_provider_t* default_provider;
} provider_registry_t;

/* JSON message types for CI communication */
#define MSG_TYPE_TASK_REQUEST    "task_request"
#define MSG_TYPE_TASK_RESPONSE   "task_response"
#define MSG_TYPE_STATUS_UPDATE   "status_update"
#define MSG_TYPE_HEARTBEAT       "heartbeat"
#define MSG_TYPE_ERROR           "error"
#define MSG_TYPE_MEMORY_DIGEST   "memory_digest"
#define MSG_TYPE_SUNSET          "sunset"
#define MSG_TYPE_SUNRISE         "sunrise"

/* Message structure (JSON format) */
typedef struct {
    char* type;
    char* ci_name;
    char* content;
    char* context;       /* Optional context/memory */
    time_t timestamp;
    int sequence;        /* Message sequence number */
} provider_message_t;

/* Provider registry management */
provider_registry_t* provider_registry_create(void);
void provider_registry_destroy(provider_registry_t* registry);

/* Provider registration */
int provider_registry_add(provider_registry_t* registry,
                         ci_provider_t* provider,
                         provider_type_t type,
                         bool requires_activation);

/* Provider discovery */
int provider_registry_discover_all(provider_registry_t* registry);
int provider_registry_check_availability(provider_registry_t* registry,
                                        const char* provider_name);
provider_entry_t* provider_registry_find(provider_registry_t* registry,
                                        const char* provider_name);
provider_entry_t** provider_registry_find_available(provider_registry_t* registry,
                                                   int* count);

/* Provider selection */
int provider_registry_set_default(provider_registry_t* registry,
                                 const char* provider_name);
ci_provider_t* provider_registry_get_default(provider_registry_t* registry);
ci_provider_t* provider_registry_get(provider_registry_t* registry,
                                    const char* provider_name);

/* Provider activation (for paid APIs) */
int provider_registry_activate(provider_registry_t* registry,
                              const char* provider_name);
int provider_registry_deactivate(provider_registry_t* registry,
                                const char* provider_name);
bool provider_registry_is_activated(provider_registry_t* registry,
                                   const char* provider_name);

/* CI assignment */
int provider_assign_ci(provider_registry_t* provider_reg,
                      ci_registry_t* ci_reg,
                      const char* ci_name,
                      const char* provider_name);
int provider_assign_ci_with_model(provider_registry_t* provider_reg,
                                  ci_registry_t* ci_reg,
                                  const char* ci_name,
                                  const char* provider_name,
                                  const char* model);

/* Message handling */
provider_message_t* provider_message_create(const char* type,
                                           const char* ci_name,
                                           const char* content);
void provider_message_destroy(provider_message_t* message);
char* provider_message_to_json(const provider_message_t* message);
provider_message_t* provider_message_from_json(const char* json);

/* Status and utilities */
void provider_registry_print_status(provider_registry_t* registry);
void provider_entry_print(const provider_entry_t* entry);
int provider_registry_health_check(provider_registry_t* registry);

#endif /* ARGO_PROVIDER_H */
