/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>

/* Project includes */
#include "argo_ci.h"
#include "argo_ci_defaults.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_memory.h"
#include "argo_claude.h"

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
    char session_path[256];

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

/* Working memory structure (stored in mmap) */
typedef struct working_memory {
    uint32_t magic;           /* 0xC1A0DE00 - verify valid */
    uint32_t version;         /* Format version */
    size_t used_bytes;        /* Current usage */
    time_t last_update;       /* Last modification */

    /* Session continuity */
    char session_id[64];
    char ci_name[32];
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

#define WORKING_MEMORY_MAGIC 0xC1A0DE00
#define WORKING_MEMORY_VERSION 1
#define WORKING_MEMORY_SIZE (533 * 1024)  /* 533KB limit */

/* Static function declarations */
static int claude_init(ci_provider_t* provider);
static int claude_connect(ci_provider_t* provider);
static int claude_query(ci_provider_t* provider, const char* prompt,
                       ci_response_callback callback, void* userdata);
static int claude_stream(ci_provider_t* provider, const char* prompt,
                        ci_stream_callback callback, void* userdata);
static void claude_cleanup(ci_provider_t* provider);

/* Process management */
static int spawn_claude_process(claude_context_t* ctx);
static int kill_claude_process(claude_context_t* ctx);
static int write_to_claude(claude_context_t* ctx, const char* input);
static int read_from_claude(claude_context_t* ctx, char** output, int timeout_ms);

/* Working memory management */
static int setup_working_memory(claude_context_t* ctx, const char* ci_name);
static int load_working_memory(claude_context_t* ctx);
static int save_working_memory(claude_context_t* ctx);
static void cleanup_working_memory(claude_context_t* ctx);

/* Sunset/sunrise handling */
static bool check_context_limit(claude_context_t* ctx, size_t new_tokens);
static int trigger_sunset(claude_context_t* ctx);
static char* build_context_with_memory(claude_context_t* ctx, const char* prompt);

/* Create Claude provider */
ci_provider_t* claude_create_provider(const char* ci_name) {
    claude_context_t* ctx = calloc(1, sizeof(claude_context_t));
    if (!ctx) {
        argo_report_error(E_SYSTEM_MEMORY, "claude_create_provider", "");
        return NULL;
    }

    /* Setup provider interface */
    ctx->provider.init = claude_init;
    ctx->provider.connect = claude_connect;
    ctx->provider.query = claude_query;
    ctx->provider.stream = claude_stream;
    ctx->provider.cleanup = claude_cleanup;
    ctx->provider.context = ctx;

    /* Set Claude configuration */
    strncpy(ctx->provider.name, "claude", sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, "claude-3.5-sonnet", sizeof(ctx->provider.model) - 1);

    /* Look up model defaults */
    const ci_model_config_t* config = ci_get_model_defaults("claude-3.5-sonnet");
    if (config) {
        ctx->provider.supports_streaming = false;  /* Not with CLI */
        ctx->provider.supports_memory = true;
        ctx->provider.supports_sunset_sunrise = true;
        ctx->provider.max_context = config->max_context;
        ctx->context_limit = config->max_context;
    } else {
        ctx->context_limit = 200000;  /* Default 200K tokens */
    }

    /* Initialize pipes to invalid */
    ctx->stdin_pipe[0] = ctx->stdin_pipe[1] = -1;
    ctx->stdout_pipe[0] = ctx->stdout_pipe[1] = -1;
    ctx->stderr_pipe[0] = ctx->stderr_pipe[1] = -1;
    ctx->claude_pid = -1;

    /* Build session path */
    snprintf(ctx->session_path, sizeof(ctx->session_path),
             CLAUDE_SESSION_PATH, ci_name ? ci_name : "default");

    LOG_INFO("Created Claude provider for %s", ci_name ? ci_name : "default");
    return &ctx->provider;
}

/* Initialize Claude provider */
static int claude_init(ci_provider_t* provider) {
    ARGO_CHECK_NULL(provider);
    claude_context_t* ctx = (claude_context_t*)provider->context;
    ARGO_CHECK_NULL(ctx);

    /* Allocate response buffer */
    ctx->response_capacity = 16384;
    ctx->response_buffer = malloc(ctx->response_capacity);
    if (!ctx->response_buffer) {
        return E_SYSTEM_MEMORY;
    }

    /* Setup working memory */
    int result = setup_working_memory(ctx, ctx->provider.name);
    if (result != ARGO_SUCCESS) {
        free(ctx->response_buffer);
        return result;
    }

    /* Try to load existing session */
    if (load_working_memory(ctx) == ARGO_SUCCESS) {
        LOG_INFO("Loaded existing Claude session from %s", ctx->session_path);
    }

    ctx->session_start = time(NULL);
    LOG_DEBUG("Claude provider initialized");
    return ARGO_SUCCESS;
}

/* Connect (spawn Claude process) */
static int claude_connect(ci_provider_t* provider) {
    ARGO_CHECK_NULL(provider);
    claude_context_t* ctx = (claude_context_t*)provider->context;
    ARGO_CHECK_NULL(ctx);

    if (ctx->claude_pid > 0) {
        /* Already connected */
        return ARGO_SUCCESS;
    }

    return spawn_claude_process(ctx);
}

/* Query Claude */
static int claude_query(ci_provider_t* provider, const char* prompt,
                       ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(provider);
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);

    claude_context_t* ctx = (claude_context_t*)provider->context;
    ARGO_CHECK_NULL(ctx);

    /* Check context limit */
    size_t prompt_tokens = strlen(prompt) / 4;  /* Rough estimate */
    if (check_context_limit(ctx, prompt_tokens)) {
        LOG_INFO("Approaching context limit, triggering sunset");
        trigger_sunset(ctx);
    }

    /* Build full context with working memory */
    char* full_prompt = build_context_with_memory(ctx, prompt);
    if (!full_prompt) {
        ci_response_t response = {
            .success = false,
            .error_code = E_SYSTEM_MEMORY,
            .content = "Failed to build context",
            .model_used = "claude-3.5-sonnet",
            .timestamp = time(NULL)
        };
        callback(&response, userdata);
        return E_SYSTEM_MEMORY;
    }

    /* Spawn Claude if not running */
    if (ctx->claude_pid <= 0) {
        int result = spawn_claude_process(ctx);
        if (result != ARGO_SUCCESS) {
            free(full_prompt);
            ci_response_t response = {
                .success = false,
                .error_code = result,
                .content = "Failed to spawn Claude process",
                .model_used = "claude-3.5-sonnet",
                .timestamp = time(NULL)
            };
            callback(&response, userdata);
            return result;
        }
    }

    /* Send prompt to Claude */
    int result = write_to_claude(ctx, full_prompt);
    free(full_prompt);

    if (result != ARGO_SUCCESS) {
        ci_response_t response = {
            .success = false,
            .error_code = result,
            .content = "Failed to send prompt to Claude",
            .model_used = "claude-3.5-sonnet",
            .timestamp = time(NULL)
        };
        callback(&response, userdata);
        return result;
    }

    /* Read response */
    char* output = NULL;
    result = read_from_claude(ctx, &output, CLAUDE_TIMEOUT_MS);

    if (result != ARGO_SUCCESS) {
        ci_response_t response = {
            .success = false,
            .error_code = result,
            .content = "Failed to read Claude response",
            .model_used = "claude-3.5-sonnet",
            .timestamp = time(NULL)
        };
        callback(&response, userdata);
        return result;
    }

    /* Update working memory session tracking */
    working_memory_t* mem = (working_memory_t*)ctx->working_memory;
    if (mem && mem->magic == WORKING_MEMORY_MAGIC) {
        mem->turn_count++;
        mem->last_update = time(NULL);
        /* Response content stored in memory_digest at higher level */
    }

    /* Build response */
    ci_response_t response = {
        .success = true,
        .error_code = ARGO_SUCCESS,
        .content = output,
        .model_used = "claude-3.5-sonnet",
        .timestamp = time(NULL)
    };

    /* Update statistics */
    ctx->total_queries++;
    ctx->last_query = time(NULL);
    ctx->tokens_used += strlen(output) / 4;  /* Rough estimate */

    /* Save working memory */
    save_working_memory(ctx);

    /* Invoke callback */
    callback(&response, userdata);

    LOG_DEBUG("Claude query completed, response size: %zu", strlen(output));
    return ARGO_SUCCESS;
}

