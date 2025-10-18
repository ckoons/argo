/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "arc_commands.h"
#include "arc_http_client.h"
#include "arc_constants.h"
#include "argo_output.h"
#include "argo_error.h"

/* Global flag for attach loop */
static int g_running = 1;

/* Escape JSON string - caller must free result */
static char* json_escape_string(const char* input) {
    if (!input) return NULL;

    /* Calculate required size (worst case: every char needs escaping) */
    size_t max_size = strlen(input) * 2 + 1;
    char* output = malloc(max_size);
    if (!output) return NULL;

    const char* src = input;
    char* dst = output;

    while (*src) {
        switch (*src) {
            case '"':  *dst++ = '\\'; *dst++ = '"'; break;
            case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
            case '\b': *dst++ = '\\'; *dst++ = 'b'; break;
            case '\f': *dst++ = '\\'; *dst++ = 'f'; break;
            case '\n': *dst++ = '\\'; *dst++ = 'n'; break;
            case '\r': *dst++ = '\\'; *dst++ = 'r'; break;
            case '\t': *dst++ = '\\'; *dst++ = 't'; break;
            default:
                /* Control characters need \uXXXX escaping */
                if (*src < 0x20) {
                    dst += snprintf(dst, 7, "\\u%04x", (unsigned char)*src);
                } else {
                    *dst++ = *src;
                }
                break;
        }
        src++;
    }
    *dst = '\0';

    return output;
}

/* Internal attach implementation with seek control */
static int arc_workflow_attach_internal(const char* workflow_id, int seek_to_end) {
    if (!workflow_id) {
        LOG_USER_ERROR("workflow ID required\n");
        return ARC_EXIT_ERROR;
    }

    /* Verify workflow exists */
    char endpoint[ARC_PATH_BUFFER];
    snprintf(endpoint, sizeof(endpoint), "/api/workflow/status/%s", workflow_id);

    arc_http_response_t* response = NULL;
    int result = arc_http_get(endpoint, &response);
    if (result != ARGO_SUCCESS || !response) {
        LOG_USER_ERROR("Failed to connect to daemon\n");
        if (response) arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    }

    if (response->status_code == ARC_HTTP_STATUS_NOT_FOUND) {
        LOG_USER_ERROR("Workflow not found: %s\n", workflow_id);
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    } else if (response->status_code != ARC_HTTP_STATUS_OK) {
        LOG_USER_ERROR("Failed to get workflow status (HTTP %d)\n", response->status_code);
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    }

    arc_http_response_free(response);

    /* Build log file path */
    const char* home = getenv("HOME");
    if (!home) home = ".";

    char log_path[ARC_PATH_BUFFER];
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

    LOG_USER_SUCCESS("Attached to workflow: %s\n", workflow_id);
    LOG_USER_INFO("Logs: %s\n", log_path);
    LOG_USER_INFO("Press Ctrl+D to detach\n");
    /* GUIDELINE_APPROVED - Visual separator for workflow output */
    printf("----------------------------------------\n");
    /* GUIDELINE_APPROVED_END */

    /* Main loop: tail log file and check for input */
    char buffer[ARC_JSON_BUFFER];
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
        tv.tv_usec = ARC_POLLING_INTERVAL_US;  /* 100ms timeout */

        int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
        if (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            /* Read line from stdin */
            char input_line[ARC_LINE_BUFFER];
            if (fgets(input_line, sizeof(input_line), stdin)) {
                /* Escape input for JSON */
                char* escaped_input = json_escape_string(input_line);
                if (!escaped_input) {
                    LOG_USER_ERROR("Failed to escape input\n");
                } else {
                    /* Send input to workflow via HTTP */
                    char json_body[ARC_ATTACH_JSON_BUFFER];
                    snprintf(json_body, sizeof(json_body), "{\"input\":\"%s\"}", escaped_input);
                    free(escaped_input);

                    snprintf(endpoint, sizeof(endpoint), "/api/workflow/input/%s", workflow_id);

                    arc_http_response_t* input_resp = NULL;
                    result = arc_http_post(endpoint, json_body, &input_resp);

                    if (result != ARGO_SUCCESS || !input_resp || input_resp->status_code != ARC_HTTP_STATUS_OK) {
                        LOG_USER_WARN("Failed to send input to workflow\n");
                        if (input_resp) arc_http_response_free(input_resp);
                        /* Continue anyway - workflow might have ended */
                    } else {
                        arc_http_response_free(input_resp);
                    }
                }
            } else {
                /* EOF detected (Ctrl+D) - detach */
                if (feof(stdin)) {
                    g_running = 0;
                }
            }
        }

        /* Small sleep to avoid busy loop */
        usleep(ARC_POLLING_INTERVAL_US);  /* 100ms */
    }

    close(log_fd);
    /* GUIDELINE_APPROVED - Visual separator for workflow output */
    printf("\n----------------------------------------\n");
    /* GUIDELINE_APPROVED_END */
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
