/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_SESSION_H
#define ARGO_SESSION_H

#include <time.h>
#include <stdbool.h>
#include "argo_orchestrator.h"
#include "argo_registry.h"
#include "argo_memory.h"

/* Session status */
typedef enum {
    SESSION_STATUS_CREATED,
    SESSION_STATUS_ACTIVE,
    SESSION_STATUS_PAUSED,
    SESSION_STATUS_SUNSET,
    SESSION_STATUS_ENDED
} session_status_t;

/* Session constants */
#define SESSION_ID_MAX_LEN 128
#define SESSION_DIR_PATH ".argo/sessions"
#define SESSION_DIR_MODE 0755
#define SESSION_FILE_EXTENSION ".json"
#define SESSION_MAX_PATH 512

/* Session structure */
typedef struct argo_session {
    char id[SESSION_ID_MAX_LEN];
    char project_name[128];
    char base_branch[64];

    session_status_t status;
    time_t created_at;
    time_t started_at;
    time_t ended_at;
    time_t last_activity;

    /* Core components */
    argo_orchestrator_t* orchestrator;
    ci_registry_t* registry;
    ci_memory_digest_t* memory;

    /* Session metadata */
    char working_directory[512];
    int total_tasks_completed;
    int total_ci_messages;
    bool auto_save;

} argo_session_t;

/* Session lifecycle */
argo_session_t* session_create(const char* session_id, const char* project_name,
                               const char* base_branch);
void session_destroy(argo_session_t* session);

/* Session state management */
int session_start(argo_session_t* session);
int session_pause(argo_session_t* session);
int session_resume(argo_session_t* session);
int session_end(argo_session_t* session);

/* Sunset/sunrise protocol */
int session_sunset(argo_session_t* session, const char* notes);
int session_sunrise(argo_session_t* session, const char* brief);

/* Session persistence */
int session_save(argo_session_t* session);
int session_load(argo_session_t* session, const char* session_id);
argo_session_t* session_restore(const char* session_id);

/* Session info */
const char* session_status_string(session_status_t status);
int session_get_uptime(argo_session_t* session);
void session_update_activity(argo_session_t* session);

/* Session file operations */
int session_build_path(char* buffer, size_t size, const char* session_id);
bool session_exists(const char* session_id);
int session_delete(const char* session_id);

/* Session constants - error messages */
#define SESSION_ERR_NULL_SESSION "session is NULL"
#define SESSION_ERR_NULL_ID "session_id is NULL"
#define SESSION_ERR_NULL_PROJECT "project_name is NULL"
#define SESSION_ERR_NULL_BRANCH "base_branch is NULL"
#define SESSION_ERR_INVALID_STATE "invalid session state"
#define SESSION_ERR_NOT_ACTIVE "session is not active"
#define SESSION_ERR_ALREADY_ACTIVE "session is already active"
#define SESSION_ERR_ORCHESTRATOR_FAILED "orchestrator operation failed"
#define SESSION_ERR_SAVE_FAILED "failed to save session"
#define SESSION_ERR_LOAD_FAILED "failed to load session"
#define SESSION_ERR_NOT_FOUND "session not found"
#define SESSION_ERR_PATH_TOO_LONG "session path too long"

/* Session constants - status strings */
#define SESSION_STATUS_STR_CREATED "created"
#define SESSION_STATUS_STR_ACTIVE "active"
#define SESSION_STATUS_STR_PAUSED "paused"
#define SESSION_STATUS_STR_SUNSET "sunset"
#define SESSION_STATUS_STR_ENDED "ended"
#define SESSION_STATUS_STR_UNKNOWN "unknown"

/* Session constants - defaults */
#define SESSION_DEFAULT_MEMORY_SIZE 8000
#define SESSION_AUTO_SAVE_DEFAULT true
#define SESSION_DEFAULT_PROJECT "restored"
#define SESSION_DEFAULT_BRANCH "main"

#endif /* ARGO_SESSION_H */
