/* © 2025 Casey Koons All rights reserved */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "argo_shared_services.h"
#include "argo_error.h"
#include "argo_limits.h"

/* Background thread main loop */
static void* shared_services_thread_main(void* arg) {
    shared_services_t* svc = (shared_services_t*)arg;

    while (!svc->should_stop) {
        time_t now = time(NULL);

        int lock_result = pthread_mutex_lock(&svc->lock);
        if (lock_result != 0) {
            /* Mutex failure in background thread - log and continue */
            usleep(SHARED_SERVICES_CHECK_INTERVAL_MS * MICROSECONDS_PER_MILLISECOND);
            continue;
        }

        /* Check each task to see if it should run */
        for (int i = 0; i < svc->task_count; i++) {
            shared_service_task_t* task = &svc->tasks[i];

            if (!task->enabled) {
                continue;
            }

            /* Time to run this task? */
            time_t elapsed = now - task->last_run;
            if (elapsed >= task->interval_sec) {
                /* Execute task (release lock during execution) */
                pthread_mutex_unlock(&svc->lock);

                task->fn(task->context);

                lock_result = pthread_mutex_lock(&svc->lock);
                if (lock_result != 0) {
                    /* Can't continue without lock, sleep and retry */
                    usleep(SHARED_SERVICES_CHECK_INTERVAL_MS * MICROSECONDS_PER_MILLISECOND);
                    break;
                }

                /* Update last run time and statistics */
                task->last_run = time(NULL);
                svc->total_task_runs++;
            }
        }

        pthread_mutex_unlock(&svc->lock);

        /* Sleep briefly before next check */
        usleep(SHARED_SERVICES_CHECK_INTERVAL_MS * MICROSECONDS_PER_MILLISECOND);
    }

    return NULL;
}

/* Create shared services manager */
shared_services_t* shared_services_create(void) {
    shared_services_t* svc = calloc(1, sizeof(shared_services_t));
    if (!svc) {
        return NULL;
    }

    if (pthread_mutex_init(&svc->lock, NULL) != 0) {
        free(svc);
        return NULL;
    }

    svc->running = false;
    svc->should_stop = false;
    svc->task_count = 0;
    svc->total_task_runs = 0;
    svc->started_at = 0;

    return svc;
}

/* Destroy shared services manager */
void shared_services_destroy(shared_services_t* svc) {
    if (!svc) {
        return;
    }

    /* Ensure thread is stopped */
    if (svc->running) {
        shared_services_stop(svc);
    }

    pthread_mutex_destroy(&svc->lock);
    free(svc);
}

/* Start background thread */
int shared_services_start(shared_services_t* svc) {
    if (!svc) {
        return E_INVALID_PARAMS;
    }

    int lock_result = pthread_mutex_lock(&svc->lock);
    if (lock_result != 0) {
        return E_SYSTEM_PROCESS;
    }

    if (svc->running) {
        pthread_mutex_unlock(&svc->lock);
        return E_INVALID_STATE;
    }

    svc->should_stop = false;
    svc->started_at = time(NULL);

    int result = pthread_create(&svc->thread, NULL,
                                shared_services_thread_main, svc);

    if (result == 0) {
        svc->running = true;
    }

    pthread_mutex_unlock(&svc->lock);

    return (result == 0) ? ARGO_SUCCESS : E_SYSTEM_THREAD;
}

/* Stop background thread (wait for completion) */
void shared_services_stop(shared_services_t* svc) {
    if (!svc) {
        return;
    }

    int lock_result = pthread_mutex_lock(&svc->lock);
    if (lock_result != 0) {
        /* Can't safely stop without lock */
        return;
    }

    if (!svc->running) {
        pthread_mutex_unlock(&svc->lock);
        return;
    }

    /* Signal thread to stop */
    svc->should_stop = true;

    pthread_mutex_unlock(&svc->lock);

    /* Wait for thread to complete all pending tasks */
    pthread_join(svc->thread, NULL);

    lock_result = pthread_mutex_lock(&svc->lock);
    if (lock_result != 0) {
        /* Thread is stopped, just can't update state cleanly */
        return;
    }
    svc->running = false;
    pthread_mutex_unlock(&svc->lock);
}

