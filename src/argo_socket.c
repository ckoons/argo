/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

/* Project includes */
#include "argo_ci.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_socket.h"
#include "jsmn.h"  /* Single-header JSON parser */

/* Socket server state */
typedef struct socket_context {
    /* Server socket */
    int listen_fd;
    char socket_path[SOCKET_PATH_MAX];

    /* Registry for CI lookup */
    ci_registry_t* registry;

    /* Poll infrastructure */
    struct pollfd pollfds[REGISTRY_MAX_CIS];
    int client_fds[REGISTRY_MAX_CIS];  /* fd -> index mapping */
    int nfds;

    /* Message parsing */
    jsmn_parser parser;
    jsmntok_t tokens[SOCKET_JSON_TOKEN_MAX];  /* Enough for reasonable JSON */
    char recv_buffer[CI_MAX_MESSAGE];
    size_t recv_pos;

    /* Request tracking */
    socket_request_t requests[MAX_PENDING_REQUESTS];
    int request_count;
    uint32_t next_request_id;

    /* Statistics */
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t errors;
} socket_context_t;

/* Global server context */
static socket_context_t* g_socket_ctx = NULL;

/* Static function declarations */
static int setup_server_socket(const char* ci_name);
static int make_nonblocking(int fd);
static int accept_client(void);
static int handle_client_data(int fd, int index);
static int parse_json_message(const char* json, ci_message_t* msg);
static void check_request_timeouts(void);
static void cleanup_client(int index);

/* Initialize socket server for CI */
int socket_server_init(const char* ci_name) {
    ARGO_CHECK_NULL(ci_name);

    if (g_socket_ctx) {
        LOG_WARN("Socket server already initialized");
        return ARGO_SUCCESS;
    }

    /* Allocate context */
    g_socket_ctx = calloc(1, sizeof(socket_context_t));
    if (!g_socket_ctx) {
        return E_SYSTEM_MEMORY;
    }

    /* Build socket path */
    snprintf(g_socket_ctx->socket_path, sizeof(g_socket_ctx->socket_path),
             SOCKET_PATH_FORMAT, ci_name);

    /* Setup server socket */
    int result = setup_server_socket(ci_name);
    if (result != ARGO_SUCCESS) {
        free(g_socket_ctx);
        g_socket_ctx = NULL;
        return result;
    }

    /* Initialize poll array */
    g_socket_ctx->pollfds[0].fd = g_socket_ctx->listen_fd;
    g_socket_ctx->pollfds[0].events = POLLIN;
    g_socket_ctx->nfds = 1;

    /* Initialize JSON parser */
    jsmn_init(&g_socket_ctx->parser);

    /* Initialize request tracking */
    g_socket_ctx->next_request_id = 1;

    LOG_INFO("Socket server initialized at %s", g_socket_ctx->socket_path);
    return ARGO_SUCCESS;
}

/* Set registry for CI lookup */
void socket_set_registry(ci_registry_t* registry) {
    if (g_socket_ctx) {
        g_socket_ctx->registry = registry;
    }
}

/* Run socket server event loop */
int socket_server_run(int timeout_ms) {
    if (!g_socket_ctx) {
        return E_INTERNAL_LOGIC;
    }

    /* Poll for events */
    int ready = poll(g_socket_ctx->pollfds, g_socket_ctx->nfds, timeout_ms);

    if (ready < 0) {
        if (errno == EINTR) {
            return ARGO_SUCCESS;  /* Interrupted by signal */
        }
        argo_report_error(E_SYSTEM_SOCKET, "socket_server_run", "poll() failed: %s", strerror(errno));
        return E_SYSTEM_SOCKET;
    }

    if (ready == 0) {
        /* Timeout - check request timeouts */
        check_request_timeouts();
        return ARGO_SUCCESS;
    }

    /* Process ready sockets */
    for (int i = 0; i < g_socket_ctx->nfds && ready > 0; i++) {
        if (g_socket_ctx->pollfds[i].revents == 0) {
            continue;
        }

        ready--;

        /* Check for errors */
        if (g_socket_ctx->pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            if (i == 0) {
                argo_report_error(E_SYSTEM_SOCKET, "socket_server_run", "server socket error");
                return E_SYSTEM_SOCKET;
            }
            LOG_DEBUG("Client %d disconnected", i);
            cleanup_client(i);
            continue;
        }

        /* Handle readable sockets */
        if (g_socket_ctx->pollfds[i].revents & POLLIN) {
            if (i == 0) {
                /* New connection on server socket */
                int result = accept_client();
                if (result != ARGO_SUCCESS) {
                    LOG_WARN("Failed to accept client: %d", result);
                }
            } else {
                /* Data from client */
                int result = handle_client_data(g_socket_ctx->pollfds[i].fd, i);
                if (result != ARGO_SUCCESS) {
                    LOG_DEBUG("Error handling client %d: %d", i, result);
                    cleanup_client(i);
                }
            }
        }
    }

    return ARGO_SUCCESS;
}

