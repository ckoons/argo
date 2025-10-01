/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

/* Project includes */
#include "argo_ci.h"
#include "argo_ci_defaults.h"
#include "argo_ci_common.h"
#include "argo_api_common.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"
#include "argo_ollama.h"

/* Ollama context structure */
typedef struct ollama_context {
    /* Configuration */
    char endpoint[OLLAMA_ENDPOINT_SIZE];
    char model[OLLAMA_MODEL_SIZE];
    int port;
    bool use_streaming;  /* Default true */

    /* Connection state */
    int socket_fd;
    bool connected;

    /* Streaming buffer */
    char buffer[OLLAMA_BUFFER_SIZE];
    size_t buffer_pos;

    /* Response accumulator */
    char* response_content;
    size_t response_size;
    size_t response_capacity;

    /* Statistics */
    uint64_t total_queries;
    uint64_t total_tokens;
    time_t last_query;

    /* Provider interface */
    ci_provider_t provider;
} ollama_context_t;

/* Static function declarations */
static int ollama_init(ci_provider_t* provider);
static int ollama_connect(ci_provider_t* provider);
static int ollama_query(ci_provider_t* provider, const char* prompt,
                       ci_response_callback callback, void* userdata);
static int ollama_stream(ci_provider_t* provider, const char* prompt,
                        ci_stream_callback callback, void* userdata);
static void ollama_cleanup(ci_provider_t* provider);
static int ollama_disconnect(ollama_context_t* ctx);

/* HTTP helpers */
static int build_http_request(char* buffer, size_t size,
                             const char* model, const char* prompt, bool streaming);
static int connect_to_ollama(const char* host, int port);

/* Create Ollama provider */
ci_provider_t* ollama_create_provider(const char* model_name) {
    ollama_context_t* ctx = calloc(1, sizeof(ollama_context_t));
    if (!ctx) {
        argo_report_error(E_SYSTEM_MEMORY, "ollama_create_provider", ERR_MSG_MEMORY_ALLOC_FAILED);
        return NULL;
    }

    /* Setup provider interface */
    init_provider_base(&ctx->provider, ctx, ollama_init, ollama_connect,
                      ollama_query, ollama_stream, ollama_cleanup);

    /* Set model configuration */
    if (model_name) {
        strncpy(ctx->model, model_name, sizeof(ctx->model) - 1);
    } else {
        strncpy(ctx->model, OLLAMA_DEFAULT_MODEL, sizeof(ctx->model) - 1);
    }

    /* Look up model defaults */
    const ci_model_config_t* config = ci_get_model_defaults(ctx->model);
    if (config) {
        ctx->provider.supports_streaming = config->supports_streaming;
        ctx->provider.supports_memory = false;  /* Ollama doesn't track memory */
        ctx->provider.max_context = config->max_context;
        strncpy(ctx->provider.name, "ollama", sizeof(ctx->provider.name) - 1);
        strncpy(ctx->provider.model, ctx->model, sizeof(ctx->provider.model) - 1);
    }

    /* Set endpoint */
    strncpy(ctx->endpoint, OLLAMA_DEFAULT_ENDPOINT, sizeof(ctx->endpoint) - 1);
    ctx->port = OLLAMA_DEFAULT_PORT;
    ctx->socket_fd = -1;
    ctx->use_streaming = true;  /* Default to streaming */

    LOG_INFO("Created Ollama provider for model %s", ctx->model);
    return &ctx->provider;
}

/* Initialize Ollama provider */
static int ollama_init(ci_provider_t* provider) {
    ARGO_GET_CONTEXT(provider, ollama_context_t, ctx);

    /* Allocate response buffer using common utility */
    int result = api_allocate_response_buffer(&ctx->response_content,
                                              &ctx->response_capacity,
                                              OLLAMA_RESPONSE_CAPACITY);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    LOG_DEBUG("Ollama provider initialized");
    return ARGO_SUCCESS;
}

/* Connect to Ollama server */
static int ollama_connect(ci_provider_t* provider) {
    ARGO_GET_CONTEXT(provider, ollama_context_t, ctx);

    if (ctx->connected) {
        return ARGO_SUCCESS;
    }

    /* Connect to Ollama */
    ctx->socket_fd = connect_to_ollama("localhost", ctx->port);
    if (ctx->socket_fd < 0) {
        argo_report_error(E_CI_NO_PROVIDER, "ollama_connect", ERR_FMT_FAILED_AT_PORT, ctx->port);
        return E_CI_NO_PROVIDER;
    }

    ctx->connected = true;
    LOG_INFO("Connected to Ollama at localhost:%d", ctx->port);
    return ARGO_SUCCESS;
}

