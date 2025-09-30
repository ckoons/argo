/* © 2025 Casey Koons All rights reserved */

/* Simplified HTTP client using curl command - temporary implementation */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Project includes */
#include "argo_http.h"
#include "argo_error.h"
#include "argo_log.h"

/* Initialize HTTP client */
int http_init(void) {
    return ARGO_SUCCESS;
}

/* Cleanup HTTP client */
void http_cleanup(void) {
    /* Nothing to cleanup for curl-based implementation */
}

/* Create new HTTP request */
http_request_t* http_request_new(http_method_t method, const char* url) {
    http_request_t* req = calloc(1, sizeof(http_request_t));
    if (!req) return NULL;

    req->method = method;
    req->url = strdup(url);
    req->timeout_seconds = 30;

    return req;
}

/* Add header to request */
void http_request_add_header(http_request_t* req, const char* name, const char* value) {
    if (!req || !name || !value) return;

    http_header_t* header = calloc(1, sizeof(http_header_t));
    if (!header) return;

    header->name = strdup(name);
    header->value = strdup(value);

    if (!req->headers) {
        req->headers = header;
    } else {
        http_header_t* h = req->headers;
        while (h->next) h = h->next;
        h->next = header;
    }
}

/* Set request body */
void http_request_set_body(http_request_t* req, const char* body, size_t len) {
    if (!req || !body) return;

    if (req->body) free(req->body);
    req->body = malloc(len + 1);
    if (req->body) {
        memcpy(req->body, body, len);
        req->body[len] = '\0';
        req->body_len = len;
    }
}

/* Free request */
void http_request_free(http_request_t* req) {
    if (!req) return;

    free(req->url);
    free(req->body);

    http_header_t* h = req->headers;
    while (h) {
        http_header_t* next = h->next;
        free(h->name);
        free(h->value);
        free(h);
        h = next;
    }

    free(req);
}

/* Execute HTTP request using curl */
int http_execute(const http_request_t* req, http_response_t** resp) {
    if (!req || !resp) return E_INPUT_NULL;

    /* Build curl command with status code output */
    char cmd[8192];
    int cmd_len = snprintf(cmd, sizeof(cmd), "curl -s -w '\\n%%{http_code}' -X %s",
                          req->method == HTTP_POST ? "POST" : "GET");

    /* Add headers */
    http_header_t* h = req->headers;
    while (h) {
        cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len,
                           " -H '%s: %s'", h->name, h->value);
        h = h->next;
    }

    /* Add body if present */
    if (req->body && req->method == HTTP_POST) {
        /* Write body to temp file */
        char temp_file[] = "/tmp/argo_http_XXXXXX";
        int fd = mkstemp(temp_file);
        if (fd < 0) {
            return E_SYSTEM_FILE;
        }
        write(fd, req->body, req->body_len);
        close(fd);

        cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len,
                           " -d @%s", temp_file);

        /* Add URL */
        cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len,
                           " '%s'", req->url);

        /* Execute curl */
        FILE* fp = popen(cmd, "r");
        if (!fp) {
            unlink(temp_file);
            return E_SYSTEM_PROCESS;
        }

        /* Read response */
        char* response_buf = malloc(65536);
        size_t response_size = 0;
        size_t response_capacity = 65536;

        while (!feof(fp)) {
            if (response_size + 4096 > response_capacity) {
                response_capacity *= 2;
                response_buf = realloc(response_buf, response_capacity);
            }

            size_t bytes = fread(response_buf + response_size, 1,
                               response_capacity - response_size - 1, fp);
            response_size += bytes;
        }
        response_buf[response_size] = '\0';

        pclose(fp);
        unlink(temp_file);

        /* Extract HTTP status code from last line */
        int status_code = 200;  /* Default */
        char* last_newline = strrchr(response_buf, '\n');
        if (last_newline && last_newline > response_buf) {
            /* Parse status code from last line */
            status_code = atoi(last_newline + 1);
            /* Remove status code line from body */
            *last_newline = '\0';
            response_size = last_newline - response_buf;
        }

        /* Create response */
        *resp = calloc(1, sizeof(http_response_t));
        if (!*resp) {
            free(response_buf);
            return E_SYSTEM_MEMORY;
        }

        (*resp)->status_code = status_code;
        (*resp)->body = response_buf;
        (*resp)->body_len = response_size;

    } else {
        /* GET request */
        cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len,
                           " '%s'", req->url);

        FILE* fp = popen(cmd, "r");
        if (!fp) {
            return E_SYSTEM_PROCESS;
        }

        /* Read response */
        char* response_buf = malloc(65536);
        size_t response_size = 0;
        size_t response_capacity = 65536;

        while (!feof(fp)) {
            if (response_size + 4096 > response_capacity) {
                response_capacity *= 2;
                response_buf = realloc(response_buf, response_capacity);
            }

            size_t bytes = fread(response_buf + response_size, 1,
                               response_capacity - response_size - 1, fp);
            response_size += bytes;
        }
        response_buf[response_size] = '\0';

        pclose(fp);

        /* Extract HTTP status code from last line */
        int status_code = 200;  /* Default */
        char* last_newline = strrchr(response_buf, '\n');
        if (last_newline && last_newline > response_buf) {
            /* Parse status code from last line */
            status_code = atoi(last_newline + 1);
            /* Remove status code line from body */
            *last_newline = '\0';
            response_size = last_newline - response_buf;
        }

        /* Create response */
        *resp = calloc(1, sizeof(http_response_t));
        if (!*resp) {
            free(response_buf);
            return E_SYSTEM_MEMORY;
        }

        (*resp)->status_code = status_code;
        (*resp)->body = response_buf;
        (*resp)->body_len = response_size;
    }

    return ARGO_SUCCESS;
}

/* Execute streaming request */
int http_execute_streaming(const http_request_t* req,
                          void (*callback)(const char* chunk, size_t len, void* userdata),
                          void* userdata) {
    /* For curl implementation, just execute normally and callback once */
    http_response_t* resp = NULL;
    int result = http_execute(req, &resp);

    if (result == ARGO_SUCCESS && resp && resp->body) {
        callback(resp->body, resp->body_len, userdata);
        http_response_free(resp);
    }

    return result;
}

/* Free response */
void http_response_free(http_response_t* resp) {
    if (!resp) return;

    free(resp->body);

    http_header_t* h = resp->headers;
    while (h) {
        http_header_t* next = h->next;
        free(h->name);
        free(h->value);
        free(h);
        h = next;
    }

    free(resp);
}

/* Parse URL */
int http_parse_url(const char* url, char** host, int* port, char** path) {
    /* Simple URL parser */
    const char* p = url;

    if (strncmp(p, "https://", 8) == 0) {
        p += 8;
        *port = 443;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
        *port = 80;
    } else {
        return -1;
    }

    const char* slash = strchr(p, '/');
    const char* colon = strchr(p, ':');

    size_t host_len;
    if (colon && (!slash || colon < slash)) {
        host_len = colon - p;
        *port = atoi(colon + 1);
    } else if (slash) {
        host_len = slash - p;
    } else {
        host_len = strlen(p);
    }

    *host = strndup(p, host_len);

    if (slash) {
        *path = strdup(slash);
    } else {
        *path = strdup("/");
    }

    return 0;
}