/* Send message and track for response */
int socket_send_message(const ci_message_t* msg, socket_callback_fn callback, void* userdata) {
    ARGO_CHECK_NULL(msg);
    ARGO_CHECK_NULL(g_socket_ctx);

    if (g_socket_ctx->request_count >= MAX_PENDING_REQUESTS) {
        argo_report_error(E_PROTOCOL_QUEUE, "socket_send_message", "");
        return E_PROTOCOL_QUEUE;
    }

    /* Find target socket from registry */
    int target_fd = -1;
    if (g_socket_ctx->registry) {
        ci_registry_entry_t* target_ci = registry_find_ci(g_socket_ctx->registry, msg->to);
        if (target_ci) {
            target_fd = target_ci->socket_fd;
        }
    }

    if (target_fd < 0) {
        argo_report_error(E_CI_DISCONNECTED, "socket_send_message", "target CI %s", msg->to);
        return E_CI_DISCONNECTED;
    }

    /* Build JSON message */
    char json_buffer[CI_MAX_MESSAGE];
    int len = snprintf(json_buffer, sizeof(json_buffer),
                      MSG_JSON_FORMAT,
                      msg->from, msg->to, msg->type,
                      msg->content ? msg->content : "");

    if (len >= (int)sizeof(json_buffer)) {
        return E_PROTOCOL_SIZE;
    }

    /* Send message */
    ssize_t sent = send(target_fd, json_buffer, len, MSG_NOSIGNAL);
    if (sent != len) {
        argo_report_error(E_SYSTEM_SOCKET, "socket_send_message", "send failed: %s", strerror(errno));
        return E_SYSTEM_SOCKET;
    }

    /* Track request if callback provided */
    if (callback) {
        socket_request_t* req = &g_socket_ctx->requests[g_socket_ctx->request_count++];
        req->id = g_socket_ctx->next_request_id++;
        req->callback = callback;
        req->userdata = userdata;
        req->timestamp = time(NULL);
        req->timeout_ms = DEFAULT_REQUEST_TIMEOUT;
        strncpy(req->target_ci, msg->to, sizeof(req->target_ci) - 1);
    }

    g_socket_ctx->messages_sent++;
    return ARGO_SUCCESS;
}

/* Cleanup socket server */
void socket_server_cleanup(void) {
    if (!g_socket_ctx) {
        return;
    }

    /* Close all client connections */
    for (int i = 1; i < g_socket_ctx->nfds; i++) {
        if (g_socket_ctx->pollfds[i].fd >= 0) {
            close(g_socket_ctx->pollfds[i].fd);
        }
    }

    /* Close server socket */
    if (g_socket_ctx->listen_fd >= 0) {
        close(g_socket_ctx->listen_fd);
    }

    /* Remove socket file */
    unlink(g_socket_ctx->socket_path);

    LOG_INFO("Socket server cleanup complete. Messages: sent=%llu recv=%llu errors=%llu",
             g_socket_ctx->messages_sent, g_socket_ctx->messages_received, g_socket_ctx->errors);

    free(g_socket_ctx);
    g_socket_ctx = NULL;
}

