/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>

/* Project includes */
#include "argo_claude_memory.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"
#include "argo_filesystem.h"
#include "argo_memory.h"

/* Setup working memory with mmap */
int setup_working_memory(claude_context_t* ctx, const char* ci_name) {
    if (!ctx || !ci_name) {
        return E_INVALID_PARAMS;
    }
    /* Open or create session file */
    ctx->memory_fd = open(ctx->session_path, O_RDWR | O_CREAT, ARGO_FILE_MODE_PRIVATE);
    if (ctx->memory_fd < 0) {
        argo_report_error(E_SYSTEM_FILE, "setup_working_memory", ERR_FMT_SYSCALL_ERROR, ERR_MSG_FILE_OPEN_FAILED, strerror(errno));
        return E_SYSTEM_FILE;
    }

    /* Ensure file is correct size */
    if (ftruncate(ctx->memory_fd, WORKING_MEMORY_SIZE) < 0) {
        argo_report_error(E_SYSTEM_FILE, "setup_working_memory", ERR_FMT_SYSCALL_ERROR, ERR_MSG_FTRUNCATE_FAILED, strerror(errno));
        close(ctx->memory_fd);
        return E_SYSTEM_FILE;
    }

    /* Memory-map the file */
    ctx->working_memory = mmap(NULL, WORKING_MEMORY_SIZE,
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED, ctx->memory_fd, 0);

    if (ctx->working_memory == MAP_FAILED) {
        argo_report_error(E_SYSTEM_MEMORY, "setup_working_memory", ERR_FMT_SYSCALL_ERROR, ERR_MSG_MMAP_FAILED, strerror(errno));
        close(ctx->memory_fd);
        return E_SYSTEM_MEMORY;
    }

    ctx->memory_size = WORKING_MEMORY_SIZE;

    /* Initialize if new */
    working_memory_t* mem = (working_memory_t*)ctx->working_memory;
    if (mem->magic != WORKING_MEMORY_MAGIC) {
        memset(mem, 0, sizeof(working_memory_t));
        mem->magic = WORKING_MEMORY_MAGIC;
        mem->version = WORKING_MEMORY_VERSION;
        mem->last_update = time(NULL);
        strncpy(mem->ci_name, ci_name, sizeof(mem->ci_name) - 1);
        snprintf(mem->session_id, sizeof(mem->session_id),
                "%s_%ld", ci_name, time(NULL));
    }

    return ARGO_SUCCESS;
}

/* Build context with working memory */
char* build_context_with_memory(claude_context_t* ctx, const char* prompt) {
    if (!ctx || !prompt) {
        return NULL;
    }
    working_memory_t* mem = (working_memory_t*)ctx->working_memory;
    if (!mem || mem->magic != WORKING_MEMORY_MAGIC) {
        /* No working memory, just return prompt */
        return strdup(prompt); /* GUIDELINE_APPROVED: NULL return handled by caller */
    }

    /* Calculate total size needed */
    size_t total_size = strlen(prompt) + MEMORY_NOTES_PADDING;  /* Prompt + "## Current Task\n" header */

    if (mem->has_sunset && mem->sunset_offset > 0) {
        total_size += strlen(mem->content + mem->sunset_offset) + MEMORY_NOTES_PADDING;
    }
    if (mem->has_apollo && mem->apollo_offset > 0) {
        total_size += strlen(mem->content + mem->apollo_offset) + MEMORY_NOTES_PADDING;
    }

    /* Allocate buffer */
    char* context = malloc(total_size);
    if (!context) {
        return NULL;
    }

    /* Build context using safe string operations */
    size_t offset = 0;

    /* Add sunset notes if available */
    if (mem->has_sunset && mem->sunset_offset > 0) {
        offset += snprintf(context + offset, total_size - offset,
                          "## Previous Session Context\n%s\n\n",
                          mem->content + mem->sunset_offset);
    }

    /* Add Apollo digest if available */
    if (mem->has_apollo && mem->apollo_offset > 0) {
        offset += snprintf(context + offset, total_size - offset,
                          "## Memory Digest\n%s\n\n",
                          mem->content + mem->apollo_offset);
    }

    /* Add current prompt */
    snprintf(context + offset, total_size - offset,
             "## Current Task\n%s", prompt);

    return context;
}

/* Load working memory */
int load_working_memory(claude_context_t* ctx) {
    if (!ctx) {
        return E_INVALID_PARAMS;
    }
    /* Memory is already mapped, just verify it's valid */
    working_memory_t* mem = (working_memory_t*)ctx->working_memory;
    if (mem->magic == WORKING_MEMORY_MAGIC) {
        return ARGO_SUCCESS;
    }
    return E_INTERNAL_CORRUPT;
}

/* Save working memory */
int save_working_memory(claude_context_t* ctx) {
    if (!ctx) {
        return E_INVALID_PARAMS;
    }
    /* Memory is mapped, so changes are automatic */
    /* Just sync to disk */
    if (msync(ctx->working_memory, ctx->memory_size, MS_SYNC) < 0) {
        argo_report_error(E_SYSTEM_FILE, "save_working_memory", ERR_FMT_SYSCALL_ERROR, ERR_MSG_MSYNC_FAILED, strerror(errno));
        return E_SYSTEM_FILE;
    }
    return ARGO_SUCCESS;
}

/* Update turn count in working memory */
void claude_memory_update_turn(claude_context_t* ctx) {
    if (!ctx) {
        return;
    }
    working_memory_t* mem = (working_memory_t*)ctx->working_memory;
    if (mem && mem->magic == WORKING_MEMORY_MAGIC) {
        mem->turn_count++;
        mem->last_update = time(NULL);
        /* Response content stored in memory_digest at higher level */
    }
}

/* Cleanup working memory */
void cleanup_working_memory(claude_context_t* ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->working_memory && ctx->working_memory != MAP_FAILED) {
        munmap(ctx->working_memory, ctx->memory_size);
    }
    if (ctx->memory_fd >= 0) {
        close(ctx->memory_fd);
    }
}
