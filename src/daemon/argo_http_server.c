/* © 2025 Casey Koons All rights reserved */
/* Minimal HTTP server for daemon REST API */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

/* Project includes */
#include "argo_http_server.h"
#include "argo_error.h"
#include "argo_limits.h"
#include "argo_log.h"

/* Thread argument for connection handling */
typedef struct {
    http_server_t* server;
    int client_fd;
} connection_arg_t;

/* Create HTTP server */
http_server_t* http_server_create(uint16_t port) {
    http_server_t* server = calloc(1, sizeof(http_server_t));
    if (!server) {
        argo_report_error(E_SYSTEM_MEMORY, "http_server_create", "allocation failed");
        return NULL;
    }

    server->port = port;
    server->socket_fd = -1;
    server->running = false;

    /* Allocate route table */
    server->route_capacity = HTTP_MAX_ROUTES;
    server->routes = calloc(server->route_capacity, sizeof(route_t));
    if (!server->routes) {
        argo_report_error(E_SYSTEM_MEMORY, "http_server_create", "route table allocation failed");
        free(server);
        return NULL;
    }

    return server;
}

/* Destroy HTTP server */
void http_server_destroy(http_server_t* server) {
    if (!server) return;

    if (server->socket_fd >= 0) {
        close(server->socket_fd);
    }

    free(server->routes);
    free(server);
}

/* Add route to server */
int http_server_add_route(http_server_t* server, http_method_t method,
                          const char* path, route_handler_fn handler) {
    if (!server || !path || !handler) {
        return E_INVALID_PARAMS;
    }

    if (server->route_count >= server->route_capacity) {
        argo_report_error(E_RESOURCE_LIMIT, "http_server_add_route", "route table full");
        return E_RESOURCE_LIMIT;
    }

    route_t* route = &server->routes[server->route_count];
    route->method = method;
    strncpy(route->path, path, sizeof(route->path) - 1);
    route->path[sizeof(route->path) - 1] = '\0';
    route->handler = handler;

    server->route_count++;
    return ARGO_SUCCESS;
}

/* Parse HTTP method */
http_method_t http_method_from_string(const char* str) {
    if (!str) return HTTP_METHOD_UNKNOWN;
    if (strcmp(str, HTTP_METHOD_STR_GET) == 0) return HTTP_METHOD_GET;
    if (strcmp(str, HTTP_METHOD_STR_POST) == 0) return HTTP_METHOD_POST;
    if (strcmp(str, HTTP_METHOD_STR_DELETE) == 0) return HTTP_METHOD_DELETE;
    if (strcmp(str, HTTP_METHOD_STR_PUT) == 0) return HTTP_METHOD_PUT;
    return HTTP_METHOD_UNKNOWN;
}

/* Get HTTP method string */
const char* http_method_string(http_method_t method) {
    switch (method) {
        case HTTP_METHOD_GET: return HTTP_METHOD_STR_GET;
        case HTTP_METHOD_POST: return HTTP_METHOD_STR_POST;
        case HTTP_METHOD_DELETE: return HTTP_METHOD_STR_DELETE;
        case HTTP_METHOD_PUT: return HTTP_METHOD_STR_PUT;
        default: return HTTP_METHOD_STR_UNKNOWN;
    }
}