/* Static function implementations */

static int setup_server_socket(const char* ci_name) {
    (void)ci_name;  /* Used in socket path already set */
    struct sockaddr_un addr;

    /* Create socket */
    g_socket_ctx->listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_socket_ctx->listen_fd < 0) {
        argo_report_error(E_SYSTEM_SOCKET, "setup_server_socket", "socket() failed: %s", strerror(errno));
        return E_SYSTEM_SOCKET;
    }

    /* Make non-blocking */
    int result = make_nonblocking(g_socket_ctx->listen_fd);
    if (result != ARGO_SUCCESS) {
        close(g_socket_ctx->listen_fd);
        return result;
    }

    /* Remove old socket file if exists */
    unlink(g_socket_ctx->socket_path);

    /* Bind to path */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_socket_ctx->socket_path, sizeof(addr.sun_path) - 1);

    if (bind(g_socket_ctx->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        argo_report_error(E_SYSTEM_SOCKET, "setup_server_socket", "bind() failed: %s", strerror(errno));
        close(g_socket_ctx->listen_fd);
        return E_SYSTEM_SOCKET;
    }

    /* Listen for connections */
    if (listen(g_socket_ctx->listen_fd, SOCKET_BACKLOG) < 0) {
        argo_report_error(E_SYSTEM_SOCKET, "setup_server_socket", "listen() failed: %s", strerror(errno));
        close(g_socket_ctx->listen_fd);
        unlink(g_socket_ctx->socket_path);
        return E_SYSTEM_SOCKET;
    }

    return ARGO_SUCCESS;
}

static int make_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        argo_report_error(E_SYSTEM_SOCKET, "make_nonblocking", "fcntl(F_GETFL) failed: %s", strerror(errno));
        return E_SYSTEM_SOCKET;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        argo_report_error(E_SYSTEM_SOCKET, "make_nonblocking", "fcntl(F_SETFL) failed: %s", strerror(errno));
        return E_SYSTEM_SOCKET;
    }

    return ARGO_SUCCESS;
}

static int accept_client(void) {
    struct sockaddr_un addr;
    socklen_t addr_len = sizeof(addr);

    int client_fd = accept(g_socket_ctx->listen_fd, (struct sockaddr*)&addr, &addr_len);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ARGO_SUCCESS;  /* No pending connections */
        }
        argo_report_error(E_SYSTEM_SOCKET, "accept_client", "accept() failed: %s", strerror(errno));
        return E_SYSTEM_SOCKET;
    }

    /* Make client non-blocking */
    int result = make_nonblocking(client_fd);
    if (result != ARGO_SUCCESS) {
        close(client_fd);
        return result;
    }

    /* Add to poll array */
    if (g_socket_ctx->nfds >= REGISTRY_MAX_CIS) {
        argo_report_error(E_PROTOCOL_QUEUE, "accept_client", "");
        close(client_fd);
        return E_PROTOCOL_QUEUE;
    }

    g_socket_ctx->pollfds[g_socket_ctx->nfds].fd = client_fd;
    g_socket_ctx->pollfds[g_socket_ctx->nfds].events = POLLIN;
    g_socket_ctx->client_fds[g_socket_ctx->nfds] = client_fd;
    g_socket_ctx->nfds++;

    LOG_DEBUG("Client connected: fd=%d", client_fd);
    return ARGO_SUCCESS;
}

