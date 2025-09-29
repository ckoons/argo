/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_LOG_H
#define ARGO_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

/* Log levels */
typedef enum log_level {
    LOG_FATAL = 0,              /* System will die */
    LOG_ERROR = 1,              /* Operation failed */
    LOG_WARN  = 2,              /* Concerning but continuing */
    LOG_INFO  = 3,              /* Normal operations */
    LOG_DEBUG = 4,              /* Detailed debugging */
    LOG_TRACE = 5               /* Everything including messages */
} log_level_t;

/* Default log settings */
#define LOG_DEFAULT_DIR ".argo/logs"
#define LOG_DEFAULT_FILE "argo.log"
#define LOG_MAX_MESSAGE 4096
#define LOG_TIME_FORMAT "%Y-%m-%d %H:%M:%S"
#define LOG_DAILY_FORMAT "argo-%Y-%m-%d.log"

/* Log configuration */
typedef struct log_config {
    bool enabled;               /* Global on/off switch */
    log_level_t level;         /* Current log level */
    char* log_dir;             /* Log directory path */
    char* log_file;            /* Current log filename */
    FILE* log_fp;              /* Log file handle */
    bool use_stdout;           /* Also log to stdout */
    bool use_stderr;           /* Errors to stderr */
    bool daily_rotate;         /* Rotate logs daily */
    time_t last_rotate;        /* Last rotation time */
    size_t max_file_size;      /* Max size before rotation */
    int max_files;             /* Max number of log files */
} log_config_t;

/* Global log configuration */
extern log_config_t* g_log_config;

/* Log initialization and cleanup */
int log_init(const char* log_dir);
int log_init_with_config(log_config_t* config);
void log_cleanup(void);

/* Log control */
void log_enable(bool enable);
void log_set_level(log_level_t level);
void log_set_location(const char* log_dir);
void log_set_file(const char* filename);
void log_set_stdout(bool enable);
void log_set_stderr(bool enable);

/* Core logging functions */
void log_write(log_level_t level,
              const char* file,
              int line,
              const char* func,
              const char* fmt, ...);

void log_write_v(log_level_t level,
                const char* file,
                int line,
                const char* func,
                const char* fmt,
                va_list args);

/* Convenience macros */
#define LOG(level, fmt, ...) \
    log_write(level, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_FATAL(fmt, ...) LOG(LOG_FATAL, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(LOG_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG(LOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) LOG(LOG_TRACE, fmt, ##__VA_ARGS__)

/* Specialized logging */
void log_error_code(int error_code, const char* context);
void log_ci_message(const char* from, const char* to, const char* message);
void log_memory_digest(const char* ci_name, size_t digest_size);
void log_performance(const char* operation, double elapsed_ms);

/* Log rotation */
int log_rotate(void);
int log_rotate_daily(void);
int log_cleanup_old_files(int keep_days);

/* Log utilities */
const char* log_level_string(log_level_t level);
log_level_t log_level_from_string(const char* str);
bool log_is_enabled(void);
log_level_t log_get_level(void);
const char* log_get_location(void);

/* Log entry structure for parsing */
typedef struct log_entry {
    time_t timestamp;
    log_level_t level;
    char* file;
    int line;
    char* function;
    char* message;
} log_entry_t;

/* Log parsing (for analysis) */
log_entry_t* log_parse_line(const char* line);
void log_free_entry(log_entry_t* entry);

/* Log statistics */
typedef struct log_stats {
    uint64_t total_entries;
    uint64_t fatal_count;
    uint64_t error_count;
    uint64_t warn_count;
    uint64_t info_count;
    uint64_t debug_count;
    uint64_t trace_count;
    size_t current_file_size;
    int file_count;
} log_stats_t;

log_stats_t* log_get_stats(void);
void log_free_stats(log_stats_t* stats);

/* Hex dump for debugging */
void log_hex_dump(log_level_t level,
                 const char* prefix,
                 const void* data,
                 size_t size);

/* Assert with logging */
#define LOG_ASSERT(cond, fmt, ...) \
    do { if (!(cond)) { \
        LOG_FATAL("Assertion failed: " #cond " - " fmt, ##__VA_ARGS__); \
        abort(); \
    }} while(0)

#endif /* ARGO_LOG_H */