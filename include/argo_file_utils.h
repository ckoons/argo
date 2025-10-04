/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_FILE_UTILS_H
#define ARGO_FILE_UTILS_H

#include <stddef.h>

/*
 * File Utility Functions
 *
 * Centralized file operations to reduce code duplication across the codebase.
 * These utilities handle common patterns like reading entire files with proper
 * error handling and memory management.
 */

/* Read entire file into memory
 *
 * Args:
 *   path: Path to file to read
 *   buffer: Pointer to receive allocated buffer (caller must free)
 *   size: Pointer to receive file size (can be NULL)
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_SYSTEM_FILE if file cannot be opened
 *   E_SYSTEM_MEMORY if allocation fails
 *   E_INVALID_PARAMS if path or buffer is NULL
 *
 * Note: Buffer is null-terminated and caller must free() it
 */
int file_read_all(const char* path, char** buffer, size_t* size);

#endif /* ARGO_FILE_UTILS_H */
