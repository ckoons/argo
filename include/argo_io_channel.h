/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_IO_CHANNEL_H
#define ARGO_IO_CHANNEL_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @file argo_io_channel.h
 * @brief Unified socket-based I/O abstraction for daemon and executor
 *
 * This module provides a clean abstraction over socket-based I/O for background
 * services (daemon and executor). It completely replaces stdin/stdout usage
 * in these components with socket-based communication.
 *
 * Key principle: Arc is the ONLY component that touches terminal stdin/stdout.
 * Daemon and executor are background services that use sockets exclusively.
 */

/**
 * I/O channel types
 */
typedef enum {
    IO_CHANNEL_SOCKET,      /* Unix socket */
    IO_CHANNEL_SOCKETPAIR,  /* socketpair(2) for parent-child communication */
    IO_CHANNEL_NULL         /* Null device (discard all output) */
} io_channel_type_t;

/**
 * I/O channel structure
 *
 * Provides buffered, non-blocking I/O over sockets.
 * Thread-safe for single reader/writer pairs.
 */
typedef struct io_channel {
    io_channel_type_t type;
    int read_fd;           /* Read file descriptor */
    int write_fd;          /* Write file descriptor */

    /* Read buffer (for line-based reading) */
    char* read_buffer;
    size_t read_buffer_size;
    size_t read_buffer_used;

    /* Write buffer (for batching small writes) */
    char* write_buffer;
    size_t write_buffer_size;
    size_t write_buffer_used;

    bool non_blocking;     /* Use non-blocking I/O */
    bool is_open;          /* Channel is open and usable */
} io_channel_t;

/**
 * Create an I/O channel from a socket file descriptor
 *
 * @param socket_fd Socket file descriptor (will be used for both read and write)
 * @param non_blocking Enable non-blocking I/O
 * @return Allocated io_channel_t, or NULL on failure
 */
io_channel_t* io_channel_create_socket(int socket_fd, bool non_blocking);

/**
 * Create an I/O channel from a socketpair
 *
 * @param read_fd Read file descriptor
 * @param write_fd Write file descriptor
 * @param non_blocking Enable non-blocking I/O
 * @return Allocated io_channel_t, or NULL on failure
 */
io_channel_t* io_channel_create_pair(int read_fd, int write_fd, bool non_blocking);

/**
 * Create a null I/O channel (discards all output, reads return EOF)
 *
 * Useful for workflows that don't need I/O.
 *
 * @return Allocated io_channel_t, or NULL on failure
 */
io_channel_t* io_channel_create_null(void);

/**
 * Write data to the channel
 *
 * @param channel I/O channel
 * @param data Data to write
 * @param len Length of data
 * @return ARGO_SUCCESS or error code
 */
int io_channel_write(io_channel_t* channel, const void* data, size_t len);

/**
 * Write a null-terminated string to the channel
 *
 * @param channel I/O channel
 * @param str String to write
 * @return ARGO_SUCCESS or error code
 */
int io_channel_write_str(io_channel_t* channel, const char* str);

/**
 * Flush write buffer to socket
 *
 * @param channel I/O channel
 * @return ARGO_SUCCESS or error code
 */
int io_channel_flush(io_channel_t* channel);

/**
 * Read a line from the channel (up to newline or max_len)
 *
 * Strips trailing newline. Returns E_IO_WOULDBLOCK if non-blocking
 * and no complete line available.
 *
 * @param channel I/O channel
 * @param buffer Output buffer
 * @param max_len Maximum bytes to read (including null terminator)
 * @return ARGO_SUCCESS or error code (E_IO_EOF on end-of-stream)
 */
int io_channel_read_line(io_channel_t* channel, char* buffer, size_t max_len);

/**
 * Read exactly len bytes from the channel
 *
 * Blocks until len bytes available or EOF. Returns E_IO_WOULDBLOCK
 * if non-blocking and insufficient data available.
 *
 * @param channel I/O channel
 * @param buffer Output buffer
 * @param len Number of bytes to read
 * @return ARGO_SUCCESS or error code (E_IO_EOF on end-of-stream)
 */
int io_channel_read(io_channel_t* channel, void* buffer, size_t len);

/**
 * Check if data is available to read (non-blocking)
 *
 * @param channel I/O channel
 * @return true if data available, false otherwise
 */
bool io_channel_has_data(io_channel_t* channel);

/**
 * Close the I/O channel
 *
 * Flushes write buffer before closing. Safe to call multiple times.
 *
 * @param channel I/O channel
 */
void io_channel_close(io_channel_t* channel);

/**
 * Free I/O channel resources
 *
 * Closes channel if still open. Safe to call with NULL.
 *
 * @param channel I/O channel to free
 */
void io_channel_free(io_channel_t* channel);

#endif /* ARGO_IO_CHANNEL_H */
