/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_IO_CHANNEL_HTTP_H
#define ARGO_IO_CHANNEL_HTTP_H

#include "argo_io_channel.h"

/**
 * @file argo_io_channel_http.h
 * @brief HTTP-based I/O channel for executor-daemon communication
 *
 * This module provides an io_channel implementation that communicates
 * with the daemon via HTTP endpoints instead of direct sockets.
 *
 * The executor uses this to:
 * - Write output via buffering (flushed periodically to daemon)
 * - Read input by polling daemon's input queue
 *
 * This keeps the executor as a pure background service with no stdin/stdout.
 */

/* JSON encoding constants */
#define JSON_ESCAPE_MAX_MULTIPLIER 6    /* Worst case: char -> \uXXXX */
#define JSON_OVERHEAD_BYTES 100         /* Space for {"output":"..."} */
#define JSON_ENCODING_SAFETY_MARGIN 10  /* Extra bytes for loop termination */

/**
 * Create an HTTP-based I/O channel
 *
 * The returned io_channel will:
 * - Buffer writes and periodically flush to daemon
 * - Poll daemon for input on reads
 *
 * @param daemon_url Base URL of daemon (e.g., "http://localhost:9876")
 * @param workflow_id Workflow ID for this channel
 * @return Allocated io_channel_t, or NULL on failure
 */
io_channel_t* io_channel_create_http(const char* daemon_url, const char* workflow_id);

/**
 * HTTP-specific write (exported for io_channel.c to call)
 */
int io_channel_http_write(io_channel_t* channel, const void* data, size_t len);

/**
 * HTTP-specific flush (exported for io_channel.c to call)
 */
int io_channel_http_flush(io_channel_t* channel);

/**
 * HTTP-specific read_line (exported for io_channel.c to call)
 */
int io_channel_http_read_line(io_channel_t* channel, char* buffer, size_t max_len);

/**
 * HTTP-specific close (exported for io_channel.c to call)
 */
void io_channel_http_close(io_channel_t* channel);

/**
 * HTTP-specific free (exported for io_channel.c to call)
 */
void io_channel_http_free(io_channel_t* channel);

#endif /* ARGO_IO_CHANNEL_HTTP_H */
