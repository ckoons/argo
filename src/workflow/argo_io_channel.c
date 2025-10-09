/* © 2025 Casey Koons All rights reserved */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "argo_io_channel.h"
#include "argo_io_channel_http.h"
#include "argo_error.h"
#include "argo_limits.h"

/* Buffer sizes for I/O channels */
#define IO_READ_BUFFER_SIZE ARGO_BUFFER_STANDARD
#define IO_WRITE_BUFFER_SIZE ARGO_BUFFER_STANDARD

/**
 * Set file descriptor to non-blocking mode
 */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return E_SYSTEM_IO;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return E_SYSTEM_IO;
    }
    return ARGO_SUCCESS;
}

io_channel_t* io_channel_create_socket(int socket_fd, bool non_blocking) {
    if (socket_fd < 0) {
        argo_report_error(E_INVALID_PARAMS, "io_channel_create_socket", "invalid socket_fd");
        return NULL;
    }

    io_channel_t* channel = calloc(1, sizeof(io_channel_t));
    if (!channel) {
        argo_report_error(E_SYSTEM_MEMORY, "io_channel_create_socket", "allocation failed");
        return NULL;
    }

    channel->type = IO_CHANNEL_SOCKET;
    channel->read_fd = socket_fd;
    channel->write_fd = socket_fd;
    channel->non_blocking = non_blocking;
    channel->is_open = true;

    /* Allocate read buffer */
    channel->read_buffer = malloc(IO_READ_BUFFER_SIZE);
    if (!channel->read_buffer) {
        argo_report_error(E_SYSTEM_MEMORY, "io_channel_create_socket", "read buffer allocation failed");
        free(channel);
        return NULL;
    }
    channel->read_buffer_size = IO_READ_BUFFER_SIZE;
    channel->read_buffer_used = 0;

    /* Allocate write buffer */
    channel->write_buffer = malloc(IO_WRITE_BUFFER_SIZE);
    if (!channel->write_buffer) {
        argo_report_error(E_SYSTEM_MEMORY, "io_channel_create_socket", "write buffer allocation failed");
        free(channel->read_buffer);
        free(channel);
        return NULL;
    }
    channel->write_buffer_size = IO_WRITE_BUFFER_SIZE;
    channel->write_buffer_used = 0;

    /* Set non-blocking mode if requested */
    if (non_blocking && set_nonblocking(socket_fd) != ARGO_SUCCESS) {
        argo_report_error(E_SYSTEM_IO, "io_channel_create_socket", "failed to set non-blocking");
        free(channel->write_buffer);
        free(channel->read_buffer);
        free(channel);
        return NULL;
    }

    return channel;
}

io_channel_t* io_channel_create_pair(int read_fd, int write_fd, bool non_blocking) {
    if (read_fd < 0 || write_fd < 0) {
        argo_report_error(E_INVALID_PARAMS, "io_channel_create_pair", "invalid file descriptors");
        return NULL;
    }

    io_channel_t* channel = calloc(1, sizeof(io_channel_t));
    if (!channel) {
        argo_report_error(E_SYSTEM_MEMORY, "io_channel_create_pair", "allocation failed");
        return NULL;
    }

    channel->type = IO_CHANNEL_SOCKETPAIR;
    channel->read_fd = read_fd;
    channel->write_fd = write_fd;
    channel->non_blocking = non_blocking;
    channel->is_open = true;

    /* Allocate read buffer */
    channel->read_buffer = malloc(IO_READ_BUFFER_SIZE);
    if (!channel->read_buffer) {
        argo_report_error(E_SYSTEM_MEMORY, "io_channel_create_pair", "read buffer allocation failed");
        free(channel);
        return NULL;
    }
    channel->read_buffer_size = IO_READ_BUFFER_SIZE;
    channel->read_buffer_used = 0;

    /* Allocate write buffer */
    channel->write_buffer = malloc(IO_WRITE_BUFFER_SIZE);
    if (!channel->write_buffer) {
        argo_report_error(E_SYSTEM_MEMORY, "io_channel_create_pair", "write buffer allocation failed");
        free(channel->read_buffer);
        free(channel);
        return NULL;
    }
    channel->write_buffer_size = IO_WRITE_BUFFER_SIZE;
    channel->write_buffer_used = 0;

    /* Set non-blocking mode if requested */
    if (non_blocking) {
        if (set_nonblocking(read_fd) != ARGO_SUCCESS || set_nonblocking(write_fd) != ARGO_SUCCESS) {
            argo_report_error(E_SYSTEM_IO, "io_channel_create_pair", "failed to set non-blocking");
            free(channel->write_buffer);
            free(channel->read_buffer);
            free(channel);
            return NULL;
        }
    }

    return channel;
}

