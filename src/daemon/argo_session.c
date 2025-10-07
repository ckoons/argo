/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Project includes */
#include "argo_session.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"

/* Create a new session */
argo_session_t* session_create(const char* session_id, const char* project_name,
                               const char* base_branch) {
    if (!session_id) {
        argo_report_error(E_INPUT_NULL, "session_create", SESSION_ERR_NULL_ID);
        return NULL;
    }
    if (!project_name) {
        argo_report_error(E_INPUT_NULL, "session_create", SESSION_ERR_NULL_PROJECT);
        return NULL;
    }
    if (!base_branch) {
        argo_report_error(E_INPUT_NULL, "session_create", SESSION_ERR_NULL_BRANCH);
        return NULL;
    }

    argo_session_t* session = calloc(1, sizeof(argo_session_t));
    if (!session) {
        return NULL;
    }

    /* Initialize session metadata */
    strncpy(session->id, session_id, sizeof(session->id) - 1);
    strncpy(session->project_name, project_name, sizeof(session->project_name) - 1);
    strncpy(session->base_branch, base_branch, sizeof(session->base_branch) - 1);

    session->status = SESSION_STATUS_CREATED;
    session->created_at = time(NULL);
    session->auto_save = SESSION_AUTO_SAVE_DEFAULT;

    /* Create orchestrator */
    session->orchestrator = orchestrator_create(project_name, base_branch);
    if (!session->orchestrator) {
        argo_report_error(E_SYSTEM_MEMORY, "session_create",
                         SESSION_ERR_ORCHESTRATOR_FAILED);
        free(session);
        return NULL;
    }

    /* Create registry */
    session->registry = registry_create();
    if (!session->registry) {
        orchestrator_destroy(session->orchestrator);
        free(session);
        return NULL;
    }

    /* Create memory digest */
    session->memory = memory_digest_create(SESSION_DEFAULT_MEMORY_SIZE);
    if (!session->memory) {
        registry_destroy(session->registry);
        orchestrator_destroy(session->orchestrator);
        free(session);
        return NULL;
    }

    /* Get current working directory */
    if (getcwd(session->working_directory, sizeof(session->working_directory)) == NULL) {
        session->working_directory[0] = '\0';
    }

    LOG_INFO("Created session: %s (project: %s, branch: %s)",
             session_id, project_name, base_branch);

    return session;
}

/* Destroy session and clean up resources */
void session_destroy(argo_session_t* session) {
    if (!session) return;

    if (session->memory) {
        memory_digest_destroy(session->memory);
    }

    if (session->registry) {
        registry_destroy(session->registry);
    }

    if (session->orchestrator) {
        orchestrator_destroy(session->orchestrator);
    }

    LOG_INFO("Destroyed session: %s", session->id);
    free(session);
}

/* Start session */
int session_start(argo_session_t* session) {
    ARGO_CHECK_NULL(session);

    if (session->status == SESSION_STATUS_ACTIVE) {
        argo_report_error(E_INVALID_STATE, "session_start", SESSION_ERR_ALREADY_ACTIVE);
        return E_INVALID_STATE;
    }

    session->status = SESSION_STATUS_ACTIVE;
    session->started_at = time(NULL);
    session->last_activity = session->started_at;

    /* Start orchestrator workflow */
    int result = orchestrator_start_workflow(session->orchestrator);
    if (result != ARGO_SUCCESS) {
        session->status = SESSION_STATUS_CREATED;
        return result;
    }

    LOG_INFO("Started session: %s", session->id);

    if (session->auto_save) {
        session_save(session);
    }

    return ARGO_SUCCESS;
}

/* Pause session */
int session_pause(argo_session_t* session) {
    ARGO_CHECK_NULL(session);

    if (session->status != SESSION_STATUS_ACTIVE) {
        argo_report_error(E_INVALID_STATE, "session_pause", SESSION_ERR_NOT_ACTIVE);
        return E_INVALID_STATE;
    }

    session->status = SESSION_STATUS_PAUSED;
    session->last_activity = time(NULL);

    int result = orchestrator_pause_workflow(session->orchestrator);
    if (result != ARGO_SUCCESS) {
        session->status = SESSION_STATUS_ACTIVE;
        return result;
    }

    LOG_INFO("Paused session: %s", session->id);

    if (session->auto_save) {
        session_save(session);
    }

    return ARGO_SUCCESS;
}