/* Parse HTTP request */
static int parse_http_request(const char* buffer, size_t len, http_request_t* req) {
    /* Parse request line: METHOD /path HTTP/1.1 */
    char method[HTTP_METHOD_SIZE] = {0};
    char path[HTTP_PATH_SIZE] = {0};

    if (sscanf(buffer, "%15s %255s", method, path) != 2) {
        return E_INVALID_PARAMS;
    }

    req->method = http_method_from_string(method);
    strncpy(req->path, path, sizeof(req->path) - 1);
    req->path[sizeof(req->path) - 1] = '\0';

    /* Find Content-Length header */
    const char* content_len_str = strstr(buffer, "Content-Length: ");
    if (content_len_str) {
        int content_len = 0;
        if (sscanf(content_len_str, "Content-Length: %d", &content_len) == 1) {
            req->body_length = content_len;
        }
    }

    /* Find body (after \r\n\r\n) */
    const char* body_start = strstr(buffer, "\r\n\r\n");
    if (body_start && req->body_length > 0) {
        body_start += 4;  /* Skip \r\n\r\n */
        size_t remaining = len - (body_start - buffer);

        if (remaining >= req->body_length) {
            req->body = malloc(req->body_length + 1);
            if (req->body) {
                memcpy(req->body, body_start, req->body_length);
                req->body[req->body_length] = '\0';
            }
        }
    }

    /* Default content type */
    strncpy(req->content_type, HTTP_CONTENT_TYPE_JSON, sizeof(req->content_type) - 1);

    return ARGO_SUCCESS;
}

/* Find matching route */
static route_handler_fn find_route(http_server_t* server, http_request_t* req) {
    /* Extract path without query string */
    const char* query_start = strchr(req->path, '?');
    size_t path_len = query_start ? (size_t)(query_start - req->path) : strlen(req->path);

    for (size_t i = 0; i < server->route_count; i++) {
        route_t* route = &server->routes[i];

        /* Skip if method doesn't match */
        if (route->method != req->method) {
            continue;
        }

        size_t route_len = strlen(route->path);

        /* Exact match */
        if (route_len == path_len &&
            strncmp(route->path, req->path, path_len) == 0) {
            return route->handler;
        }

        /* Prefix match for routes with path parameters */
        /* If route path is shorter and request starts with it, check for separator */
        if (route_len < path_len &&
            strncmp(route->path, req->path, route_len) == 0 &&
            req->path[route_len] == '/') {
            return route->handler;
        }
    }
    return NULL;
}

/* Send HTTP response */
static void send_http_response(int client_fd, http_response_t* resp) {
    char header[ARGO_BUFFER_MEDIUM];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        resp->status_code,
        resp->content_type,
        resp->body_length);

    /* Send header */
    if (write(client_fd, header, header_len) < 0) {
        return;
    }

    /* Send body */
    if (resp->body && resp->body_length > 0) {
        write(client_fd, resp->body, resp->body_length);
    }
}

/* Handle single client connection */
static void* handle_connection(void* arg) {
    connection_arg_t* conn_arg = (connection_arg_t*)arg;
    http_server_t* server = conn_arg->server;
    int client_fd = conn_arg->client_fd;
    free(conn_arg);

    char* buffer = malloc(HTTP_BUFFER_SIZE);
    if (!buffer) {
        close(client_fd);
        return NULL;
    }

    /* Read request */
    ssize_t bytes = read(client_fd, buffer, HTTP_BUFFER_SIZE - 1);
    if (bytes <= 0) {
        free(buffer);
        close(client_fd);
        return NULL;
    }
    buffer[bytes] = '\0';

    /* Parse HTTP request */
    http_request_t req = {0};
    req.client_fd = client_fd;

    int result = parse_http_request(buffer, bytes, &req);

    /* Log incoming request */
    LOG_INFO("HTTP %s %s", http_method_string(req.method), req.path);
    if (req.body && strlen(req.body) > 0) {
        LOG_DEBUG("Request body: %s", req.body);
    }

    free(buffer);

    if (result != ARGO_SUCCESS) {
        LOG_ERROR("Failed to parse HTTP request");
        /* Send BAD_REQUEST */
        const char* error_resp =
            "HTTP/1.1 BAD_REQUEST\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        write(client_fd, error_resp, strlen(error_resp));
        close(client_fd);
        free(req.body);
        return NULL;
    }

    /* Find route handler */
    route_handler_fn handler = find_route(server, &req);

    http_response_t resp = {0};
    resp.status_code = HTTP_STATUS_OK;
    strncpy(resp.content_type, HTTP_CONTENT_TYPE_JSON, sizeof(resp.content_type) - 1);

    if (handler) {
        /* Call handler */
        handler(&req, &resp);
    } else {
        /* NOT_FOUND */
        resp.status_code = HTTP_STATUS_NOT_FOUND;
        const char* not_found = "{\"status\":\"error\",\"message\":\"Not found\"}";
        resp.body = (char*)not_found;
        resp.body_length = strlen(not_found);
    }

    /* Log response */
    LOG_INFO("HTTP Response %d for %s %s",
             resp.status_code,
             http_method_string(req.method),
             req.path);
    if (resp.status_code != HTTP_STATUS_OK) {
        LOG_WARN("non-OK response: %d", resp.status_code);
        if (resp.body) {
            LOG_DEBUG("Response body: %s", resp.body);
        }
    }

    /* Send response */
    send_http_response(client_fd, &resp);

    /* Cleanup */
    if (resp.body && resp.status_code != HTTP_STATUS_NOT_FOUND) {
        free(resp.body);
    }
    free(req.body);
    close(client_fd);

    return NULL;
}