/* Query Ollama (async with callback) */
static int ollama_query(ci_provider_t* provider, const char* prompt,
                       ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, ollama_context_t, ctx);

    /* Ensure connected */
    ARGO_ENSURE_CONNECTED(provider, ctx);

    /* Build HTTP request - use non-streaming for regular query */
    char request[OLLAMA_REQUEST_BUFFER_SIZE];
    int req_len = build_http_request(request, sizeof(request), ctx->model, prompt, false);
    if (req_len < 0) {
        return E_PROTOCOL_SIZE;
    }

    /* Send request */
    ssize_t sent = send(ctx->socket_fd, request, req_len, MSG_NOSIGNAL);
    if (sent != req_len) {
        argo_report_error(E_SYSTEM_SOCKET, "ollama_query", ERR_FMT_SYSCALL_ERROR, ERR_MSG_SOCKET_SEND_FAILED, strerror(errno));
        ollama_disconnect(ctx);
        return E_SYSTEM_SOCKET;
    }

    /* Clear buffers */
    ctx->response_size = 0;
    ctx->buffer_pos = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
    if (ctx->response_content) {
        ctx->response_content[0] = '\0';
    }

    /* Read the complete HTTP response */
    time_t start = time(NULL);
    bool headers_complete = false;
    char* json_start = NULL;

    while (time(NULL) - start < OLLAMA_TIMEOUT_SECONDS) {
        ssize_t bytes = recv(ctx->socket_fd,
                           ctx->buffer + ctx->buffer_pos,
                           sizeof(ctx->buffer) - ctx->buffer_pos - 1,
                           MSG_DONTWAIT);

        if (bytes > 0) {
            ctx->buffer_pos += bytes;
            ctx->buffer[ctx->buffer_pos] = '\0';

            /* Look for end of HTTP headers if not found yet */
            if (!headers_complete) {
                char* header_end = strstr(ctx->buffer, "\r\n\r\n");
                if (header_end) {
                    json_start = header_end + 4;
                    headers_complete = true;
                }
            }

            /* If we have headers and see closing brace, we're done */
            if (headers_complete && json_start) {
                /* Simple check for complete JSON - look for final } */
                char* last_brace = strrchr(json_start, '}');
                if (last_brace) {
                    /* Null terminate right after the closing brace */
                    *(last_brace + 1) = '\0';
                    break;
                }
            }
        } else if (bytes == 0) {
            break;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            argo_report_error(E_SYSTEM_SOCKET, "ollama_query", ERR_FMT_SYSCALL_ERROR, ERR_MSG_RECV_ERROR, strerror(errno));
            ollama_disconnect(ctx);
            return E_SYSTEM_SOCKET;
        }

        usleep(OLLAMA_POLL_DELAY_US);
    }

    if (!json_start) {
        argo_report_error(E_PROTOCOL_FORMAT, "ollama_query", ERR_MSG_NO_JSON_IN_RESPONSE);
        ollama_disconnect(ctx);
        return E_PROTOCOL_FORMAT;
    }

    /* Simple extraction of response field - jsmn has issues with emojis */
    char* response_start = strstr(json_start, "\"response\":\"");
    if (!response_start) {
        argo_report_error(E_PROTOCOL_FORMAT, "ollama_query", ERR_MSG_NO_RESPONSE_FIELD);
        ollama_disconnect(ctx);
        return E_PROTOCOL_FORMAT;
    }

    response_start += OLLAMA_RESPONSE_FIELD_OFFSET;

    /* Find the closing quote, handling escapes */
    char* p = response_start;
    while (*p) {
        if (*p == '"' && *(p-1) != '\\') {
            break;
        }
        p++;
    }

    if (*p != '"') {
        argo_report_error(E_PROTOCOL_FORMAT, "ollama_query", ERR_MSG_UNTERMINATED_STRING);
        ollama_disconnect(ctx);
        return E_PROTOCOL_FORMAT;
    }

    /* Copy response */
    size_t response_len = p - response_start;
    int result = ensure_buffer_capacity(&ctx->response_content, &ctx->response_capacity,
                                       response_len + 1);
    if (result != ARGO_SUCCESS) {
        ollama_disconnect(ctx);
        return result;
    }

    strncpy(ctx->response_content, response_start, response_len);
    ctx->response_content[response_len] = '\0';
    ctx->response_size = response_len;

    /* Disconnect after each query (Ollama doesn't keep connections) */
    ollama_disconnect(ctx);

    /* Build response */
    ci_response_t response;
    build_ci_response(&response, true, ARGO_SUCCESS,
                     ctx->response_content, ctx->model);

    /* Update statistics */
    ARGO_UPDATE_STATS(ctx);

    /* Invoke callback */
    callback(&response, userdata);

    LOG_DEBUG("Ollama query completed, response size: %zu", ctx->response_size);
    return ARGO_SUCCESS;
}

