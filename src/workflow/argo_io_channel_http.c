/* © 2025 Casey Koons All rights reserved */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>
#include "argo_io_channel_http.h"
#include "argo_io_channel.h"
#include "argo_error.h"
#include "argo_limits.h"
#include "argo_log.h"
#include "argo_json.h"
#include "argo_http.h"

/* HTTP I/O channel context */
typedef struct {
    io_channel_t base;           /* Base channel (must be first) */
    char daemon_url[ARGO_BUFFER_MEDIUM];
    char workflow_id[ARGO_BUFFER_SMALL];
    CURL* curl;                  /* Reusable CURL handle */

    /* Write buffering */
    char* write_buffer;
    size_t write_buffer_size;
    size_t write_buffer_used;

    /* Read buffering */
    char* read_buffer;
    size_t read_buffer_size;
    size_t read_buffer_used;
} http_io_context_t;

/* CURL write callback for response data */
static size_t http_io_curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    char** response_ptr = (char**)userp;

    /* Allocate or reallocate response buffer */
    char* new_buffer = realloc(*response_ptr, realsize + 1);
    if (!new_buffer) {
        return 0;  /* Signal error to CURL */
    }

    *response_ptr = new_buffer;
    memcpy(*response_ptr, contents, realsize);
    (*response_ptr)[realsize] = '\0';

    return realsize;
}

/* POST output to daemon (internal) */
static int http_flush_output_internal(http_io_context_t* ctx) {
    if (ctx->write_buffer_used == 0) {
        return ARGO_SUCCESS;  /* Nothing to flush */
    }

    /* Build URL with query parameter */
    char url[ARGO_BUFFER_MEDIUM];
    snprintf(url, sizeof(url), "%s/api/workflow/output?workflow_name=%s",
             ctx->daemon_url, ctx->workflow_id);

    /* Build JSON body - escape the output text */
    size_t max_escaped = ctx->write_buffer_used * JSON_ESCAPE_MAX_MULTIPLIER + JSON_OVERHEAD_BYTES;
    char* json_body = malloc(max_escaped);
    if (!json_body) {
        return E_SYSTEM_MEMORY;
    }

    /* Simple JSON encoding - just escape quotes and backslashes */
    char* dst = json_body;
    size_t remaining = max_escaped;

    /* Add opening */
    int written = snprintf(dst, remaining, "{\"output\":\"");
    dst += written;
    remaining -= written;

    /* Escape content */
    for (size_t i = 0; i < ctx->write_buffer_used && remaining > JSON_ENCODING_SAFETY_MARGIN; i++) {
        char c = ctx->write_buffer[i];
        if (c == '"' || c == '\\') {
            *dst++ = '\\';
            remaining--;
        }
        if (c == '\n') {
            *dst++ = '\\';
            *dst++ = 'n';
            remaining -= 2;
        } else if (c == '\r') {
            *dst++ = '\\';
            *dst++ = 'r';
            remaining -= 2;
        } else if (c == '\t') {
            *dst++ = '\\';
            *dst++ = 't';
            remaining -= 2;
        } else {
            *dst++ = c;
            remaining--;
        }
    }

    /* Add closing */
    snprintf(dst, remaining, "\"}");

    /* Setup CURL request */
    curl_easy_setopt(ctx->curl, CURLOPT_URL, url);
    curl_easy_setopt(ctx->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT, (long)IO_HTTP_WRITE_TIMEOUT_SEC);

    /* Discard response body (don't write to stdout) */
    char* response = NULL;
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, http_io_curl_write_callback);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, &response);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, headers);

    /* Perform request */
    CURLcode res = curl_easy_perform(ctx->curl);

    free(response);  /* Discard response */

    curl_slist_free_all(headers);
    free(json_body);

    if (res != CURLE_OK) {
        LOG_ERROR("Failed to flush output to daemon: %s", curl_easy_strerror(res));
        return E_SYSTEM_NETWORK;
    }

    /* Clear write buffer */
    ctx->write_buffer_used = 0;

    return ARGO_SUCCESS;
}

/* GET input from daemon (internal) */
static int http_poll_input_internal(http_io_context_t* ctx, char* buffer, size_t max_len) {
    /* Build URL with query parameter */
    char url[ARGO_BUFFER_MEDIUM];
    snprintf(url, sizeof(url), "%s/api/workflow/input?workflow_name=%s",
             ctx->daemon_url, ctx->workflow_id);

    /* Setup CURL request */
    curl_easy_setopt(ctx->curl, CURLOPT_URL, url);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT, (long)IO_HTTP_READ_TIMEOUT_SEC);

    char* response = NULL;
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, http_io_curl_write_callback);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, &response);

    /* Perform request */
    CURLcode res = curl_easy_perform(ctx->curl);

    if (res != CURLE_OK) {
        LOG_DEBUG("Network error polling input from daemon: %s", curl_easy_strerror(res));
        free(response);
        return E_IO_WOULDBLOCK;  /* Treat network errors as "no data yet" for resilience */
    }

    /* Check HTTP status */
    long http_code = 0;
    curl_easy_getinfo(ctx->curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code == HTTP_STATUS_NOT_FOUND || http_code == HTTP_STATUS_NO_CONTENT) {
        free(response);
        return E_IO_WOULDBLOCK;  /* No input available */
    }

    if (http_code != HTTP_STATUS_OK) {
        free(response);
        LOG_ERROR("HTTP error polling input: %ld", http_code);
        return E_SYSTEM_NETWORK;
    }

    if (!response) {
        return E_IO_WOULDBLOCK;  /* No data */
    }

    /* Parse JSON response to extract "input" field */
    const char* field_path[] = {"input"};
    char* input_text = NULL;
    size_t input_len = 0;
    int result = json_extract_nested_string(response, field_path, 1, &input_text, &input_len);
    free(response);

    if (result != ARGO_SUCCESS || !input_text || input_len == 0) {
        free(input_text);
        return E_IO_WOULDBLOCK;  /* No input available or empty */
    }

    /* Copy to buffer */
    size_t len = input_len;
    if (len >= max_len) {
        len = max_len - 1;
    }
    memcpy(buffer, input_text, len);
    buffer[len] = '\0';

    free(input_text);
    return ARGO_SUCCESS;
}

