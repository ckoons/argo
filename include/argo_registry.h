/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_REGISTRY_H
#define ARGO_REGISTRY_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

/* Registry limits */
#define REGISTRY_MAX_CIS 50
#define REGISTRY_NAME_MAX 32
#define REGISTRY_ROLE_MAX 32
#define REGISTRY_MODEL_MAX 64
#define REGISTRY_HOST_MAX 128

/* Registry message buffer sizes */
#define MESSAGE_JSON_BUFFER_SIZE 8192

/* Registry message JSON field names */
#define REGISTRY_JSON_TIMESTAMP "\"timestamp\":"
#define REGISTRY_JSON_TIMEOUT "\"timeout_ms\":"

/* CI status */
typedef enum ci_status {
    CI_STATUS_OFFLINE,          /* Not running */
    CI_STATUS_STARTING,         /* Initializing */
    CI_STATUS_READY,           /* Available for work */
    CI_STATUS_BUSY,            /* Processing request */
    CI_STATUS_ERROR,           /* In error state */
    CI_STATUS_SHUTDOWN         /* Shutting down */
} ci_status_t;

/* Port configuration from .env.argo_local */
typedef struct port_config {
    int base_port;             /* ARGO_CI_BASE_PORT */
    int port_max;              /* ARGO_CI_PORT_MAX */
    int builder_offset;        /* 0-9 */
    int coordinator_offset;    /* 10-19 */
    int requirements_offset;   /* 20-29 */
    int analysis_offset;       /* 30-39 */
    int reserved_offset;       /* 40-49 */
} port_config_t;

/* Registry entry for a CI */
typedef struct ci_registry_entry {
    /* Identity */
    char name[REGISTRY_NAME_MAX];       /* "Argo", "Maia", etc */
    char role[REGISTRY_ROLE_MAX];       /* "builder", "requirements" */
    char model[REGISTRY_MODEL_MAX];     /* "llama3:70b", "claude" */

    /* Network */
    char host[REGISTRY_HOST_MAX];       /* Usually "localhost" */
    int port;                            /* Socket port number */
    int socket_fd;                       /* Active socket or -1 */

    /* Status */
    ci_status_t status;
    time_t last_heartbeat;
    time_t registered_at;

    /* Capabilities */
    size_t context_size;                 /* Model's context limit */
    bool supports_streaming;
    bool supports_memory;

    /* Statistics */
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t errors_count;
    time_t last_error;

    /* Next in list */
    struct ci_registry_entry* next;
} ci_registry_entry_t;

/* Message for CI communication */
typedef struct ci_message {
    char from[REGISTRY_NAME_MAX];
    char to[REGISTRY_NAME_MAX];
    time_t timestamp;
    char* type;                          /* request|response|broadcast|negotiation */
    char* thread_id;                     /* Optional thread identifier */
    char* content;
    struct {
        char* priority;                  /* normal|high|low */
        int timeout_ms;
    } metadata;
} ci_message_t;

/* Registry management */
typedef struct ci_registry {
    ci_registry_entry_t* entries;
    int count;
    port_config_t port_config;
    bool initialized;
} ci_registry_t;

/* Registry initialization and cleanup */
ci_registry_t* registry_create(void);
void registry_destroy(ci_registry_t* registry);
int registry_load_config(ci_registry_t* registry, const char* config_path);

/* CI registration */
int registry_add_ci(ci_registry_t* registry,
                   const char* name,
                   const char* role,
                   const char* model,
                   int port);
int registry_remove_ci(ci_registry_t* registry, const char* name);
ci_registry_entry_t* registry_find_ci(ci_registry_t* registry,
                                      const char* name);

/* Port management */
int registry_allocate_port(ci_registry_t* registry, const char* role);
int registry_get_port_for_role(ci_registry_t* registry,
                              const char* role,
                              int instance);
bool registry_is_port_available(ci_registry_t* registry, int port);

/* CI discovery */
ci_registry_entry_t* registry_find_by_role(ci_registry_t* registry,
                                          const char* role);
ci_registry_entry_t** registry_find_all_by_role(ci_registry_t* registry,
                                               const char* role,
                                               int* count);
ci_registry_entry_t* registry_find_available(ci_registry_t* registry,
                                            const char* role);

/* Status management */
int registry_update_status(ci_registry_t* registry,
                          const char* name,
                          ci_status_t status);
int registry_heartbeat(ci_registry_t* registry, const char* name);
int registry_check_health(ci_registry_t* registry);

/* Message lifecycle */
void message_free(ci_message_t* msg);

/* Message passing */
int registry_send_message(ci_registry_t* registry,
                         const char* from_ci,
                         const char* to_ci,
                         const char* message_json);
int registry_broadcast_message(ci_registry_t* registry,
                              const char* from_ci,
                              const char* role_filter,
                              const char* message_json);

/* Message creation and parsing */
ci_message_t* message_create(const char* from,
                            const char* to,
                            const char* type,
                            const char* content);
void message_destroy(ci_message_t* message);
char* message_to_json(ci_message_t* message);
ci_message_t* message_from_json(const char* json);

/* Socket operations */
int registry_connect_ci(ci_registry_t* registry, const char* name);
int registry_disconnect_ci(ci_registry_t* registry, const char* name);
bool registry_is_connected(ci_registry_t* registry, const char* name);

/* Utility functions */
void registry_print_status(ci_registry_t* registry);
void registry_print_entry(ci_registry_entry_t* entry);
int registry_save_state(ci_registry_t* registry, const char* filepath);
int registry_load_state(ci_registry_t* registry, const char* filepath);

/* Statistics */
typedef struct registry_stats {
    int total_cis;
    int online_cis;
    int busy_cis;
    uint64_t total_messages;
    uint64_t total_errors;
    time_t uptime;
} registry_stats_t;

registry_stats_t* registry_get_stats(ci_registry_t* registry);
void registry_free_stats(registry_stats_t* stats);

#endif /* ARGO_REGISTRY_H */