/* Start HTTP server */
int http_server_start(http_server_t* server) {
    if (!server) return E_INVALID_PARAMS;

    /* Create socket */
    server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket_fd < 0) {
        argo_report_error(E_SYSTEM_SOCKET, "http_server_start", "socket creation failed");
        return E_SYSTEM_SOCKET;
    }

    /* Set socket options */
    int opt = 1;
    if (setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server->socket_fd);
        argo_report_error(E_SYSTEM_SOCKET, "http_server_start", "setsockopt failed");
        return E_SYSTEM_SOCKET;
    }

    /* Bind to port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server->port);

    if (bind(server->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server->socket_fd);
        argo_report_error(E_SYSTEM_SOCKET, "http_server_start", "bind failed");
        return E_SYSTEM_SOCKET;
    }

    /* Listen */
    if (listen(server->socket_fd, HTTP_BACKLOG) < 0) {
        close(server->socket_fd);
        argo_report_error(E_SYSTEM_SOCKET, "http_server_start", "listen failed");
        return E_SYSTEM_SOCKET;
    }

    server->running = true;
    printf("HTTP server listening on port %d\n", server->port);

    /* Accept connections */
    while (server->running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server->socket_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (server->running) {
                continue;  /* Transient error, keep going */
            } else {
                break;  /* Server stopping */
            }
        }

        /* Spawn thread to handle connection */
        connection_arg_t* arg = malloc(sizeof(connection_arg_t));
        if (!arg) {
            close(client_fd);
            continue;
        }
        arg->server = server;
        arg->client_fd = client_fd;

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_connection, arg) == 0) {
            pthread_detach(thread);
        } else {
            free(arg);
            close(client_fd);
        }
    }

    return ARGO_SUCCESS;
}

/* Stop HTTP server */
void http_server_stop(http_server_t* server) {
    if (!server) return;

    server->running = false;

    if (server->socket_fd >= 0) {
        shutdown(server->socket_fd, SHUT_RDWR);
        close(server->socket_fd);
        server->socket_fd = -1;
    }
}

/* Response helper - set JSON body */
void http_response_set_json(http_response_t* resp, int status, const char* json_body) {
    if (!resp) return;

    resp->status_code = status;
    strncpy(resp->content_type, HTTP_CONTENT_TYPE_JSON, sizeof(resp->content_type) - 1);

    if (json_body) {
        size_t len = strlen(json_body);
        resp->body = malloc(len + 1);
        if (resp->body) {
            memcpy(resp->body, json_body, len);
            resp->body[len] = '\0';
            resp->body_length = len;
        }
    }
}

/* Response helper - set error */
void http_response_set_error(http_response_t* resp, int status, const char* error_msg) {
    if (!resp) return;

    char json[ARGO_PATH_MAX];
    snprintf(json, sizeof(json),
        "{\"status\":\"error\",\"message\":\"%s\"}",
        error_msg ? error_msg : HTTP_DEFAULT_ERROR_MESSAGE);

    http_response_set_json(resp, status, json);
}