/* Streaming not supported with CLI */
static int claude_stream(ci_provider_t* provider, const char* prompt,
                        ci_stream_callback callback, void* userdata) {
    (void)provider;
    (void)prompt;
    (void)callback;
    (void)userdata;
    return E_INTERNAL_NOTIMPL;
}

/* Cleanup Claude provider */
static void claude_cleanup(ci_provider_t* provider) {
    if (!provider) return;

    claude_context_t* ctx = (claude_context_t*)provider->context;
    if (!ctx) return;

    /* Kill Claude process if running */
    if (ctx->claude_pid > 0) {
        kill_claude_process(ctx);
    }

    /* Save and cleanup working memory */
    save_working_memory(ctx);
    cleanup_working_memory(ctx);

    /* Free buffers */
    if (ctx->response_buffer) {
        free(ctx->response_buffer);
    }
    if (ctx->sunset_notes) {
        free(ctx->sunset_notes);
    }

    LOG_INFO("Claude provider cleanup: queries=%llu tokens=%zu",
             ctx->total_queries, ctx->tokens_used);

    free(ctx);
}

/* Spawn Claude subprocess */
static int spawn_claude_process(claude_context_t* ctx) {
    /* Create pipes */
    if (pipe(ctx->stdin_pipe) < 0 ||
        pipe(ctx->stdout_pipe) < 0 ||
        pipe(ctx->stderr_pipe) < 0) {
        argo_report_error(E_SYSTEM_FORK, "spawn_claude_process", "pipe() failed: %s", strerror(errno));
        return E_SYSTEM_FORK;
    }

    /* Fork process */
    ctx->claude_pid = fork();
    if (ctx->claude_pid < 0) {
        argo_report_error(E_SYSTEM_FORK, "spawn_claude_process", "fork() failed: %s", strerror(errno));
        return E_SYSTEM_FORK;
    }

    if (ctx->claude_pid == 0) {
        /* Child process - become Claude */

        /* Redirect stdin/stdout/stderr */
        dup2(ctx->stdin_pipe[0], STDIN_FILENO);
        dup2(ctx->stdout_pipe[1], STDOUT_FILENO);
        dup2(ctx->stderr_pipe[1], STDERR_FILENO);

        /* Close unused pipe ends */
        close(ctx->stdin_pipe[1]);
        close(ctx->stdout_pipe[0]);
        close(ctx->stderr_pipe[0]);

        /* Execute Claude */
        execlp("claude", "claude", NULL);

        /* If we get here, exec failed */
        fprintf(stderr, "Failed to execute claude: %s\n", strerror(errno));
        exit(1);
    }

    /* Parent process */

    /* Close unused pipe ends */
    close(ctx->stdin_pipe[0]);
    close(ctx->stdout_pipe[1]);
    close(ctx->stderr_pipe[1]);

    /* Make pipes non-blocking */
    fcntl(ctx->stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(ctx->stderr_pipe[0], F_SETFL, O_NONBLOCK);

    LOG_INFO("Spawned Claude process with PID %d", ctx->claude_pid);
    return ARGO_SUCCESS;
}

/* Kill Claude process */
static int kill_claude_process(claude_context_t* ctx) {
    if (ctx->claude_pid <= 0) {
        return ARGO_SUCCESS;
    }

    /* Send SIGTERM */
    kill(ctx->claude_pid, SIGTERM);

    /* Wait for process to exit */
    int status;
    waitpid(ctx->claude_pid, &status, 0);

    /* Close pipes */
    if (ctx->stdin_pipe[1] >= 0) close(ctx->stdin_pipe[1]);
    if (ctx->stdout_pipe[0] >= 0) close(ctx->stdout_pipe[0]);
    if (ctx->stderr_pipe[0] >= 0) close(ctx->stderr_pipe[0]);

    ctx->claude_pid = -1;
    LOG_DEBUG("Claude process terminated");
    return ARGO_SUCCESS;
}

/* Setup working memory with mmap */
static int setup_working_memory(claude_context_t* ctx, const char* ci_name) {
    /* Open or create session file */
    ctx->memory_fd = open(ctx->session_path, O_RDWR | O_CREAT, 0600);
    if (ctx->memory_fd < 0) {
        argo_report_error(E_SYSTEM_FILE, "setup_working_memory", "open() failed: %s", strerror(errno));
        return E_SYSTEM_FILE;
    }

    /* Ensure file is correct size */
    if (ftruncate(ctx->memory_fd, WORKING_MEMORY_SIZE) < 0) {
        argo_report_error(E_SYSTEM_FILE, "setup_working_memory", "ftruncate() failed: %s", strerror(errno));
        close(ctx->memory_fd);
        return E_SYSTEM_FILE;
    }

    /* Memory-map the file */
    ctx->working_memory = mmap(NULL, WORKING_MEMORY_SIZE,
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED, ctx->memory_fd, 0);

    if (ctx->working_memory == MAP_FAILED) {
        argo_report_error(E_SYSTEM_MEMORY, "setup_working_memory", "mmap() failed: %s", strerror(errno));
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
static char* build_context_with_memory(claude_context_t* ctx, const char* prompt) {
    working_memory_t* mem = (working_memory_t*)ctx->working_memory;
    if (!mem || mem->magic != WORKING_MEMORY_MAGIC) {
        /* No working memory, just return prompt */
        return strdup(prompt);
    }

    /* Calculate total size needed */
    size_t total_size = strlen(prompt) + 1;

    if (mem->has_sunset && mem->sunset_offset > 0) {
        total_size += strlen(mem->content + mem->sunset_offset) + 50;
    }
    if (mem->has_apollo && mem->apollo_offset > 0) {
        total_size += strlen(mem->content + mem->apollo_offset) + 50;
    }

    /* Allocate buffer */
    char* context = malloc(total_size);
    if (!context) {
        return NULL;
    }

    /* Build context */
    context[0] = '\0';

    /* Add sunset notes if available */
    if (mem->has_sunset && mem->sunset_offset > 0) {
        strcat(context, "## Previous Session Context\n");
        strcat(context, mem->content + mem->sunset_offset);
        strcat(context, "\n\n");
    }

    /* Add Apollo digest if available */
    if (mem->has_apollo && mem->apollo_offset > 0) {
        strcat(context, "## Memory Digest\n");
        strcat(context, mem->content + mem->apollo_offset);
        strcat(context, "\n\n");
    }

    /* Add current prompt */
    strcat(context, "## Current Task\n");
    strcat(context, prompt);

    return context;
}

/* Stub implementations for remaining functions */
static int write_to_claude(claude_context_t* ctx, const char* input) {
    size_t len = strlen(input);
    ssize_t written = write(ctx->stdin_pipe[1], input, len);
    if (written != (ssize_t)len) {
        return E_SYSTEM_SOCKET;
    }
    write(ctx->stdin_pipe[1], "\n", 1);
    return ARGO_SUCCESS;
}

static int read_from_claude(claude_context_t* ctx, char** output, int timeout_ms) {
    /* Poll for response */
    struct pollfd pfd = {
        .fd = ctx->stdout_pipe[0],
        .events = POLLIN
    };

    /* Simple implementation - real version would accumulate response */
    if (poll(&pfd, 1, timeout_ms) <= 0) {
        return E_CI_TIMEOUT;
    }

    ctx->response_size = 0;
    char buffer[4096];
    ssize_t bytes;

    while ((bytes = read(ctx->stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        /* Grow response buffer if needed */
        if (ctx->response_size + bytes >= ctx->response_capacity) {
            ctx->response_capacity *= 2;
            ctx->response_buffer = realloc(ctx->response_buffer, ctx->response_capacity);
        }
        memcpy(ctx->response_buffer + ctx->response_size, buffer, bytes);
        ctx->response_size += bytes;
    }

    ctx->response_buffer[ctx->response_size] = '\0';
    *output = ctx->response_buffer;
    return ARGO_SUCCESS;
}

static int load_working_memory(claude_context_t* ctx) {
    /* Memory is already mapped, just verify it's valid */
    working_memory_t* mem = (working_memory_t*)ctx->working_memory;
    if (mem->magic == WORKING_MEMORY_MAGIC) {
        return ARGO_SUCCESS;
    }
    return E_INTERNAL_CORRUPT;
}

static int save_working_memory(claude_context_t* ctx) {
    /* Memory is mapped, so changes are automatic */
    /* Just sync to disk */
    if (msync(ctx->working_memory, ctx->memory_size, MS_SYNC) < 0) {
        argo_report_error(E_SYSTEM_FILE, "save_working_memory", "msync() failed: %s", strerror(errno));
        return E_SYSTEM_FILE;
    }
    return ARGO_SUCCESS;
}

static void cleanup_working_memory(claude_context_t* ctx) {
    if (ctx->working_memory && ctx->working_memory != MAP_FAILED) {
        munmap(ctx->working_memory, ctx->memory_size);
    }
    if (ctx->memory_fd >= 0) {
        close(ctx->memory_fd);
    }
}

static bool check_context_limit(claude_context_t* ctx, size_t new_tokens) {
    /* Simple check - real implementation would be more sophisticated */
    return (ctx->tokens_used + new_tokens) > (ctx->context_limit * 0.8);
}

static int trigger_sunset(claude_context_t* ctx) {
    /* Would implement sunset protocol here */
    LOG_INFO("Sunset triggered at %zu tokens", ctx->tokens_used);
    return ARGO_SUCCESS;
}