/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include "arc_commands.h"
#include "arc_http_client.h"
#include "arc_constants.h"
#include "argo_output.h"
#include "argo_error.h"

/* Global flag for signal handling */
static volatile sig_atomic_t g_running = 1;

/* Signal handler for SIGINT (Ctrl+C) */
static void handle_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

/* Internal attach implementation with seek control */
static int arc_workflow_attach_internal(const char* workflow_id, int seek_to_end) {
    if (!workflow_id) {
        LOG_USER_ERROR("workflow ID required\n");
        return ARC_EXIT_ERROR;
    }

    /* Verify workflow exists */
    char endpoint[512];
    snprintf(endpoint, sizeof(endpoint), "/api/workflow/status/%s", workflow_id);

    arc_http_response_t* response = NULL;
    int result = arc_http_get(endpoint, &response);
    if (result != ARGO_SUCCESS || !response) {
        LOG_USER_ERROR("Failed to connect to daemon\n");
        if (response) arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    }

    if (response->status_code == 404) {
        LOG_USER_ERROR("Workflow not found: %s\n", workflow_id);
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    } else if (response->status_code != 200) {
        LOG_USER_ERROR("Failed to get workflow status (HTTP %d)\n", response->status_code);
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    }

    arc_http_response_free(response);

    /* Build log file path */
    const char* home = getenv("HOME");
    if (!home) home = ".";

    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/.argo/logs/%s.log", home, workflow_id);

    /* Open log file */
    int log_fd = open(log_path, O_RDONLY | O_NONBLOCK);
    if (log_fd < 0) {
        LOG_USER_ERROR("Failed to open log file: %s\n", log_path);
        LOG_USER_INFO("  Error: %s\n", strerror(errno));
        return ARC_EXIT_ERROR;
    }

    /* Seek to end of file if requested (for manual attach, not auto-attach) */
    if (seek_to_end) {
        lseek(log_fd, 0, SEEK_END);
    }

    /* Setup signal handler for Ctrl+C */
    signal(SIGINT, handle_sigint);

    LOG_USER_SUCCESS("Attached to workflow: %s\n", workflow_id);
    LOG_USER_INFO("Logs: %s\n", log_path);
    LOG_USER_INFO("Press Ctrl+C to detach\n");
    printf("----------------------------------------\n");

    /* Main loop: tail log file and check for input */
    char buffer[4096];
    while (g_running) {
        /* Read from log file */
        ssize_t bytes_read = read(log_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("%s", buffer);
            fflush(stdout);
        }

        /* Check if stdin has data available (non-blocking) */
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  /* 100ms timeout */

        int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
        if (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            /* Read line from stdin */
            char input_line[1024];
            if (fgets(input_line, sizeof(input_line), stdin)) {
                /* Send input to workflow via HTTP */
                char json_body[2048];
                snprintf(json_body, sizeof(json_body), "{\"input\":\"%s\"}", input_line);

                snprintf(endpoint, sizeof(endpoint), "/api/workflow/input/%s", workflow_id);

                arc_http_response_t* input_resp = NULL;
                result = arc_http_post(endpoint, json_body, &input_resp);

                if (result != ARGO_SUCCESS || !input_resp || input_resp->status_code != 200) {
                    LOG_USER_WARN("Failed to send input to workflow\n");
                    if (input_resp) arc_http_response_free(input_resp);
                    /* Continue anyway - workflow might have ended */
                } else {
                    arc_http_response_free(input_resp);
                }
            }
        }

        /* Small sleep to avoid busy loop */
        usleep(100000);  /* 100ms */
    }

    close(log_fd);
    printf("\n----------------------------------------\n");
    LOG_USER_INFO("Detached from workflow: %s\n", workflow_id);

    return ARC_EXIT_SUCCESS;
}

/* arc workflow attach command handler - tail log file and send input */
int arc_workflow_attach(int argc, char** argv) {
    if (argc < 1) {
        LOG_USER_ERROR("workflow ID required\n");
        LOG_USER_INFO("Usage: arc workflow attach <workflow_id>\n");
        return ARC_EXIT_ERROR;
    }

    const char* workflow_id = argv[0];

    /* Manual attach - seek to end to show only new output */
    return arc_workflow_attach_internal(workflow_id, 1);
}

/* Auto-attach from workflow start - show all output from beginning */
int arc_workflow_attach_auto(const char* workflow_id) {
    /* Auto-attach - start from beginning to show all output */
    return arc_workflow_attach_internal(workflow_id, 0);
}
