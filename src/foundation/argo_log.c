/* © 2025 Casey Koons All rights reserved */
/* Logging system - writes to files for background processes */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

/* Project includes */
#include "argo_log.h"
#include "argo_limits.h"
#include "argo_error.h"

/* Global log configuration */
log_config_t* g_log_config = NULL;

/* Initialize logging system */
int log_init(const char* log_dir) {
    if (g_log_config) {
        return ARGO_SUCCESS;  /* Already initialized */
    }

    g_log_config = calloc(1, sizeof(log_config_t));
    if (!g_log_config) {
        return E_SYSTEM_MEMORY;
    }

    /* Create log directory if needed */
    const char* dir = log_dir ? log_dir : LOG_DEFAULT_DIR;
    mkdir(dir, ARGO_DIR_PERMISSIONS);

    /* Allocate log directory path */
    g_log_config->log_dir = strdup(dir);
    if (!g_log_config->log_dir) {
        free(g_log_config);
        g_log_config = NULL;
        return E_SYSTEM_MEMORY;
    }

    /* Build log filename with PID */
    char filename[ARGO_BUFFER_SMALL];
    pid_t pid = getpid();

    /* Read process name from /proc or argv to determine if daemon/executor */
    /* For simplicity, check if we're in a daemon context by looking for "daemon" in any way */
    /* This is a heuristic - proper solution would pass process type to log_init */
    snprintf(filename, sizeof(filename), "argo_process_%d.log", pid);

    size_t path_len = strlen(dir) + strlen(filename) + 2;
    g_log_config->log_file = malloc(path_len);
    if (!g_log_config->log_file) {
        free(g_log_config->log_dir);
        free(g_log_config);
        g_log_config = NULL;
        return E_SYSTEM_MEMORY;
    }

    snprintf(g_log_config->log_file, path_len, "%s/%s", dir, filename);

    /* Open log file */
    g_log_config->log_fp = fopen(g_log_config->log_file, "a");
    if (!g_log_config->log_fp) {
        free(g_log_config->log_file);
        free(g_log_config->log_dir);
        free(g_log_config);
        g_log_config = NULL;
        return E_SYSTEM_FILE;
    }

    /* Set unbuffered for immediate writes */
    setbuf(g_log_config->log_fp, NULL);

    /* Default configuration */
    g_log_config->enabled = true;
    g_log_config->level = LOG_INFO;
    g_log_config->use_stdout = false;  /* Background processes don't use stdout */
    g_log_config->use_stderr = false;  /* Background processes don't use stderr */

    /* Write initialization message */
    fprintf(g_log_config->log_fp, "\n=== Log initialized: %s (PID %d) ===\n",
            g_log_config->log_file, pid);

    return ARGO_SUCCESS;
}

/* Cleanup logging system */
void log_cleanup(void) {
    if (!g_log_config) return;

    if (g_log_config->log_fp) {
        fprintf(g_log_config->log_fp, "=== Log closed ===\n");
        fclose(g_log_config->log_fp);
    }

    free(g_log_config->log_file);
    free(g_log_config->log_dir);
    free(g_log_config);
    g_log_config = NULL;
}

/* Set log level */
void log_set_level(log_level_t level) {
    if (g_log_config) {
        g_log_config->level = level;
    }
}

/* Get log level string */
const char* log_level_string(log_level_t level) {
    switch (level) {
        case LOG_FATAL: return "FATAL";
        case LOG_ERROR: return "ERROR";
        case LOG_WARN:  return "WARN ";
        case LOG_INFO:  return "INFO ";
        case LOG_DEBUG: return "DEBUG";
        case LOG_TRACE: return "TRACE";
        default:        return "?????";
    }
}

/* Core logging function */
void log_write(log_level_t level, const char* file, int line,
              const char* func, const char* fmt, ...) {
    if (!g_log_config || !g_log_config->enabled) {
        return;
    }

    if (level > g_log_config->level) {
        return;  /* Below current log level */
    }

    if (!g_log_config->log_fp) {
        return;  /* No log file */
    }

    /* Get timestamp */
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    /* Extract just the filename (not full path) */
    const char* filename = strrchr(file, '/');
    if (filename) {
        filename++;  /* Skip the '/' */
    } else {
        filename = file;
    }

    /* Write log header */
    fprintf(g_log_config->log_fp, "[%s] %s %s:%d (%s): ",
            timestamp, log_level_string(level), filename, line, func);

    /* Write log message */
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_config->log_fp, fmt, args);
    va_end(args);

    fprintf(g_log_config->log_fp, "\n");
}

/* Get log level */
log_level_t log_get_level(void) {
    if (!g_log_config) return LOG_INFO;
    return g_log_config->level;
}

/* Check if logging is enabled */
bool log_is_enabled(void) {
    return g_log_config && g_log_config->enabled;
}

/* Get log location */
const char* log_get_location(void) {
    if (!g_log_config) return NULL;
    return g_log_config->log_dir;
}
