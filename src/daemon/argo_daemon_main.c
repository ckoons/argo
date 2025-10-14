/* © 2025 Casey Koons All rights reserved */
/* Argo Daemon - Main entry point */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Project includes */
#include "argo_daemon.h"
#include "argo_error.h"
#include "argo_env_utils.h"
#include "argo_limits.h"
#include "argo_urls.h"
#include "argo_log.h"

/* Localhost address */
#define LOCALHOST_ADDR "127.0.0.1"

/* Global daemon for signal handling */
static argo_daemon_t* g_daemon = NULL;

/* Volatile flag for signal-safe shutdown */
static volatile sig_atomic_t g_shutdown_requested = 0;

/* Kill any existing daemon on this port - NOT SELF */
static void kill_existing_daemon(uint16_t port) {
    /* Try to connect to the port */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return;  /* Can't check, assume port is free */
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(LOCALHOST_ADDR);

    /* Try to connect */
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        /* Port is in use - kill existing daemon using lsof */
        close(sock);

        fprintf(stderr, "Port %d is in use, killing existing daemon...\n", port);

        /* Use lsof to find the PID listening on this port */
        char lsof_cmd[ARGO_BUFFER_SMALL];
        snprintf(lsof_cmd, sizeof(lsof_cmd), "lsof -ti tcp:%d", port);

        FILE* pipe = popen(lsof_cmd, "r"); /* GUIDELINE_APPROVED: popen with validated port number */
        if (pipe) {
            char pid_str[32];
            if (fgets(pid_str, sizeof(pid_str), pipe)) { /* GUIDELINE_APPROVED: fgets in if condition */
                char* endptr = NULL;
                long pid_long = strtol(pid_str, &endptr, 10);
                if (endptr != pid_str && pid_long > 0 && pid_long <= INT_MAX) {
                    kill((pid_t)pid_long, SIGKILL);
                }
            }
            pclose(pipe);
        }

        /* Wait for port to become free */
        sleep(1);

        fprintf(stderr, "Previous daemon killed.\n");
    } else {
        /* Port is free */
        close(sock);
    }
}

/* Signal handler for shutdown - POSIX async-signal-safe */
static void signal_handler(int signum) {
    (void)signum;
    /* ONLY set atomic flag - no fprintf, no function calls */
    g_shutdown_requested = 1;
}

/* NOTE: SIGCHLD handler is installed in argo_daemon_start() */
/* The handler in argo_daemon.c properly reaps children and pushes exit codes to the exit queue */

/* Print usage */
static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [OPTIONS]\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --port PORT    Listen on PORT (default: %d or ARGO_DAEMON_PORT env)\n", DEFAULT_DAEMON_PORT);
    fprintf(stderr, "  --help         Show this help message\n");
    fprintf(stderr, "\n");
}

/* Helper: Log startup diagnostics */
static void log_startup_info(void) {
    char cwd[ARGO_PATH_MAX];

    fprintf(stderr, "=== Argo Daemon Starting ===\n");

    if (getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "Current directory: %s\n", cwd);
    } else {
        fprintf(stderr, "WARNING: Could not get current directory\n");
    }

    fprintf(stderr, "Environment variables:\n");
    fprintf(stderr, "  ARGO_DAEMON_PORT = %s\n", getenv("ARGO_DAEMON_PORT") ? getenv("ARGO_DAEMON_PORT") : "(not set)");
    fprintf(stderr, "  ARC_ENV = %s\n", getenv("ARC_ENV") ? getenv("ARC_ENV") : "(not set)");
    fprintf(stderr, "  HOME = %s\n", getenv("HOME") ? getenv("HOME") : "(not set)");
    fprintf(stderr, "  PWD = %s\n", getenv("PWD") ? getenv("PWD") : "(not set)");
}

/* Helper: Parse port from environment and arguments */
static uint16_t parse_port_config(int argc, char** argv) {
    uint16_t port = DEFAULT_DAEMON_PORT;

    /* Check environment variable */
    const char* env_port = getenv("ARGO_DAEMON_PORT");
    if (env_port) {
        char* endptr = NULL;
        long p = strtol(env_port, &endptr, 10);
        if (endptr != env_port && p > 0 && p <= MAX_TCP_PORT) {
            port = (uint16_t)p;
            fprintf(stderr, "Using port from ARGO_DAEMON_PORT: %d\n", port);
        }
    }

    /* Parse command-line arguments (override env) */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                char* endptr = NULL;
                long p = strtol(argv[++i], &endptr, 10);
                if (endptr != argv[i] && *endptr == '\0' && p > 0 && p <= MAX_TCP_PORT) {
                    port = (uint16_t)p;
                    fprintf(stderr, "Using port from --port argument: %d\n", port);
                } else {
                    fprintf(stderr, "Error: Invalid port: %s\n", argv[i]);
                    exit(1);
                }
            } else {
                fprintf(stderr, "Error: --port requires an argument\n");
                print_usage(argv[0]);
                exit(1);
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            exit(1);
        }
    }

    return port;
}

/* Helper: Initialize directories and logging */
static int init_directories_and_logging(void) {
    const char* home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Error: HOME environment variable not set\n");
        return -1;
    }

    char argo_dir[ARGO_PATH_MAX];
    snprintf(argo_dir, sizeof(argo_dir), "%s/.argo", home);
    mkdir(argo_dir, ARGO_DIR_PERMISSIONS);

    char logs_dir[ARGO_PATH_MAX];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.argo/logs", home);
    mkdir(logs_dir, ARGO_DIR_PERMISSIONS);

    /* Initialize logging system */
    int log_result = log_init(logs_dir);
    if (log_result != ARGO_SUCCESS) {
        fprintf(stderr, "Warning: Failed to initialize logging to %s (error %d)\n", logs_dir, log_result);
        fprintf(stderr, "Continuing without file logging...\n");
    }
    log_set_level(LOG_DEBUG);
    LOG_DEBUG("Daemon debug logging enabled (PID %d)", getpid());

    return 0;
}

int main(int argc, char** argv) {
    /* Log startup diagnostics */
    log_startup_info();

    /* Parse port configuration */
    uint16_t port = parse_port_config(argc, argv);

    fprintf(stderr, "Final port: %d\n", port);
    fprintf(stderr, "============================\n");
    fflush(stderr);

    /* Initialize directories and logging */
    if (init_directories_and_logging() != 0) {
        return 1;
    }

    /* Kill any existing daemon on this port */
    kill_existing_daemon(port);

    /* Create daemon */
    g_daemon = argo_daemon_create(port);
    if (!g_daemon) {
        fprintf(stderr, "Failed to create daemon\n");
        return 1;
    }

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    /* SIGCHLD handler installed by argo_daemon_start() */

    /* Start daemon (blocks until stopped) */
    fprintf(stderr, "Starting daemon on port %d...\n", port);
    fflush(stderr);
    int result = argo_daemon_start(g_daemon);

    /* Cleanup */
    fprintf(stderr, "Daemon stopping...\n");
    argo_daemon_destroy(g_daemon);
    g_daemon = NULL;

    return (result == ARGO_SUCCESS) ? 0 : 1;
}