io_channel_t* io_channel_create_http(const char* daemon_url, const char* workflow_id) {
    if (!daemon_url || !workflow_id) {
        argo_report_error(E_INVALID_PARAMS, "io_channel_create_http", "null parameters");
        return NULL;
    }

    /* Allocate context */
    http_io_context_t* ctx = calloc(1, sizeof(http_io_context_t));
    if (!ctx) {
        argo_report_error(E_SYSTEM_MEMORY, "io_channel_create_http", "context allocation");
        return NULL;
    }

    /* Initialize base channel fields */
    ctx->base.type = IO_CHANNEL_HTTP;  /* Mark as HTTP channel */
    ctx->base.read_fd = -1;   /* Not used for HTTP */
    ctx->base.write_fd = -1;  /* Not used for HTTP */
    ctx->base.is_open = true;
    ctx->base.non_blocking = true;  /* HTTP is always non-blocking */

    /* Store daemon URL and workflow ID */
    strncpy(ctx->daemon_url, daemon_url, sizeof(ctx->daemon_url) - 1);
    strncpy(ctx->workflow_id, workflow_id, sizeof(ctx->workflow_id) - 1);

    /* Initialize CURL */
    ctx->curl = curl_easy_init();
    if (!ctx->curl) {
        argo_report_error(E_SYSTEM_NETWORK, "io_channel_create_http", "curl_easy_init failed");
        free(ctx);
        return NULL;
    }

    /* Allocate write buffer */
    ctx->write_buffer_size = ARGO_BUFFER_STANDARD;
    ctx->write_buffer = malloc(ctx->write_buffer_size);
    if (!ctx->write_buffer) {
        argo_report_error(E_SYSTEM_MEMORY, "io_channel_create_http", "write buffer allocation");
        curl_easy_cleanup(ctx->curl);
        free(ctx);
        return NULL;
    }
    ctx->write_buffer_used = 0;

    /* Allocate read buffer */
    ctx->read_buffer_size = ARGO_BUFFER_STANDARD;
    ctx->read_buffer = malloc(ctx->read_buffer_size);
    if (!ctx->read_buffer) {
        argo_report_error(E_SYSTEM_MEMORY, "io_channel_create_http", "read buffer allocation");
        free(ctx->write_buffer);
        curl_easy_cleanup(ctx->curl);
        free(ctx);
        return NULL;
    }
    ctx->read_buffer_used = 0;

    LOG_INFO("Created HTTP I/O channel for workflow %s (daemon: %s)",
             workflow_id, daemon_url);

    return (io_channel_t*)ctx;
}

/* Exported wrapper functions for io_channel.c to call */

int io_channel_http_write(io_channel_t* channel, const void* data, size_t len) {
    if (!channel || channel->type != IO_CHANNEL_HTTP) {
        return E_INVALID_PARAMS;
    }

    http_io_context_t* ctx = (http_io_context_t*)channel;

    /* Buffer the data */
    if (ctx->write_buffer_used + len > ctx->write_buffer_size) {
        /* Flush existing buffer first */
        int result = http_flush_output_internal(ctx);
        if (result != ARGO_SUCCESS) {
            return result;
        }
    }

    /* If still doesn't fit, write directly */
    if (len > ctx->write_buffer_size) {
        /* For very large writes, we'd need to write directly - for now just fail */
        return E_INPUT_TOO_LARGE;
    }

    /* Buffer the data */
    memcpy(ctx->write_buffer + ctx->write_buffer_used, data, len);
    ctx->write_buffer_used += len;

    return ARGO_SUCCESS;
}

int io_channel_http_flush(io_channel_t* channel) {
    if (!channel || channel->type != IO_CHANNEL_HTTP) {
        return E_INVALID_PARAMS;
    }

    http_io_context_t* ctx = (http_io_context_t*)channel;
    return http_flush_output_internal(ctx);
}

int io_channel_http_read_line(io_channel_t* channel, char* buffer, size_t max_len) {
    if (!channel || channel->type != IO_CHANNEL_HTTP) {
        return E_INVALID_PARAMS;
    }

    http_io_context_t* ctx = (http_io_context_t*)channel;
    return http_poll_input_internal(ctx, buffer, max_len);
}

void io_channel_http_close(io_channel_t* channel) {
    if (!channel || channel->type != IO_CHANNEL_HTTP) {
        return;
    }

    http_io_context_t* ctx = (http_io_context_t*)channel;

    /* Flush any remaining output */
    http_flush_output_internal(ctx);

    channel->is_open = false;
}

void io_channel_http_free(io_channel_t* channel) {
    if (!channel || channel->type != IO_CHANNEL_HTTP) {
        return;
    }

    http_io_context_t* ctx = (http_io_context_t*)channel;

    /* Flush before cleanup */
    http_flush_output_internal(ctx);

    /* Cleanup CURL */
    if (ctx->curl) {
        curl_easy_cleanup(ctx->curl);
    }

    /* Free buffers */
    free(ctx->write_buffer);
    free(ctx->read_buffer);
    free(ctx);
}
