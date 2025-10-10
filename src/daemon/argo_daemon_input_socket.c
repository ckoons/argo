/* © 2025 Casey Koons All rights reserved */
/* Daemon Input Socket Handler - accepts TCP connections from arc for user input */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* External library */
#define JSMN_HEADER
#include "jsmn.h"

/* Project includes */
#include "argo_daemon.h"
#include "argo_daemon_input_socket.h"
#include "argo_workflow_registry.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_limits.h"
#include "argo_json.h"

/* Connection tracking */
typedef struct {
    int socket_fd;
    char workflow_id[ARGO_BUFFER_MEDIUM];
    bool identified;
} input_connection_t;

#define MAX_INPUT_CONNECTIONS 32

/* Global state for input socket handler */
static struct {
    argo_daemon_t* daemon;
    input_connection_t connections[MAX_INPUT_CONNECTIONS];
    pthread_mutex_t connections_lock;
    bool initialized;
} g_input_socket = {0};

/* Parse handshake JSON to extract workflow_id */
static int parse_handshake(const char* json_str, char* workflow_id, size_t id_size) {
    jsmn_parser parser;
    jsmntok_t tokens[32];

    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, json_str, strlen(json_str), tokens, 32);

    if (token_count < 2) {
        return E_PROTOCOL_FORMAT;
    }

    /* Find "workflow_id" field */
    for (int i = 0; i < token_count - 1; i++) {
        if (tokens[i].type == JSMN_STRING) {
            int len = tokens[i].end - tokens[i].start;
            if (len == 11 && strncmp(json_str + tokens[i].start, "workflow_id", 11) == 0) {
                /* Next token is the value */
                int value_len = tokens[i+1].end - tokens[i+1].start;
                if (value_len >= (int)id_size) {
                    return E_INPUT_INVALID;
                }
                strncpy(workflow_id, json_str + tokens[i+1].start, value_len);
                workflow_id[value_len] = '\0';
                return ARGO_SUCCESS;
            }
        }
    }

    return E_PROTOCOL_FORMAT;
}

/* Parse input JSON to extract input text */
static int parse_input(const char* json_str, char* input, size_t input_size) {
    jsmn_parser parser;
    jsmntok_t tokens[32];

    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, json_str, strlen(json_str), tokens, 32);

    if (token_count < 2) {
        return E_PROTOCOL_FORMAT;
    }

    /* Find "input" field */
    for (int i = 0; i < token_count - 1; i++) {
        if (tokens[i].type == JSMN_STRING) {
            int len = tokens[i].end - tokens[i].start;
            if (len == 5 && strncmp(json_str + tokens[i].start, "input", 5) == 0) {
                /* Next token is the value */
                int value_len = tokens[i+1].end - tokens[i+1].start;
                if (value_len >= (int)input_size) {
                    return E_INPUT_INVALID;
                }
                strncpy(input, json_str + tokens[i+1].start, value_len);
                input[value_len] = '\0';
                return ARGO_SUCCESS;
            }
        }
    }

    return E_PROTOCOL_FORMAT;
}

/* Send JSON response to client */
static void send_response(int socket_fd, const char* status, const char* message) {
    char response[ARGO_BUFFER_STANDARD];
    snprintf(response, sizeof(response),
            "{\"status\":\"%s\",\"message\":\"%s\"}\n",
            status, message ? message : "");

    write(socket_fd, response, strlen(response));
}


/* Register a new input connection in tracking table */
static int register_connection(int client_fd, const char* workflow_id) {
    pthread_mutex_lock(&g_input_socket.connections_lock);

    /* Find empty slot */
    int slot = -1;
    for (int i = 0; i < MAX_INPUT_CONNECTIONS; i++) {
        if (g_input_socket.connections[i].socket_fd == 0) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        pthread_mutex_unlock(&g_input_socket.connections_lock);
        return -1;  /* No slots available */
    }

    /* Register connection */
    g_input_socket.connections[slot].socket_fd = client_fd;
    g_input_socket.connections[slot].identified = true;
    strncpy(g_input_socket.connections[slot].workflow_id, workflow_id,
            sizeof(g_input_socket.connections[slot].workflow_id) - 1);

    pthread_mutex_unlock(&g_input_socket.connections_lock);

    LOG_INFO("Registered input connection: fd=%d, workflow=%s, slot=%d",
             client_fd, workflow_id, slot);
    return slot;
}

