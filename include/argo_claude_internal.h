/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_CLAUDE_INTERNAL_H
#define ARGO_CLAUDE_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include "argo_ci.h"
#include "argo_limits.h"

/* Working memory structure (stored in mmap) */
typedef struct working_memory {
    uint32_t magic;           /* 0xC1A0DE00 - verify valid */
    uint32_t version;         /* Format version */
    size_t used_bytes;        /* Current usage */
    time_t last_update;       /* Last modification */

    /* Session continuity */
    char session_id[ARGO_BUFFER_SMALL];
    char ci_name[ARGO_BUFFER_TINY];
    uint32_t turn_count;

    /* Sunset/sunrise data */
    bool has_sunset;
    size_t sunset_offset;     /* Offset to sunset notes */

    /* Apollo digest */
    bool has_apollo;
    size_t apollo_offset;     /* Offset to Apollo digest */

    /* Current task context */
    size_t task_offset;       /* Offset to current task */

    /* Memory content starts here */
    char content[];
} working_memory_t;

/* Claude context structure */
typedef struct claude_context {
    /* Process management */
    pid_t claude_pid;
    int stdin_pipe[2];   /* Write to [1], Claude reads from [0] */
    int stdout_pipe[2];  /* Claude writes to [1], we read from [0] */
    int stderr_pipe[2];  /* Claude errors to [1], we read from [0] */

    /* Working memory (memory-mapped) */
    void* working_memory;
    size_t memory_size;
    int memory_fd;
    char session_path[ARGO_BUFFER_MEDIUM];

    /* Sunset/sunrise state */
    bool approaching_limit;
    size_t tokens_used;
    size_t context_limit;
    char* sunset_notes;

    /* Response accumulator */
    char* response_buffer;
    size_t response_size;
    size_t response_capacity;

    /* Statistics */
    uint64_t total_queries;
    time_t session_start;
    time_t last_query;

    /* Provider interface */
    ci_provider_t provider;
} claude_context_t;

/* Working memory constants */
#define WORKING_MEMORY_MAGIC 0xC1A0DE00
#define WORKING_MEMORY_VERSION 1
#define WORKING_MEMORY_SIZE (533 * 1024)  /* 533KB limit */

/* Error message for exec failure in child process */
#define CLAUDE_EXEC_FAILED_MSG "Failed to execute claude: "

#endif /* ARGO_CLAUDE_INTERNAL_H */