io_channel_t* io_channel_create_null(void) {
    io_channel_t* channel = calloc(1, sizeof(io_channel_t));
    if (!channel) {
        argo_report_error(E_SYSTEM_MEMORY, "io_channel_create_null", "allocation failed");
        return NULL;
    }

    channel->type = IO_CHANNEL_NULL;
    channel->read_fd = -1;
    channel->write_fd = -1;
    channel->is_open = true;
    channel->non_blocking = false;

    /* Null channel doesn't need buffers */
    return channel;
}

int io_channel_write(io_channel_t* channel, const void* data, size_t len) {
    if (!channel || !data) {
        argo_report_error(E_INVALID_PARAMS, "io_channel_write", "null parameter");
        return E_INVALID_PARAMS;
    }

    if (!channel->is_open) {
        argo_report_error(E_IO_INVALID, "io_channel_write", "channel closed");
        return E_IO_INVALID;
    }

    /* Dispatch to HTTP implementation */
    if (channel->type == IO_CHANNEL_HTTP) {
        return io_channel_http_write(channel, data, len);
    }

    /* Null channel discards all output */
    if (channel->type == IO_CHANNEL_NULL) {
        return ARGO_SUCCESS;
    }

    /* If data fits in buffer, buffer it */
    if (channel->write_buffer && len <= (channel->write_buffer_size - channel->write_buffer_used)) {
        memcpy(channel->write_buffer + channel->write_buffer_used, data, len);
        channel->write_buffer_used += len;
        return ARGO_SUCCESS;
    }

    /* Buffer full or no buffer - flush then write directly */
    int result = io_channel_flush(channel);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Write data directly */
    ssize_t written = write(channel->write_fd, data, len);
    if (written < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return E_IO_WOULDBLOCK;
        }
        argo_report_error(E_SYSTEM_IO, "io_channel_write", strerror(errno));
        return E_SYSTEM_IO;
    }

    if ((size_t)written < len) {
        argo_report_error(E_SYSTEM_IO, "io_channel_write", "partial write");
        return E_SYSTEM_IO;
    }

    return ARGO_SUCCESS;
}

int io_channel_write_str(io_channel_t* channel, const char* str) {
    if (!str) {
        argo_report_error(E_INVALID_PARAMS, "io_channel_write_str", "null string");
        return E_INVALID_PARAMS;
    }
    return io_channel_write(channel, str, strlen(str));
}

