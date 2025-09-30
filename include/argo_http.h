/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_HTTP_H
#define ARGO_HTTP_H

#include <stddef.h>
#include <stdbool.h>

/* HTTP methods */
typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE
} http_method_t;

/* HTTP header */
typedef struct http_header {
    char* name;
    char* value;
    struct http_header* next;
} http_header_t;

/* HTTP request */
typedef struct {
    http_method_t method;
    char* url;
    http_header_t* headers;
    char* body;
    size_t body_len;
    int timeout_seconds;
} http_request_t;

/* HTTP response */
typedef struct {
    int status_code;
    char* body;
    size_t body_len;
    http_header_t* headers;
} http_response_t;

/* HTTP client functions */
int http_init(void);
void http_cleanup(void);

/* Request builders */
http_request_t* http_request_new(http_method_t method, const char* url);
void http_request_add_header(http_request_t* req, const char* name, const char* value);
void http_request_set_body(http_request_t* req, const char* body, size_t len);
void http_request_free(http_request_t* req);

/* Execute request */
int http_execute(const http_request_t* req, http_response_t** resp);
int http_execute_streaming(const http_request_t* req,
                          void (*callback)(const char* chunk, size_t len, void* userdata),
                          void* userdata);

/* Response handling */
void http_response_free(http_response_t* resp);
const char* http_response_get_header(const http_response_t* resp, const char* name);

/* Utility functions */
char* http_url_encode(const char* str);
int http_parse_url(const char* url, char** host, int* port, char** path);

#endif /* ARGO_HTTP_H */