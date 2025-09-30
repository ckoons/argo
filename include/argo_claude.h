/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_CLAUDE_H
#define ARGO_CLAUDE_H

#include "argo_ci.h"

/* Claude configuration */
#define CLAUDE_COMMAND "claude"
#define CLAUDE_TIMEOUT_MS 60000
#define CLAUDE_SESSION_DIR ".argo/sessions"
#define CLAUDE_SESSION_PATH ".argo/sessions/%s.mmap"

/* Claude Code configuration */
#define CLAUDE_CODE_SESSION_ID_SIZE 64
#define CLAUDE_CODE_PATH_SIZE 256
#define CLAUDE_CODE_MAX_CONTEXT 200000
#define CLAUDE_CODE_RESPONSE_CAPACITY 65536
#define CLAUDE_CODE_TIMEOUT_SECONDS 300
#define CLAUDE_CODE_CHUNK_BUFFER_SIZE 1024
#define CLAUDE_CODE_MIN_FILE_SIZE 100
#define CLAUDE_CODE_COMPLETION_CHECK_DELAY 2

/* Claude provider creation */
ci_provider_t* claude_create_provider(const char* ci_name);

/* Claude Code prompt mode provider */
ci_provider_t* claude_code_create_provider(const char* ci_name);

/* Claude-specific functions */
bool claude_is_available(void);
bool claude_code_is_available(void);
int claude_get_session_info(const char* ci_name, char** info);
int claude_clear_session(const char* ci_name);

/* Working memory access (for testing/debugging) */
typedef struct claude_working_memory claude_working_memory_t;
claude_working_memory_t* claude_get_working_memory(ci_provider_t* provider);
size_t claude_get_memory_usage(ci_provider_t* provider);
int claude_set_sunset_notes(ci_provider_t* provider, const char* notes);

#endif /* ARGO_CLAUDE_H */