static int handle_client_data(int fd, int index) {
    (void)index;  /* Used for cleanup on error */
    char buffer[SOCKET_BUFFER_SIZE];

    ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ARGO_SUCCESS;
        }
        return E_SYSTEM_SOCKET;
    }

    if (bytes == 0) {
        /* Client closed connection */
        return E_CI_DISCONNECTED;
    }

    buffer[bytes] = '\0';

    /* Parse JSON message */
    ci_message_t msg;
    int result = parse_json_message(buffer, &msg);
    if (result != ARGO_SUCCESS) {
        LOG_WARN("Failed to parse message: %s", buffer);
        g_socket_ctx->errors++;
        return result;
    }

    g_socket_ctx->messages_received++;

    /* Route message through registry */
    int route_result = ARGO_SUCCESS;
    if (g_socket_ctx->registry) {
        /* Convert to JSON for registry_send_message */
        char json_msg[CI_MAX_MESSAGE];
        snprintf(json_msg, sizeof(json_msg), MSG_JSON_FORMAT,
                msg.from, msg.to, msg.type, msg.content ? msg.content : "");

        route_result = registry_send_message(g_socket_ctx->registry,
                                            msg.from, msg.to, json_msg);
        if (route_result != ARGO_SUCCESS) {
            LOG_WARN("Failed to route message: %d", route_result);
        }
    } else {
        LOG_DEBUG("Received message from %s to %s (no registry): %s", msg.from, msg.to, msg.content);
    }

    /* Free message memory to prevent leaks */
    message_free(&msg);

    return route_result;
}

static int parse_json_message(const char* json, ci_message_t* msg) {
    ARGO_CHECK_NULL(json);
    ARGO_CHECK_NULL(msg);

    memset(msg, 0, sizeof(*msg));

    /* Parse JSON */
    jsmn_parser parser;
    jsmntok_t tokens[SOCKET_JSON_TOKEN_MAX];
    jsmn_init(&parser);

    int token_count = jsmn_parse(&parser, json, strlen(json), tokens, SOCKET_JSON_TOKEN_MAX);
    if (token_count < 0) {
        return E_PROTOCOL_FORMAT;
    }

    /* Extract fields - simple parsing for known format */
    for (int i = 0; i < token_count - 1; i++) {
        if (tokens[i].type != JSMN_STRING) continue;

        int len = tokens[i].end - tokens[i].start;
        if (strncmp(json + tokens[i].start, "from", len) == 0) {
            int val_len = tokens[i+1].end - tokens[i+1].start;
            if (val_len < (int)sizeof(msg->from)) {
                strncpy(msg->from, json + tokens[i+1].start, val_len);
            }
        } else if (strncmp(json + tokens[i].start, "to", len) == 0) {
            int val_len = tokens[i+1].end - tokens[i+1].start;
            if (val_len < (int)sizeof(msg->to)) {
                strncpy(msg->to, json + tokens[i+1].start, val_len);
            }
        } else if (strncmp(json + tokens[i].start, "content", len) == 0) {
            int val_len = tokens[i+1].end - tokens[i+1].start;
            msg->content = strndup(json + tokens[i+1].start, val_len);
        }
    }

    msg->timestamp = time(NULL);
    msg->type = strdup("request");  /* Default type */

    return ARGO_SUCCESS;
}

static void check_request_timeouts(void) {
    time_t now = time(NULL);

    for (int i = 0; i < g_socket_ctx->request_count; i++) {
        socket_request_t* req = &g_socket_ctx->requests[i];

        if ((now - req->timestamp) * MS_PER_SECOND > req->timeout_ms) {
            /* Timeout occurred */
            LOG_DEBUG("Request %u timed out", req->id);

            if (req->callback) {
                ci_response_t timeout_response = {
                    .success = false,
                    .error_code = E_CI_TIMEOUT,
                    .content = "Request timed out"
                };
                req->callback(&timeout_response, req->userdata);
            }

            /* Remove from list */
            memmove(&g_socket_ctx->requests[i],
                   &g_socket_ctx->requests[i+1],
                   sizeof(socket_request_t) * (g_socket_ctx->request_count - i - 1));
            g_socket_ctx->request_count--;
            i--;
        }
    }
}

static void cleanup_client(int index) {
    if (index <= 0 || index >= g_socket_ctx->nfds) {
        return;
    }

    close(g_socket_ctx->pollfds[index].fd);

    /* Shift array down */
    memmove(&g_socket_ctx->pollfds[index],
           &g_socket_ctx->pollfds[index + 1],
           sizeof(struct pollfd) * (g_socket_ctx->nfds - index - 1));

    g_socket_ctx->nfds--;
}