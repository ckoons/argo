/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_SOCKET_H
#define ARGO_SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Socket configuration */
#define SOCKET_PATH_FORMAT "/tmp/argo_ci_%s.sock"
#define SOCKET_BACKLOG 5
#define MAX_PENDING_REQUESTS 50
#define DEFAULT_REQUEST_TIMEOUT 30000  /* 30 seconds */

/* Message format strings */
#define MSG_JSON_FORMAT "{\"from\":\"%s\",\"to\":\"%s\",\"type\":\"%s\",\"content\":\"%s\"}"

/* Socket response structure (extends ci_response) */
typedef struct socket_response {
    bool success;
    int error_code;
    char* content;
    char* model_used;
    time_t timestamp;
} socket_response_t;

/* Include CI types */
#include "argo_ci.h"
#include "argo_registry.h"

/* ci_message_t is defined in argo_registry.h */

/* Callback for async responses */
typedef void (*socket_callback_fn)(const ci_response_t* response, void* userdata);

/* Request tracking */
typedef struct socket_request {
    uint32_t id;
    socket_callback_fn callback;
    void* userdata;
    time_t timestamp;
    int timeout_ms;
    char target_ci[32];
} socket_request_t;

/* Socket server API */
int socket_server_init(const char* ci_name);
void socket_set_registry(ci_registry_t* registry);
int socket_server_run(int timeout_ms);
int socket_send_message(const ci_message_t* msg, socket_callback_fn callback, void* userdata);
void socket_server_cleanup(void);

/* Socket client API (for CI-to-CI communication) */
int socket_connect_to_ci(const char* target_ci);
int socket_disconnect_from_ci(const char* target_ci);
bool socket_is_connected(const char* target_ci);

/* Utility functions */
const char* socket_get_path(const char* ci_name);
int socket_set_timeout(int fd, int timeout_ms);
int socket_set_nonblocking(int fd);

#endif /* ARGO_SOCKET_H */