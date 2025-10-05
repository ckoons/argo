/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_LIFECYCLE_H
#define ARGO_LIFECYCLE_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "argo_registry.h"

/* Lifecycle buffer sizes */
#define LIFECYCLE_TIME_BUFFER_SIZE 32

/* Lifecycle configuration */
#define DEFAULT_HEARTBEAT_TIMEOUT 60
#define DEFAULT_MAX_MISSED 3
#define INITIAL_CAPACITY 16

/* Lifecycle events */
typedef enum {
    LIFECYCLE_EVENT_CREATED,       /* CI instance created */
    LIFECYCLE_EVENT_INITIALIZING,  /* CI starting up */
    LIFECYCLE_EVENT_READY,          /* CI ready for work */
    LIFECYCLE_EVENT_TASK_ASSIGNED,  /* Task given to CI */
    LIFECYCLE_EVENT_TASK_COMPLETE,  /* Task finished */
    LIFECYCLE_EVENT_ERROR,          /* Error occurred */
    LIFECYCLE_EVENT_SHUTDOWN_REQ,   /* Shutdown requested */
    LIFECYCLE_EVENT_SHUTDOWN,       /* CI shutting down */
    LIFECYCLE_EVENT_TERMINATED      /* CI terminated */
} lifecycle_event_t;

/* Lifecycle transition */
typedef struct lifecycle_transition {
    time_t timestamp;
    ci_status_t from_status;
    ci_status_t to_status;
    lifecycle_event_t event;
    char* reason;                   /* Optional reason/description */
    struct lifecycle_transition* next;
} lifecycle_transition_t;

/* CI lifecycle state */
typedef struct {
    char ci_name[REGISTRY_NAME_MAX];
    ci_status_t current_status;
    time_t created;
    time_t last_transition;
    int transition_count;
    lifecycle_transition_t* transitions;  /* History */

    /* Heartbeat tracking */
    time_t last_heartbeat;
    int missed_heartbeats;
    int heartbeat_interval_seconds;

    /* Task tracking */
    char* current_task;
    time_t task_start_time;

    /* Error tracking */
    int error_count;
    char* last_error;
} ci_lifecycle_t;

/* Lifecycle manager */
typedef struct lifecycle_manager {
    ci_lifecycle_t** cis;           /* Array of CI lifecycles */
    int count;
    int capacity;
    ci_registry_t* registry;        /* Link to registry */

    /* Configuration */
    int heartbeat_timeout_seconds;
    int max_missed_heartbeats;
    bool auto_restart_on_error;
} lifecycle_manager_t;

/* Lifecycle management functions */
lifecycle_manager_t* lifecycle_manager_create(ci_registry_t* registry);
void lifecycle_manager_destroy(lifecycle_manager_t* manager);

/* CI lifecycle operations */
int lifecycle_create_ci(lifecycle_manager_t* manager,
                       const char* ci_name,
                       const char* role,
                       const char* model);
int lifecycle_start_ci(lifecycle_manager_t* manager, const char* ci_name);
int lifecycle_stop_ci(lifecycle_manager_t* manager,
                     const char* ci_name,
                     bool graceful);
int lifecycle_restart_ci(lifecycle_manager_t* manager, const char* ci_name);

/* Status transitions */
int lifecycle_transition_internal(lifecycle_manager_t* manager,
                        const char* ci_name,
                        lifecycle_event_t event,
                        const char* reason);

/* Task management */
int lifecycle_assign_task(lifecycle_manager_t* manager,
                         const char* ci_name,
                         const char* task_description);
int lifecycle_complete_task(lifecycle_manager_t* manager,
                           const char* ci_name,
                           bool success);

/* Heartbeat management */
int lifecycle_heartbeat(lifecycle_manager_t* manager, const char* ci_name);
int lifecycle_check_heartbeats(lifecycle_manager_t* manager);

/* Error handling */
int lifecycle_report_error(lifecycle_manager_t* manager,
                          const char* ci_name,
                          const char* error_message);

/* Query functions */
ci_lifecycle_t* lifecycle_get_ci(lifecycle_manager_t* manager,
                                const char* ci_name);
lifecycle_transition_t* lifecycle_get_history(lifecycle_manager_t* manager,
                                             const char* ci_name);

/* Health and statistics */
int lifecycle_health_check(lifecycle_manager_t* manager);
void lifecycle_print_status(lifecycle_manager_t* manager);
void lifecycle_print_ci(ci_lifecycle_t* ci);
void lifecycle_print_timeline(ci_lifecycle_t* ci);

/* Cleanup */
void lifecycle_clear_history(ci_lifecycle_t* ci);

#endif /* ARGO_LIFECYCLE_H */