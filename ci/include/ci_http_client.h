/* © 2025 Casey Koons All rights reserved */
#ifndef CI_HTTP_CLIENT_H
#define CI_HTTP_CLIENT_H

/* Default daemon configuration */
#define CI_DAEMON_DEFAULT_HOST "localhost"
#define CI_DAEMON_DEFAULT_PORT 9876
#define CI_DAEMON_PORT_ENV "ARGO_DAEMON_PORT"

/* Response structure */
typedef struct {
    int status_code;
    char* body;
    size_t body_size;
} ci_http_response_t;

/* HTTP client functions */
int ci_http_post(const char* endpoint, const char* json_body, ci_http_response_t** response);
void ci_http_response_free(ci_http_response_t* response);

/* Get daemon URL */
const char* ci_get_daemon_url(void);

/* Daemon management */
int ci_ensure_daemon_running(void);

#endif /* CI_HTTP_CLIENT_H */
