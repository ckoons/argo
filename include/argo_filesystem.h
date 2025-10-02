/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_FILESYSTEM_H
#define ARGO_FILESYSTEM_H

/*
 * Argo Filesystem Constants
 *
 * This header defines file permissions, directory modes, and other
 * filesystem-related constants used throughout the Argo system.
 */

/* ===== File Permissions ===== */

/* Session and working memory files - owner read/write only */
#define ARGO_FILE_MODE_PRIVATE 0600

/* Standard read-only file for owner, readable by others */
#define ARGO_FILE_MODE_READONLY 0644

/* ===== Directory Permissions ===== */

/* Standard directory mode - rwxr-xr-x (owner full, others read+execute) */
#define ARGO_DIR_MODE_STANDARD 0755

/* Private directory - rwx------ (owner only) */
#define ARGO_DIR_MODE_PRIVATE 0700

#endif /* ARGO_FILESYSTEM_H */
