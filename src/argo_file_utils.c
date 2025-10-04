/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "argo_file_utils.h"
#include "argo_error.h"
#include "argo_log.h"

/* Read entire file into memory */
int file_read_all(const char* path, char** buffer, size_t* size) {
    if (!path || !buffer) {
        return E_INVALID_PARAMS;
    }

    /* Open file */
    FILE* fp = fopen(path, "r");
    if (!fp) {
        LOG_ERROR("Failed to open file: %s (%s)", path, strerror(errno));
        return E_SYSTEM_FILE;
    }

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0) {
        LOG_ERROR("Failed to get file size: %s", path);
        fclose(fp);
        return E_SYSTEM_FILE;
    }

    /* Allocate buffer (with room for null terminator) */
    char* file_buffer = malloc((size_t)file_size + 1);
    if (!file_buffer) {
        argo_report_error(E_SYSTEM_MEMORY, "file_read_all",
                         "Failed to allocate %ld bytes for %s", file_size + 1, path);
        fclose(fp);
        return E_SYSTEM_MEMORY;
    }

    /* Read file */
    size_t read_size = fread(file_buffer, 1, (size_t)file_size, fp);
    fclose(fp);

    /* Null-terminate */
    file_buffer[read_size] = '\0';

    /* Return results */
    *buffer = file_buffer;
    if (size) {
        *size = read_size;
    }

    return ARGO_SUCCESS;
}
