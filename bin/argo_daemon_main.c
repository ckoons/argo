/* © 2025 Casey Koons All rights reserved */
/* Argo Daemon - Main entry point */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

/* Project includes */
#include "argo_daemon.h"
#include "argo_error.h"
#include "argo_env_utils.h"

/* Default port */
#define DEFAULT_DAEMON_PORT 9876

/* Global daemon for signal handling */
static argo_daemon_t* g_daemon = NULL;

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

    /* Check environment variable */
    const char* env_port = getenv("ARGO_DAEMON_PORT");
    if (env_port) {
        int p = atoi(env_port);
        if (p > 0 && p < 65536) {
            port = (uint16_t)p;
        }
    }

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                int p = atoi(argv[++i]);
                if (p > 0 && p < 65536) {
                    port = (uint16_t)p;
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
    int result = argo_daemon_start(g_daemon);

    /* Cleanup */
    argo_daemon_destroy(g_daemon);
    g_daemon = NULL;

    return (result == ARGO_SUCCESS) ? 0 : 1;
}