/* Check if thread is running */
bool shared_services_is_running(shared_services_t* svc) {
    if (!svc) {
        return false;
    }

    int lock_result = pthread_mutex_lock(&svc->lock);
    if (lock_result != 0) {
        return false;
    }
    bool running = svc->running;
    pthread_mutex_unlock(&svc->lock);

    return running;
}

/* Register a new task */
int shared_services_register_task(shared_services_t* svc,
                                   shared_service_task_fn fn,
                                   void* context,
                                   int interval_sec) {
    if (!svc || !fn || interval_sec <= 0) {
        return E_INVALID_PARAMS;
    }

    int lock_result = pthread_mutex_lock(&svc->lock);
    if (lock_result != 0) {
        return E_SYSTEM_PROCESS;
    }

    /* Check if task limit reached */
    if (svc->task_count >= SHARED_SERVICES_MAX_TASKS) {
        pthread_mutex_unlock(&svc->lock);
        return E_RESOURCE_LIMIT;
    }

    /* Check if task already registered */
    for (int i = 0; i < svc->task_count; i++) {
        if (svc->tasks[i].fn == fn) {
            pthread_mutex_unlock(&svc->lock);
            return E_DUPLICATE;
        }
    }

    /* Add new task */
    shared_service_task_t* task = &svc->tasks[svc->task_count];
    task->fn = fn;
    task->context = context;
    task->interval_sec = interval_sec;
    task->last_run = time(NULL);
    task->enabled = true;

    svc->task_count++;

    pthread_mutex_unlock(&svc->lock);

    return ARGO_SUCCESS;
}

/* Unregister a task */
int shared_services_unregister_task(shared_services_t* svc,
                                     shared_service_task_fn fn) {
    if (!svc || !fn) {
        return E_INVALID_PARAMS;
    }

    int lock_result = pthread_mutex_lock(&svc->lock);
    if (lock_result != 0) {
        return E_SYSTEM_PROCESS;
    }

    /* Find and remove task */
    int found = -1;
    for (int i = 0; i < svc->task_count; i++) {
        if (svc->tasks[i].fn == fn) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        pthread_mutex_unlock(&svc->lock);
        return E_NOT_FOUND;
    }

    /* Shift remaining tasks down */
    for (int i = found; i < svc->task_count - 1; i++) {
        svc->tasks[i] = svc->tasks[i + 1];
    }

    svc->task_count--;

    pthread_mutex_unlock(&svc->lock);

    return ARGO_SUCCESS;
}

/* Enable or disable a task */
int shared_services_enable_task(shared_services_t* svc,
                                 shared_service_task_fn fn,
                                 bool enable) {
    if (!svc || !fn) {
        return E_INVALID_PARAMS;
    }

    int lock_result = pthread_mutex_lock(&svc->lock);
    if (lock_result != 0) {
        return E_SYSTEM_PROCESS;
    }

    /* Find task */
    int found = -1;
    for (int i = 0; i < svc->task_count; i++) {
        if (svc->tasks[i].fn == fn) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        pthread_mutex_unlock(&svc->lock);
        return E_NOT_FOUND;
    }

    svc->tasks[found].enabled = enable;

    pthread_mutex_unlock(&svc->lock);

    return ARGO_SUCCESS;
}

/* Get total number of task runs */
uint64_t shared_services_get_task_runs(shared_services_t* svc) {
    if (!svc) {
        return 0;
    }

    int lock_result = pthread_mutex_lock(&svc->lock);
    if (lock_result != 0) {
        return 0;
    }
    uint64_t runs = svc->total_task_runs;
    pthread_mutex_unlock(&svc->lock);

    return runs;
}

/* Get uptime in seconds */
time_t shared_services_get_uptime(shared_services_t* svc) {
    if (!svc) {
        return 0;
    }

    int lock_result = pthread_mutex_lock(&svc->lock);
    if (lock_result != 0) {
        return 0;
    }
    time_t uptime = 0;
    if (svc->running) {
        uptime = time(NULL) - svc->started_at;
    }
    pthread_mutex_unlock(&svc->lock);

    return uptime;
}