/* Unregister connection */
static void unregister_connection(int client_fd) {
    pthread_mutex_lock(&g_input_socket.connections_lock);

    for (int i = 0; i < MAX_INPUT_CONNECTIONS; i++) {
        if (g_input_socket.connections[i].socket_fd == client_fd) {
            LOG_INFO("Unregistering input connection: fd=%d, workflow=%s",
                     client_fd, g_input_socket.connections[i].workflow_id);
            g_input_socket.connections[i].socket_fd = 0;
            g_input_socket.connections[i].identified = false;
            g_input_socket.connections[i].workflow_id[0] = '\0';
            break;
        }
    }

    pthread_mutex_unlock(&g_input_socket.connections_lock);
}


/* Initialize input socket subsystem */
int input_socket_init(argo_daemon_t* daemon) {
    if (!daemon) {
        return E_INVALID_PARAMS;
    }

    g_input_socket.daemon = daemon;
    pthread_mutex_init(&g_input_socket.connections_lock, NULL);

    /* Initialize connection table */
    for (int i = 0; i < MAX_INPUT_CONNECTIONS; i++) {
        g_input_socket.connections[i].socket_fd = 0;
        g_input_socket.connections[i].identified = false;
        g_input_socket.connections[i].workflow_id[0] = '\0';
    }

    g_input_socket.initialized = true;
    LOG_INFO("Input socket subsystem initialized");
    return ARGO_SUCCESS;
}

/* Handle a JSON input socket connection (called from HTTP server accept loop) */
void input_socket_handle_connection(int client_fd) {
    char buffer[ARGO_BUFFER_LARGE];
    char workflow_id[ARGO_BUFFER_MEDIUM];

    /* Read handshake (first message must identify workflow) */
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        close(client_fd);
        return;
    }
    buffer[bytes] = '\0';

    /* Remove trailing newline */
    if (bytes > 0 && buffer[bytes-1] == '\n') {
        buffer[bytes-1] = '\0';
    }

    /* Parse handshake */
    int result = parse_handshake(buffer, workflow_id, sizeof(workflow_id));
    if (result != ARGO_SUCCESS) {
        LOG_ERROR("Failed to parse handshake from input connection");
        send_response(client_fd, "error", "Invalid handshake");
        close(client_fd);
        return;
    }

    /* Register connection */
    int slot = register_connection(client_fd, workflow_id);
    if (slot < 0) {
        LOG_ERROR("Too many input connections");
        send_response(client_fd, "error", "Server full");
        close(client_fd);
        return;
    }

    send_response(client_fd, "ok", "Connected");

    /* Handle input messages in loop until connection closes */
    while (true) {
        bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            /* Connection closed */
            LOG_INFO("Input connection closed: workflow=%s", workflow_id);
            break;
        }
        buffer[bytes] = '\0';

        /* Remove trailing newline */
        if (bytes > 0 && buffer[bytes-1] == '\n') {
            buffer[bytes-1] = '\0';
        }

        /* Parse and queue input */
        char input_text[ARGO_BUFFER_LARGE];
        result = parse_input(buffer, input_text, sizeof(input_text));
        if (result != ARGO_SUCCESS) {
            LOG_ERROR("Failed to parse input JSON");
            send_response(client_fd, "error", "Invalid JSON format");
            continue;
        }

        /* Queue input for workflow */
        result = workflow_registry_enqueue_input(g_input_socket.daemon->workflow_registry,
                                                workflow_id, input_text);

        if (result == ARGO_SUCCESS) {
            send_response(client_fd, "ok", "Input queued");
        } else if (result == E_NOT_FOUND) {
            send_response(client_fd, "error", "Workflow not found");
        } else if (result == E_RESOURCE_LIMIT) {
            send_response(client_fd, "error", "Input queue full");
        } else {
            send_response(client_fd, "error", "Failed to queue input");
        }
    }

    /* Unregister and close */
    unregister_connection(client_fd);
    close(client_fd);
}

/* Shutdown input socket subsystem */
void input_socket_shutdown(void) {
    if (!g_input_socket.initialized) {
        return;
    }

    /* Close all active connections */
    pthread_mutex_lock(&g_input_socket.connections_lock);
    for (int i = 0; i < MAX_INPUT_CONNECTIONS; i++) {
        if (g_input_socket.connections[i].socket_fd > 0) {
            close(g_input_socket.connections[i].socket_fd);
            g_input_socket.connections[i].socket_fd = 0;
        }
    }
    pthread_mutex_unlock(&g_input_socket.connections_lock);

    pthread_mutex_destroy(&g_input_socket.connections_lock);

    g_input_socket.initialized = false;
    LOG_INFO("Input socket subsystem shutdown");
}
