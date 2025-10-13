/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_SHARED_SERVICES_H
#define ARGO_SHARED_SERVICES_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>

/* Maximum number of registered tasks */
#define SHARED_SERVICES_MAX_TASKS 32

/* Task check interval (how often thread wakes up) */
#define SHARED_SERVICES_CHECK_INTERVAL_MS 100

/* Task function signature */
typedef void (*shared_service_task_fn)(void* context);

/* Task structure */
typedef struct shared_service_task {
    shared_service_task_fn fn;     /* Task function to execute */
    void* context;                  /* User data passed to function */
    int interval_sec;               /* How often to run (seconds) */
    time_t last_run;                /* Timestamp of last execution */
    bool enabled;                   /* Can be disabled without unregistering */
} shared_service_task_t;

/*
 * Shared services manager
 *
 * THREAD SAFETY:
 * - All access to tasks[], task_count, and statistics MUST be protected by lock
 * - running and should_stop are accessed atomically (bool reads/writes are atomic)
 * - Background thread holds lock only during task list iteration setup
 * - Task functions execute WITHOUT holding lock (to prevent deadlocks)
 */
typedef struct shared_services {
    pthread_t thread;               /* Background thread */
    pthread_mutex_t lock;           /* PROTECTS: tasks, task_count, statistics */
    bool running;                   /* Thread running flag (atomic) */
    bool should_stop;               /* Shutdown signal (atomic) */

    /* Registered tasks - PROTECTED BY lock */
    shared_service_task_t tasks[SHARED_SERVICES_MAX_TASKS];
    int task_count;                 /* PROTECTED BY lock */

    /* Statistics - PROTECTED BY lock */
    uint64_t total_task_runs;       /* PROTECTED BY lock */
    time_t started_at;              /* PROTECTED BY lock */

} shared_services_t;

/* Lifecycle management */
shared_services_t* shared_services_create(void);
void shared_services_destroy(shared_services_t* svc);

/* Thread control */
int shared_services_start(shared_services_t* svc);  /* LOCKS: svc->lock briefly */
void shared_services_stop(shared_services_t* svc);  /* LOCKS: svc->lock briefly */
bool shared_services_is_running(shared_services_t* svc);  /* LOCKS: none (atomic read) */

/* Task management - THREAD SAFE */
int shared_services_register_task(shared_services_t* svc,  /* LOCKS: svc->lock */
                                   shared_service_task_fn fn,
                                   void* context,
                                   int interval_sec);

int shared_services_unregister_task(shared_services_t* svc,  /* LOCKS: svc->lock */
                                     shared_service_task_fn fn);

int shared_services_enable_task(shared_services_t* svc,  /* LOCKS: svc->lock */
                                 shared_service_task_fn fn,
                                 bool enable);

/* Statistics - THREAD SAFE */
uint64_t shared_services_get_task_runs(shared_services_t* svc);  /* LOCKS: svc->lock */
time_t shared_services_get_uptime(shared_services_t* svc);  /* LOCKS: svc->lock */

#endif /* ARGO_SHARED_SERVICES_H */