/* Stream response from Ollama */
static int ollama_stream(ci_provider_t* provider, const char* prompt,
                        ci_stream_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, ollama_context_t, ctx);

    /* Ensure connected */
    ARGO_ENSURE_CONNECTED(provider, ctx);

    /* Build HTTP request with streaming enabled */
    char request[OLLAMA_REQUEST_BUFFER_SIZE];
    int req_len = build_http_request(request, sizeof(request), ctx->model, prompt, true);
    if (req_len < 0) {
        return E_PROTOCOL_SIZE;
    }

    /* Send request */
    ssize_t sent = send(ctx->socket_fd, request, req_len, MSG_NOSIGNAL);
    if (sent != req_len) {
        argo_report_error(E_SYSTEM_SOCKET, "ollama_stream", ERR_FMT_SYSCALL_ERROR, ERR_MSG_SOCKET_SEND_FAILED, strerror(errno));
        ollama_disconnect(ctx);
        return E_SYSTEM_SOCKET;
    }

    /* Clear buffers */
    ctx->buffer_pos = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));

    /* Read streaming response */
    time_t start = time(NULL);
    bool headers_complete = false;
    char line_buffer[OLLAMA_LINE_BUFFER_SIZE];

    while (time(NULL) - start < OLLAMA_TIMEOUT_SECONDS) {
        ssize_t bytes = recv(ctx->socket_fd,
                           ctx->buffer + ctx->buffer_pos,
                           sizeof(ctx->buffer) - ctx->buffer_pos - 1,
                           MSG_DONTWAIT);

        if (bytes > 0) {
            ctx->buffer_pos += bytes;
            ctx->buffer[ctx->buffer_pos] = '\0';

            /* Skip headers if not done yet */
            if (!headers_complete) {
                char* header_end = strstr(ctx->buffer, "\r\n\r\n");
                if (header_end) {
                    /* Move past headers */
                    char* json_start = header_end + 4;
                    size_t remaining = ctx->buffer_pos - (json_start - ctx->buffer);
                    memmove(ctx->buffer, json_start, remaining);
                    ctx->buffer_pos = remaining;
                    headers_complete = true;
                }
            }

            if (headers_complete) {
                /* Process lines from buffer */
                char* p = ctx->buffer;
                char* line_start = ctx->buffer;

                while (p < ctx->buffer + ctx->buffer_pos) {
                    if (*p == '\n') {
                        /* Complete line found - copy to line buffer */
                        size_t line_len = p - line_start;
                        if (line_len > 0 && line_len < sizeof(line_buffer)) {
                            memcpy(line_buffer, line_start, line_len);
                            line_buffer[line_len] = '\0';

                            /* Parse JSON line and extract response */
                            char* response_field = strstr(line_buffer, "\"response\":\"");
                            if (response_field) {
                                response_field += 12;
                                char* end = response_field;
                                while (*end && !(*end == '"' && *(end-1) != '\\')) end++;

                                /* Extract the chunk */
                                size_t chunk_len = end - response_field;
                                char chunk[OLLAMA_CHUNK_SIZE];
                                if (chunk_len < sizeof(chunk)) {
                                    strncpy(chunk, response_field, chunk_len);
                                    chunk[chunk_len] = '\0';

                                    /* Call callback with chunk */
                                    callback(chunk, chunk_len, userdata);
                                }
                            }

                            /* Check if done */
                            if (strstr(line_buffer, "\"done\":true")) {
                                /* Streaming complete */
                                ollama_disconnect(ctx);
                                ARGO_UPDATE_STATS(ctx);
                                return ARGO_SUCCESS;
                            }
                        }

                        /* Move to next line */
                        line_start = p + 1;
                    }
                    p++;
                }

                /* Move remaining partial line to start of buffer */
                size_t remaining = ctx->buffer + ctx->buffer_pos - line_start;
                if (remaining > 0) {
                    memmove(ctx->buffer, line_start, remaining);
                    ctx->buffer_pos = remaining;
                } else {
                    ctx->buffer_pos = 0;
                }
            }
        } else if (bytes == 0) {
            break;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            argo_report_error(E_SYSTEM_SOCKET, "ollama_stream", ERR_FMT_SYSCALL_ERROR, ERR_MSG_RECV_ERROR, strerror(errno));
            ollama_disconnect(ctx);
            return E_SYSTEM_SOCKET;
        }

        usleep(OLLAMA_POLL_DELAY_US);
    }

    ollama_disconnect(ctx);
    return E_CI_TIMEOUT;
}

