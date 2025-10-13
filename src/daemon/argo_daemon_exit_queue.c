/* © 2025 Casey Koons All rights reserved */
/* Daemon exit code queue - lock-free signal-safe communication */

/* System includes */
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_daemon_exit_queue.h"

/* Initialize queue */
void exit_queue_init(exit_code_queue_t* queue) {
    if (!queue) return;

    memset(queue, 0, sizeof(exit_code_queue_t));
    queue->write_idx = 0;
    queue->read_idx = 0;
    queue->dropped = 0;

    /* Mark all entries as invalid */
    for (int i = 0; i < EXIT_QUEUE_SIZE; i++) {
        queue->entries[i].valid = false;
    }
}

/* Push exit code (ASYNC-SIGNAL-SAFE) */
bool exit_queue_push(exit_code_queue_t* queue, pid_t pid, int exit_code) {
    if (!queue) return false;

    /* Calculate next write position */
    int current_write = queue->write_idx;
    int next_write = (current_write + 1) % EXIT_QUEUE_SIZE;

    /* Check if queue is full (write would overtake read) */
    if (next_write == queue->read_idx) {
        /* Queue full - increment dropped counter */
        queue->dropped++;
        return false;
    }

    /* Write entry */
    queue->entries[current_write].pid = pid;
    queue->entries[current_write].exit_code = exit_code;
    queue->entries[current_write].timestamp = time(NULL);
    queue->entries[current_write].valid = true;

    /* Advance write index (atomic on most architectures for int) */
    queue->write_idx = next_write;

    return true;
}

/* Pop exit code (NOT signal-safe) */
bool exit_queue_pop(exit_code_queue_t* queue, exit_code_entry_t* entry) {
    if (!queue || !entry) return false;

    /* Check if queue is empty */
    if (queue->read_idx == queue->write_idx) {
        return false;
    }

    /* Read entry */
    int current_read = queue->read_idx;
    if (!queue->entries[current_read].valid) {
        /* Should never happen, but guard against it */
        return false;
    }

    /* Copy entry */
    *entry = queue->entries[current_read];

    /* Mark as invalid and advance read index */
    queue->entries[current_read].valid = false;
    queue->read_idx = (current_read + 1) % EXIT_QUEUE_SIZE;

    return true;
}

/* Get dropped count */
int exit_queue_get_dropped(exit_code_queue_t* queue) {
    if (!queue) return 0;

    int dropped = queue->dropped;
    queue->dropped = 0;  /* Reset counter */
    return dropped;
}

/* Check if empty */
bool exit_queue_is_empty(const exit_code_queue_t* queue) {
    if (!queue) return true;
    return (queue->read_idx == queue->write_idx);
}
