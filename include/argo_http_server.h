/* © 2025 Casey Koons All rights reserved */
#ifndef ARGO_HTTP_SERVER_H
#define ARGO_HTTP_SERVER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* HTTP status codes */
#define HTTP_STATUS_OK 200
#define HTTP_STATUS_NO_CONTENT 204
#define HTTP_STATUS_BAD_REQUEST 400
#define HTTP_STATUS_UNAUTHORIZED 401
#define HTTP_STATUS_FORBIDDEN 403
#define HTTP_STATUS_NOT_FOUND 404
#define HTTP_STATUS_CONFLICT 409
#define HTTP_STATUS_RATE_LIMIT 429
#define HTTP_STATUS_SERVER_ERROR 500

/* HTTP methods */
typedef enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_PUT,
    HTTP_METHOD_UNKNOWN
} http_method_t;

/* HTTP method string constants */
#define HTTP_METHOD_STR_GET "GET"
#define HTTP_METHOD_STR_POST "POST"
#define HTTP_METHOD_STR_DELETE "DELETE"
#define HTTP_METHOD_STR_PUT "PUT"
#define HTTP_METHOD_STR_UNKNOWN "UNKNOWN"

/* HTTP content types */
#define HTTP_CONTENT_TYPE_JSON "application/json"

/* HTTP error messages */
#define HTTP_DEFAULT_ERROR_MESSAGE "Unknown error"

/* HTTP request structure */
typedef struct {
    http_method_t method;
    char path[256];
    char* body;
    size_t body_length;
    char content_type[64];
    int client_fd;
} http_request_t;

/* HTTP response structure */
typedef struct {
    int status_code;
    char* body;
    size_t body_length;
    char content_type[64];
} http_response_t;

/* Route handler function type */
typedef int (*route_handler_fn)(http_request_t* req, http_response_t* resp);

/* Route entry */
typedef struct {
    http_method_t method;
    char path[256];
    route_handler_fn handler;
} route_t;

/* HTTP server structure */
typedef struct {
    int socket_fd;
    uint16_t port;
    route_t* routes;
    size_t route_count;
    size_t route_capacity;
    volatile bool running;
} http_server_t;

/* Server lifecycle */
http_server_t* http_server_create(uint16_t port);
void http_server_destroy(http_server_t* server);

/* Route management */
int http_server_add_route(http_server_t* server, http_method_t method,
                          const char* path, route_handler_fn handler);

/* Server operations */
int http_server_start(http_server_t* server);
void http_server_stop(http_server_t* server);

/* Response helpers */
void http_response_set_json(http_response_t* resp, int status, const char* json_body);
void http_response_set_error(http_response_t* resp, int status, const char* error_msg);

/* Request helpers */
const char* http_method_string(http_method_t method);
http_method_t http_method_from_string(const char* str);

#endif /* ARGO_HTTP_SERVER_H */
