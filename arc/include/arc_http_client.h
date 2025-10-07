/* © 2025 Casey Koons All rights reserved */
#ifndef ARC_HTTP_CLIENT_H
#define ARC_HTTP_CLIENT_H

/* Default daemon configuration */
#define ARC_DAEMON_DEFAULT_HOST "localhost"
#define ARC_DAEMON_DEFAULT_PORT 9876
#define ARC_DAEMON_PORT_ENV "ARGO_DAEMON_PORT"

/* Response structure */
typedef struct {
    int status_code;
    char* body;
    size_t body_size;
} arc_http_response_t;

/* HTTP client functions */
int arc_http_get(const char* endpoint, arc_http_response_t** response);
int arc_http_post(const char* endpoint, const char* json_body, arc_http_response_t** response);
int arc_http_delete(const char* endpoint, arc_http_response_t** response);
void arc_http_response_free(arc_http_response_t* response);

/* Get daemon URL */
const char* arc_get_daemon_url(void);

/* Daemon management */
int arc_ensure_daemon_running(void);

#endif /* ARC_HTTP_CLIENT_H */