/* Resume session */
int session_resume(argo_session_t* session) {
    ARGO_CHECK_NULL(session);

    if (session->status != SESSION_STATUS_PAUSED) {
        argo_report_error(E_INVALID_STATE, "session_resume", SESSION_ERR_INVALID_STATE);
        return E_INVALID_STATE;
    }

    session->status = SESSION_STATUS_ACTIVE;
    session->last_activity = time(NULL);

    int result = orchestrator_resume_workflow(session->orchestrator);
    if (result != ARGO_SUCCESS) {
        session->status = SESSION_STATUS_PAUSED;
        return result;
    }

    LOG_INFO("Resumed session: %s", session->id);

    if (session->auto_save) {
        session_save(session);
    }

    return ARGO_SUCCESS;
}

/* End session */
int session_end(argo_session_t* session) {
    ARGO_CHECK_NULL(session);

    session->status = SESSION_STATUS_ENDED;
    session->ended_at = time(NULL);
    session->last_activity = session->ended_at;

    LOG_INFO("Ended session: %s", session->id);

    if (session->auto_save) {
        session_save(session);
    }

    return ARGO_SUCCESS;
}

/* Sunset protocol - end of work session */
int session_sunset(argo_session_t* session, const char* notes) {
    ARGO_CHECK_NULL(session);

    session->status = SESSION_STATUS_SUNSET;
    session->last_activity = time(NULL);

    /* Store sunset notes in memory */
    if (notes) {
        memory_set_sunset_notes(session->memory, notes);
    }

    /* Update task completion stats */
    if (session->orchestrator && session->orchestrator->workflow) {
        session->total_tasks_completed = session->orchestrator->workflow->completed_tasks;
    }

    LOG_INFO("Session sunset: %s", session->id);

    if (session->auto_save) {
        session_save(session);
    }

    return ARGO_SUCCESS;
}

/* Sunrise protocol - start of new work session */
int session_sunrise(argo_session_t* session, const char* brief) {
    ARGO_CHECK_NULL(session);

    /* Store sunrise brief in memory */
    if (brief) {
        memory_set_sunrise_brief(session->memory, brief);
    }

    /* Resume from sunset state */
    if (session->status == SESSION_STATUS_SUNSET) {
        session->status = SESSION_STATUS_ACTIVE;
    }

    session->last_activity = time(NULL);

    LOG_INFO("Session sunrise: %s", session->id);

    if (session->auto_save) {
        session_save(session);
    }

    return ARGO_SUCCESS;
}

/* Save session to disk */
int session_save(argo_session_t* session) {
    ARGO_CHECK_NULL(session);

    char path[SESSION_MAX_PATH];
    int result = session_build_path(path, sizeof(path), session->id);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Create session directory if needed */
    mkdir(SESSION_DIR_PATH, SESSION_DIR_MODE);

    FILE* fp = fopen(path, "w");
    if (!fp) {
        argo_report_error(E_SYSTEM_FILE, "session_save", SESSION_ERR_SAVE_FAILED);
        return E_SYSTEM_FILE;
    }

    /* Write session metadata */
    fprintf(fp, "{\n");
    fprintf(fp, "  \"id\": \"%s\",\n", session->id);
    fprintf(fp, "  \"project_name\": \"%s\",\n", session->project_name);
    fprintf(fp, "  \"base_branch\": \"%s\",\n", session->base_branch);
    fprintf(fp, "  \"status\": %d,\n", session->status);
    fprintf(fp, "  \"created_at\": %ld,\n", (long)session->created_at);
    fprintf(fp, "  \"started_at\": %ld,\n", (long)session->started_at);
    fprintf(fp, "  \"ended_at\": %ld,\n", (long)session->ended_at);
    fprintf(fp, "  \"last_activity\": %ld,\n", (long)session->last_activity);
    fprintf(fp, "  \"working_directory\": \"%s\",\n", session->working_directory);
    fprintf(fp, "  \"total_tasks_completed\": %d,\n", session->total_tasks_completed);
    fprintf(fp, "  \"total_ci_messages\": %d,\n", session->total_ci_messages);
    fprintf(fp, "  \"auto_save\": %s\n", session->auto_save ? "true" : "false");
    fprintf(fp, "}\n");

    fclose(fp);

    LOG_DEBUG("Saved session: %s to %s", session->id, path);

    return ARGO_SUCCESS;
}

