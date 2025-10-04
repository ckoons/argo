/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

/* Project includes */
#include "argo_init.h"
#include "argo_workflow.h"
#include "argo_orchestrator.h"
#include "argo_error.h"
#include "argo_log.h"

/* Signal handling state */
static volatile sig_atomic_t pause_requested = 0;
static volatile sig_atomic_t shutdown_requested = 0;

/* Signal handlers */
static void handle_pause(int sig) {
    (void)sig;
    pause_requested = 1;
}

static void handle_resume(int sig) {
    (void)sig;
    pause_requested = 0;
}

static void handle_shutdown(int sig) {
    (void)sig;
    shutdown_requested = 1;
}

/* Setup signal handlers for workflow control */
static void setup_signal_handlers(void) {
    struct sigaction sa_pause;
    struct sigaction sa_resume;
    struct sigaction sa_shutdown;

    /* SIGUSR1: Pause at next checkpoint */
    memset(&sa_pause, 0, sizeof(sa_pause));
    sa_pause.sa_handler = handle_pause;
    sigemptyset(&sa_pause.sa_mask);
    sa_pause.sa_flags = 0;
    sigaction(SIGUSR1, &sa_pause, NULL);

    /* SIGUSR2: Resume execution */
    memset(&sa_resume, 0, sizeof(sa_resume));
    sa_resume.sa_handler = handle_resume;
    sigemptyset(&sa_resume.sa_mask);
    sa_resume.sa_flags = 0;
    sigaction(SIGUSR2, &sa_resume, NULL);

    /* SIGTERM: Graceful shutdown */
    memset(&sa_shutdown, 0, sizeof(sa_shutdown));
    sa_shutdown.sa_handler = handle_shutdown;
    sigemptyset(&sa_shutdown.sa_mask);
    sa_shutdown.sa_flags = 0;
    sigaction(SIGTERM, &sa_shutdown, NULL);
}

/* Check if paused and wait for resume */
static void check_pause_state(const char* workflow_id) {
    while (pause_requested && !shutdown_requested) {
        LOG_INFO("Workflow %s paused, waiting for resume signal", workflow_id);
        sleep(1);
    }
}

/* Main workflow executor */
int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <workflow_id> <template_path> <branch>\n", argv[0]);
        return 1;
    }

    const char* workflow_id = argv[1];
    const char* template_path = argv[2];
    const char* branch = argv[3];

    /* Setup signal handlers */
    setup_signal_handlers();

    fprintf(stdout, "========================================\n");
    fprintf(stdout, "Workflow Executor\n");
    fprintf(stdout, "========================================\n");
    fprintf(stdout, "Workflow ID: %s\n", workflow_id);
    fprintf(stdout, "Template:    %s\n", template_path);
    fprintf(stdout, "Branch:      %s\n", branch);
    fprintf(stdout, "PID:         %d\n", getpid());
    fprintf(stdout, "========================================\n\n");

    /* Simulate workflow execution with signal handling */
    fprintf(stdout, "Starting workflow execution...\n");

    int step = 1;
    int total_steps = 5;

    while (step <= total_steps && !shutdown_requested) {
        /* Check if paused */
        check_pause_state(workflow_id);

        if (shutdown_requested) {
            break;
        }

        fprintf(stdout, "Executing step %d/%d...\n", step, total_steps);
        sleep(2);  /* Simulate work */

        step++;
    }

    if (shutdown_requested) {
        fprintf(stdout, "\n========================================\n");
        fprintf(stdout, "Workflow %s TERMINATED by signal\n", workflow_id);
        fprintf(stdout, "Completed %d/%d steps\n", step - 1, total_steps);
        fprintf(stdout, "========================================\n");
        return 2;
    }

    fprintf(stdout, "\n========================================\n");
    fprintf(stdout, "Workflow %s COMPLETED successfully\n", workflow_id);
    fprintf(stdout, "All %d steps executed\n", total_steps);
    fprintf(stdout, "========================================\n");

    return 0;
}
