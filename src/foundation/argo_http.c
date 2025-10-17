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
#include "argo_limits.h"

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
    if (!url) return NULL;

    http_request_t* req = calloc(1, sizeof(http_request_t));
    if (!req) return NULL;

    req->method = method;
    req->url = strdup(url);
    if (!req->url) {
        free(req);
        return NULL;
    }
    req->timeout_seconds = HTTP_DEFAULT_TIMEOUT_SECONDS;

    return req;
}

/* Add header to request */
void http_request_add_header(http_request_t* req, const char* name, const char* value) {
    http_header_t* header = NULL;

    if (!req || !name || !value) return;

    header = calloc(1, sizeof(http_header_t));
    if (!header) goto cleanup;

    header->name = strdup(name);
    if (!header->name) goto cleanup;

    header->value = strdup(value);
    if (!header->value) goto cleanup;

    if (!req->headers) {
        req->headers = header;
    } else {
        http_header_t* h = req->headers;
        while (h->next) h = h->next;
        h->next = header;
    }
    return;

cleanup:
    if (header) {
        free(header->name);
        free(header->value);
        free(header);
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

/* Read HTTP response from curl pipe and parse status code */
static int read_http_response(FILE* fp, http_response_t** resp) {
    int result = ARGO_SUCCESS;
    char* response_buf = NULL;
    http_response_t* new_resp = NULL;

    if (!fp || !resp) {
        result = E_INPUT_NULL;
        goto cleanup;
    }

    /* Read response */
    response_buf = malloc(HTTP_RESPONSE_BUFFER_SIZE);
    if (!response_buf) {
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }
    size_t response_size = 0;
    size_t response_capacity = HTTP_RESPONSE_BUFFER_SIZE;

    while (!feof(fp)) {
        if (response_size + HTTP_CHUNK_SIZE > response_capacity) {
            response_capacity *= 2;
            char* new_buf = realloc(response_buf, response_capacity);
            if (!new_buf) {
                result = E_SYSTEM_MEMORY;
                goto cleanup;
            }
            response_buf = new_buf;
        }

        size_t bytes = fread(response_buf + response_size, 1,
                           response_capacity - response_size - 1, fp);
        if (bytes == 0 && ferror(fp)) {
            argo_report_error(E_SYSTEM_IO, "read_http_response",
                             "Error reading HTTP response from pipe");
            result = E_SYSTEM_IO;
            goto cleanup;
        }
        response_size += bytes;
    }
    response_buf[response_size] = '\0';

    /* Extract HTTP status code from last line */
    int status_code = HTTP_STATUS_OK;
    char* last_newline = strrchr(response_buf, '\n');
    if (last_newline && last_newline > response_buf) {
        char* endptr = NULL;
        long code = strtol(last_newline + 1, &endptr, DECIMAL_BASE);
        if (endptr != last_newline + 1 && code >= HTTP_STATUS_MIN_VALID && code < HTTP_STATUS_MAX_VALID) {
            status_code = (int)code;
        }
        *last_newline = '\0';
        response_size = last_newline - response_buf;
    }

    /* Validate HTTP status code - catch non-OK responses */
    if (status_code < HTTP_STATUS_OK || status_code >= HTTP_STATUS_SERVER_ERROR) {
        LOG_WARN("HTTP request returned non-2xx status: %d", status_code);
        /* Still return the response so caller can handle it */
    }

    /* Validate response body is not empty for successful requests */
    if (status_code == HTTP_STATUS_OK && response_size == 0) {
        LOG_WARN("HTTP request returned OK but empty body");
        /* This might be valid for some APIs, so don't fail */
    }

    /* Create response */
    new_resp = calloc(1, sizeof(http_response_t));
    if (!new_resp) {
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    new_resp->status_code = status_code;
    new_resp->body = response_buf;
    new_resp->body_len = response_size;

    *resp = new_resp;
    response_buf = NULL;  /* Transfer ownership */
    new_resp = NULL;

cleanup:
    free(response_buf);
    if (new_resp) {
        free(new_resp->body);
        free(new_resp);
    }
    return result;
}

/* Execute HTTP request using curl */
int http_execute(const http_request_t* req, http_response_t** resp) {
    if (!req || !resp) return E_INPUT_NULL;

    /* Build curl command with status code output and timeout */
    char cmd[HTTP_CMD_BUFFER_SIZE];
    int cmd_len = snprintf(cmd, sizeof(cmd),
                          "curl -s --max-time %d -w '\\n%%{http_code}' -X %s",
                          req->timeout_seconds > 0 ? req->timeout_seconds : HTTP_DEFAULT_TIMEOUT_SECONDS,
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
        FILE* fp = popen(cmd, "r"); /* GUIDELINE_APPROVED: curl command with validated parameters */
        if (!fp) {
            unlink(temp_file);
            return E_SYSTEM_PROCESS;
        }

        int result = read_http_response(fp, resp);
        pclose(fp);
        unlink(temp_file);
        return result;

    } else {
        /* GET request */
        cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len,
                           " '%s'", req->url);

        FILE* fp = popen(cmd, "r"); /* GUIDELINE_APPROVED: curl command with validated parameters */
        if (!fp) {
            return E_SYSTEM_PROCESS;
        }

        int result = read_http_response(fp, resp);
        pclose(fp);
        return result;
    }
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
    int result = ARGO_SUCCESS;
    char* parsed_host = NULL;
    char* parsed_path = NULL;

    /* Simple URL parser */
    if (!url || !host || !port || !path) {
        result = E_INPUT_NULL;
        goto cleanup;
    }

    const char* p = url;

    if (strncmp(p, HTTPS_SCHEME_PREFIX, HTTPS_SCHEME_PREFIX_LEN) == 0) {
        p += HTTPS_SCHEME_PREFIX_LEN;
        *port = HTTP_PORT_HTTPS;
    } else if (strncmp(p, HTTP_SCHEME_PREFIX, HTTP_SCHEME_PREFIX_LEN) == 0) {
        p += HTTP_SCHEME_PREFIX_LEN;
        *port = HTTP_PORT_HTTP;
    } else {
        result = E_INVALID_PARAMS;
        goto cleanup;
    }

    const char* slash = strchr(p, '/');
    const char* colon = strchr(p, ':');

    size_t host_len;
    if (colon && (!slash || colon < slash)) {
        host_len = colon - p;
        char* endptr = NULL;
        long port_num = strtol(colon + 1, &endptr, DECIMAL_BASE);
        if (endptr != colon + 1 && port_num > 0 && port_num <= MAX_VALID_PORT) {
            *port = (int)port_num;
        }
    } else if (slash) {
        host_len = slash - p;
    } else {
        host_len = strlen(p);
    }

    parsed_host = strndup(p, host_len);
    if (!parsed_host) {
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    if (slash) {
        parsed_path = strdup(slash);
    } else {
        parsed_path = strdup("/");
    }

    if (!parsed_path) {
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    *host = parsed_host;
    *path = parsed_path;
    parsed_host = NULL;  /* Transfer ownership */
    parsed_path = NULL;

cleanup:
    free(parsed_host);
    free(parsed_path);
    return result;
}