int io_channel_flush(io_channel_t* channel) {
    if (!channel) {
        argo_report_error(E_INVALID_PARAMS, "io_channel_flush", "null channel");
        return E_INVALID_PARAMS;
    }

    if (!channel->is_open) {
        return ARGO_SUCCESS;  /* Already closed */
    }

    /* Dispatch to HTTP implementation */
    if (channel->type == IO_CHANNEL_HTTP) {
        return io_channel_http_flush(channel);
    }

    if (channel->type == IO_CHANNEL_NULL) {
        return ARGO_SUCCESS;  /* Nothing to flush */
    }

    if (channel->write_buffer_used == 0) {
        return ARGO_SUCCESS;  /* Nothing buffered */
    }

    /* Write buffered data */
    ssize_t written = write(channel->write_fd, channel->write_buffer, channel->write_buffer_used);
    if (written < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return E_IO_WOULDBLOCK;
        }
        argo_report_error(E_SYSTEM_IO, "io_channel_flush", strerror(errno));
        return E_SYSTEM_IO;
    }

    if ((size_t)written < channel->write_buffer_used) {
        /* Partial write - move remaining data to start of buffer */
        size_t remaining = channel->write_buffer_used - written;
        memmove(channel->write_buffer, channel->write_buffer + written, remaining);
        channel->write_buffer_used = remaining;
        return E_IO_WOULDBLOCK;  /* Could not flush everything */
    }

    /* All data written */
    channel->write_buffer_used = 0;
    return ARGO_SUCCESS;
}

int io_channel_read_line(io_channel_t* channel, char* buffer, size_t max_len) {
    if (!channel || !buffer || max_len == 0) {
        argo_report_error(E_INVALID_PARAMS, "io_channel_read_line", "invalid parameters");
        return E_INVALID_PARAMS;
    }

    if (!channel->is_open) {
        argo_report_error(E_IO_INVALID, "io_channel_read_line", "channel closed");
        return E_IO_INVALID;
    }

    /* Dispatch to HTTP implementation */
    if (channel->type == IO_CHANNEL_HTTP) {
        return io_channel_http_read_line(channel, buffer, max_len);
    }

    /* Null channel always returns EOF */
    if (channel->type == IO_CHANNEL_NULL) {
        return E_IO_EOF;
    }

    size_t buffer_pos = 0;

    while (buffer_pos < max_len - 1) {
        /* Check if we have buffered data */
        if (channel->read_buffer_used > 0) {
            /* Look for newline in buffered data */
            char* newline = memchr(channel->read_buffer, '\n', channel->read_buffer_used);
            if (newline) {
                /* Found newline - copy up to (but not including) newline */
                size_t line_len = newline - channel->read_buffer;
                size_t copy_len = (line_len < max_len - buffer_pos - 1) ? line_len : max_len - buffer_pos - 1;
                memcpy(buffer + buffer_pos, channel->read_buffer, copy_len);
                buffer_pos += copy_len;

                /* Remove line from buffer (including newline) */
                size_t consumed = line_len + 1;
                channel->read_buffer_used -= consumed;
                memmove(channel->read_buffer, channel->read_buffer + consumed, channel->read_buffer_used);

                /* Null-terminate and return */
                buffer[buffer_pos] = '\0';
                return ARGO_SUCCESS;
            }

            /* No newline yet - copy all buffered data and continue reading */
            size_t copy_len = (channel->read_buffer_used < max_len - buffer_pos - 1) ?
                              channel->read_buffer_used : max_len - buffer_pos - 1;
            memcpy(buffer + buffer_pos, channel->read_buffer, copy_len);
            buffer_pos += copy_len;
            channel->read_buffer_used = 0;
        }

        /* Read more data into buffer */
        ssize_t bytes_read = read(channel->read_fd, channel->read_buffer,
                                 channel->read_buffer_size);
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (buffer_pos > 0) {
                    /* Have partial data - return what we have */
                    buffer[buffer_pos] = '\0';
                    return E_IO_WOULDBLOCK;
                }
                return E_IO_WOULDBLOCK;
            }
            argo_report_error(E_SYSTEM_IO, "io_channel_read_line", strerror(errno));
        return E_SYSTEM_IO;
        }

        if (bytes_read == 0) {
            /* EOF - return what we have if anything */
            if (buffer_pos > 0) {
                buffer[buffer_pos] = '\0';
                return ARGO_SUCCESS;
            }
            return E_IO_EOF;
        }

        channel->read_buffer_used = bytes_read;
    }

    /* Buffer full without finding newline */
    buffer[max_len - 1] = '\0';
    argo_report_error(E_BUFFER_OVERFLOW, "io_channel_read_line", "line too long");
        return E_BUFFER_OVERFLOW;
}