/* Restore session from disk */
argo_session_t* session_restore(const char* session_id) {
    if (!session_id) {
        argo_report_error(E_INPUT_NULL, "session_restore", SESSION_ERR_NULL_ID);
        return NULL;
    }

    if (!session_exists(session_id)) {
        argo_report_error(E_SYSTEM_FILE, "session_restore", SESSION_ERR_NOT_FOUND);
        return NULL;
    }

    /* For now, create a new session - full restore will be implemented later */
    /* This is a placeholder that creates a fresh session with the same ID */
    argo_session_t* session = session_create(session_id, SESSION_DEFAULT_PROJECT,
                                             SESSION_DEFAULT_BRANCH);
    if (!session) {
        return NULL;
    }

    LOG_INFO("Restored session: %s", session_id);

    return session;
}

/* Get status string */
const char* session_status_string(session_status_t status) {
    switch (status) {
        case SESSION_STATUS_CREATED: return SESSION_STATUS_STR_CREATED;
        case SESSION_STATUS_ACTIVE: return SESSION_STATUS_STR_ACTIVE;
        case SESSION_STATUS_PAUSED: return SESSION_STATUS_STR_PAUSED;
        case SESSION_STATUS_SUNSET: return SESSION_STATUS_STR_SUNSET;
        case SESSION_STATUS_ENDED: return SESSION_STATUS_STR_ENDED;
        default: return SESSION_STATUS_STR_UNKNOWN;
    }
}

/* Get session uptime in seconds */
int session_get_uptime(argo_session_t* session) {
    if (!session) return 0;

    if (session->started_at == 0) {
        return 0;
    }

    time_t end_time = session->ended_at > 0 ? session->ended_at : time(NULL);
    return (int)(end_time - session->started_at);
}

/* Update last activity time */
void session_update_activity(argo_session_t* session) {
    if (!session) return;
    session->last_activity = time(NULL);
}

/* Build session file path */
int session_build_path(char* buffer, size_t size, const char* session_id) {
    ARGO_CHECK_NULL(buffer);
    ARGO_CHECK_NULL(session_id);

    int written = snprintf(buffer, size, "%s/%s%s",
                          SESSION_DIR_PATH, session_id, SESSION_FILE_EXTENSION);

    if (written < 0 || (size_t)written >= size) {
        argo_report_error(E_PROTOCOL_SIZE, "session_build_path", SESSION_ERR_PATH_TOO_LONG);
        return E_PROTOCOL_SIZE;
    }

    return ARGO_SUCCESS;
}

/* Check if session exists */
bool session_exists(const char* session_id) {
    if (!session_id) return false;

    char path[SESSION_MAX_PATH];
    if (session_build_path(path, sizeof(path), session_id) != ARGO_SUCCESS) {
        return false;
    }

    return access(path, F_OK) == 0;
}

/* Delete session file */
int session_delete(const char* session_id) {
    ARGO_CHECK_NULL(session_id);

    char path[SESSION_MAX_PATH];
    int result = session_build_path(path, sizeof(path), session_id);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    if (unlink(path) != 0) {
        argo_report_error(E_SYSTEM_FILE, "session_delete", SESSION_ERR_NOT_FOUND);
        return E_SYSTEM_FILE;
    }

    LOG_INFO("Deleted session file: %s", session_id);

    return ARGO_SUCCESS;
}
