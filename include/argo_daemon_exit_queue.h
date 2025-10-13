/* © 2025 Casey Koons All rights reserved */
/* Daemon exit code queue - async-signal-safe communication from SIGCHLD to completion task */

#ifndef ARGO_DAEMON_EXIT_QUEUE_H
#define ARGO_DAEMON_EXIT_QUEUE_H

#include <sys/types.h>
#include <time.h>
#include <stdbool.h>

/* Exit code entry - stored when SIGCHLD reaps a child */
typedef struct {
    pid_t pid;              /* Process ID that exited */
    int exit_code;          /* Exit code from waitpid() */
    time_t timestamp;       /* When process was reaped */
    bool valid;             /* Entry contains valid data */
} exit_code_entry_t;

/* Exit code queue
 *
 * Lock-free ring buffer for passing exit codes from SIGCHLD handler
 * to workflow completion task.
 *
 * Design:
 * - Fixed-size ring buffer (no malloc in signal handler)
 * - Write-only from signal handler (async-signal-safe)
 * - Read-only from completion task
 * - Atomic operations for indices
 * - If queue fills, oldest entries are dropped (logged by completion task)
 *
 * Thread Safety:
 * - Signal handler: writes only
 * - Completion task: reads and clears only
 * - No locks needed (single writer, single reader)
 */

#define EXIT_QUEUE_SIZE 128  /* Max pending exit codes (must be power of 2) */

typedef struct {
    exit_code_entry_t entries[EXIT_QUEUE_SIZE];
    volatile int write_idx;  /* Next write position (updated by signal handler) */
    volatile int read_idx;   /* Next read position (updated by completion task) */
    volatile int dropped;    /* Count of dropped entries (queue full) */
} exit_code_queue_t;

/* Initialize exit code queue
 *
 * Must be called before use. Safe to call multiple times.
 *
 * Parameters:
 *   queue - Queue to initialize
 */
void exit_queue_init(exit_code_queue_t* queue);

/* Push exit code (ASYNC-SIGNAL-SAFE)
 *
 * Called from SIGCHLD handler when a child exits.
 * If queue is full, increments dropped counter.
 *
 * Parameters:
 *   queue     - Queue handle
 *   pid       - Process ID that exited
 *   exit_code - Exit code from WEXITSTATUS()
 *
 * Returns:
 *   true if added, false if queue full (entry dropped)
 */
bool exit_queue_push(exit_code_queue_t* queue, pid_t pid, int exit_code);

/* Pop exit code (NOT signal-safe)
 *
 * Called from completion task to retrieve exit codes.
 * Returns false when queue is empty.
 *
 * Parameters:
 *   queue - Queue handle
 *   entry - Output: exit code entry
 *
 * Returns:
 *   true if entry retrieved, false if queue empty
 */
bool exit_queue_pop(exit_code_queue_t* queue, exit_code_entry_t* entry);

/* Get dropped count
 *
 * Returns number of exit codes dropped due to queue overflow.
 * Resets counter to zero after reading.
 *
 * Parameters:
 *   queue - Queue handle
 *
 * Returns:
 *   Number of dropped entries since last check
 */
int exit_queue_get_dropped(exit_code_queue_t* queue);

/* Check if queue is empty (NOT signal-safe)
 *
 * Parameters:
 *   queue - Queue handle
 *
 * Returns:
 *   true if empty, false otherwise
 */
bool exit_queue_is_empty(const exit_code_queue_t* queue);

#endif /* ARGO_DAEMON_EXIT_QUEUE_H */
