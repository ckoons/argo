/* © 2025 Casey Koons All rights reserved */
/* Argo Daemon - Main entry point */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Project includes */
#include "argo_daemon.h"
#include "argo_error.h"
#include "argo_env_utils.h"
#include "argo_limits.h"
#include "argo_urls.h"

/* Global daemon for signal handling */
static argo_daemon_t* g_daemon = NULL;

/* Kill any existing daemon - idempotent takeover */
static void kill_existing_daemon(uint16_t port) {
    (void)port;  /* Port displayed in main() output */

    /* Blind kill - idempotent */
    system("pkill -9 argo-daemon 2>/dev/null");
    sleep(1);
}

/* Signal handler */
static void signal_handler(int signum) {
    (void)signum;
    printf("\nReceived shutdown signal\n");
    if (g_daemon) {
        argo_daemon_stop(g_daemon);
    }
}

/* Print usage */
static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [OPTIONS]\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --port PORT    Listen on PORT (default: %d or ARGO_DAEMON_PORT env)\n", DEFAULT_DAEMON_PORT);
    fprintf(stderr, "  --help         Show this help message\n");
    fprintf(stderr, "\n");
}

int main(int argc, char** argv) {
    uint16_t port = DEFAULT_DAEMON_PORT;
    char cwd[ARGO_PATH_MAX];

    /* Log startup - do this FIRST before any crashes */
    fprintf(stderr, "=== Argo Daemon Starting ===\n");

    /* Get and log current directory */
    if (getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "Current directory: %s\n", cwd);
    } else {
        fprintf(stderr, "WARNING: Could not get current directory\n");
    }

    /* Log all relevant environment variables */
    fprintf(stderr, "Environment variables:\n");
    fprintf(stderr, "  ARGO_DAEMON_PORT = %s\n", getenv("ARGO_DAEMON_PORT") ? getenv("ARGO_DAEMON_PORT") : "(not set)");
    fprintf(stderr, "  ARC_ENV = %s\n", getenv("ARC_ENV") ? getenv("ARC_ENV") : "(not set)");
    fprintf(stderr, "  HOME = %s\n", getenv("HOME") ? getenv("HOME") : "(not set)");
    fprintf(stderr, "  PWD = %s\n", getenv("PWD") ? getenv("PWD") : "(not set)");

    /* Check environment variable */
    const char* env_port = getenv("ARGO_DAEMON_PORT");
    if (env_port) {
        int p = atoi(env_port);
        if (p > 0 && p <= MAX_TCP_PORT) {
            port = (uint16_t)p;
            fprintf(stderr, "Using port from ARGO_DAEMON_PORT: %d\n", port);
        }
    }

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                int p = atoi(argv[++i]);
                if (p > 0 && p <= MAX_TCP_PORT) {
                    port = (uint16_t)p;
                    fprintf(stderr, "Using port from --port argument: %d\n", port);
                } else {
                    fprintf(stderr, "Error: Invalid port: %s\n", argv[i]);
                    return 1;
                }
            } else {
                fprintf(stderr, "Error: --port requires an argument\n");
                print_usage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    fprintf(stderr, "Final port: %d\n", port);
    fprintf(stderr, "============================\n");
    fflush(stderr);

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