int io_channel_read(io_channel_t* channel, void* buffer, size_t len) {
    if (!channel || !buffer || len == 0) {
        argo_report_error(E_INVALID_PARAMS, "io_channel_read", "invalid parameters");
        return E_INVALID_PARAMS;
    }

    if (!channel->is_open) {
        argo_report_error(E_IO_INVALID, "io_channel_read", "channel closed");
        return E_IO_INVALID;
    }

    if (channel->type == IO_CHANNEL_NULL) {
        return E_IO_EOF;
    }

    size_t total_read = 0;
    char* buf = (char*)buffer;

    /* First consume any buffered data */
    if (channel->read_buffer_used > 0) {
        size_t copy_len = (channel->read_buffer_used < len) ? channel->read_buffer_used : len;
        memcpy(buf, channel->read_buffer, copy_len);
        total_read += copy_len;
        channel->read_buffer_used -= copy_len;
        memmove(channel->read_buffer, channel->read_buffer + copy_len, channel->read_buffer_used);

        if (total_read == len) {
            return ARGO_SUCCESS;
        }
    }

    /* Read remaining data directly */
    while (total_read < len) {
        ssize_t bytes_read = read(channel->read_fd, buf + total_read, len - total_read);
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return (total_read > 0) ? E_IO_WOULDBLOCK : E_IO_WOULDBLOCK;
            }
            argo_report_error(E_SYSTEM_IO, "io_channel_read", strerror(errno));
        return E_SYSTEM_IO;
        }

        if (bytes_read == 0) {
            return (total_read > 0) ? ARGO_SUCCESS : E_IO_EOF;
        }

        total_read += bytes_read;
    }

    return ARGO_SUCCESS;
}

bool io_channel_has_data(io_channel_t* channel) {
    if (!channel || !channel->is_open) {
        return false;
    }

    if (channel->type == IO_CHANNEL_NULL) {
        return false;
    }

    /* Check buffered data first */
    if (channel->read_buffer_used > 0) {
        return true;
    }

    /* Try a non-blocking read of 1 byte */
    char test_byte;
    int old_flags = fcntl(channel->read_fd, F_GETFL, 0);
    fcntl(channel->read_fd, F_SETFL, old_flags | O_NONBLOCK);

    ssize_t result = read(channel->read_fd, &test_byte, 1);

    fcntl(channel->read_fd, F_SETFL, old_flags);

    if (result > 0) {
        /* Put byte back into buffer */
        if (channel->read_buffer_used < channel->read_buffer_size) {
            channel->read_buffer[channel->read_buffer_used++] = test_byte;
        }
        return true;
    }

    return false;
}

void io_channel_close(io_channel_t* channel) {
    if (!channel || !channel->is_open) {
        return;
    }

    /* Dispatch to HTTP implementation */
    if (channel->type == IO_CHANNEL_HTTP) {
        io_channel_http_close(channel);
        return;
    }

    /* Flush write buffer */
    io_channel_flush(channel);

    /* Close file descriptors */
    if (channel->type == IO_CHANNEL_SOCKET) {
        if (channel->read_fd >= 0) {
            close(channel->read_fd);
        }
    } else if (channel->type == IO_CHANNEL_SOCKETPAIR) {
        if (channel->read_fd >= 0) {
            close(channel->read_fd);
        }
        if (channel->write_fd >= 0 && channel->write_fd != channel->read_fd) {
            close(channel->write_fd);
        }
    }

    channel->is_open = false;
}

void io_channel_free(io_channel_t* channel) {
    if (!channel) {
        return;
    }

    /* Dispatch to HTTP implementation */
    if (channel->type == IO_CHANNEL_HTTP) {
        io_channel_http_free(channel);
        return;
    }

    io_channel_close(channel);

    free(channel->read_buffer);
    free(channel->write_buffer);
    free(channel);
}