/* Cleanup Ollama provider */
static void ollama_cleanup(ci_provider_t* provider) {
    if (!provider) return;

    ollama_context_t* ctx = (ollama_context_t*)provider->context;
    if (!ctx) return;

    /* Disconnect if connected */
    if (ctx->connected) {
        ollama_disconnect(ctx);
    }

    /* Free response buffer */
    if (ctx->response_content) {
        free(ctx->response_content);
    }

    LOG_INFO("Ollama provider cleanup: queries=%llu tokens=%llu",
             ctx->total_queries, ctx->total_tokens);

    free(ctx);
}

/* Disconnect from Ollama */
static int ollama_disconnect(ollama_context_t* ctx) {
    if (ctx->socket_fd >= 0) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }
    ctx->connected = false;
    return ARGO_SUCCESS;
}

/* Build HTTP POST request for Ollama */
static int build_http_request(char* buffer, size_t size,
                             const char* model, const char* prompt, bool streaming) {
    /* Build JSON body */
    char json_body[OLLAMA_JSON_BODY_SIZE];
    int json_len = snprintf(json_body, sizeof(json_body),
                           OLLAMA_REQUEST_FORMAT,
                           model, prompt, streaming ? "true" : "false");

    if (json_len >= (int)sizeof(json_body)) {
        return -1;
    }

    /* Build HTTP request */
    int req_len = snprintf(buffer, size,
                          "POST /api/generate HTTP/1.1\r\n"
                          "Host: localhost\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: %d\r\n"
                          "\r\n"
                          "%s",
                          json_len, json_body);

    if (req_len >= (int)size) {
        return -1;
    }

    return req_len;
}

/* Streaming control functions */
void ollama_set_streaming(ci_provider_t* provider, bool enabled) {
    if (!provider || !provider->context) return;
    ollama_context_t* ctx = (ollama_context_t*)provider->context;
    ctx->use_streaming = enabled;
}

bool ollama_get_streaming(ci_provider_t* provider) {
    if (!provider || !provider->context) return false;
    ollama_context_t* ctx = (ollama_context_t*)provider->context;
    return ctx->use_streaming;
}

/* Default query - uses streaming by default */
int ollama_query_default(ci_provider_t* provider, const char* prompt,
                        ci_stream_callback callback, void* userdata) {
    if (!provider || !provider->context) {
        return -1;
    }

    ollama_context_t* ctx = (ollama_context_t*)provider->context;

    /* Use streaming if enabled (default) */
    if (ctx->use_streaming && provider->stream) {
        return provider->stream(provider, prompt, callback, userdata);
    } else if (provider->query) {
        /* Fall back to non-streaming, but need to adapt callback */
        /* This would require a wrapper - for now just use streaming */
        return provider->stream(provider, prompt, callback, userdata);
    }

    return E_INTERNAL_NOTIMPL;
}

/* Connect to Ollama server */
static int connect_to_ollama(const char* host, int port) {
    (void)host;  /* Always using localhost */
    struct sockaddr_in addr;
    int sock_fd;

    /* Create socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        argo_report_error(E_SYSTEM_SOCKET, "connect_to_ollama", ERR_FMT_SYSCALL_ERROR, ERR_MSG_SOCKET_CREATE_FAILED, strerror(errno));
        return -1;
    }

    /* Set socket options */
    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Setup address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* Connect */
    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        argo_report_error(E_SYSTEM_SOCKET, "connect_to_ollama", ERR_FMT_SYSCALL_ERROR, ERR_MSG_SOCKET_CONNECT_FAILED, strerror(errno));
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}