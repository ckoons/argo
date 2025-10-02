/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_STRING_UTILS_H
#define ARGO_STRING_UTILS_H

#include <stddef.h>
#include <stdbool.h>

/*
 * Argo String Utilities
 *
 * Generic and Argo-specific string manipulation functions.
 * These utilities don't count against the 10,000 line diet budget.
 */

/* ===== Generic String Utilities ===== */

/**
 * Trim whitespace from both ends of a string (in-place).
 * Modifies the string by moving content and null-terminating.
 *
 * @param str String to trim (modified in-place)
 * @return Pointer to the trimmed string (same as input)
 */
char* trim_whitespace(char* str);

/**
 * Trim whitespace and convert to lowercase (in-place).
 * Useful for case-insensitive comparisons and normalized keys.
 *
 * @param str String to process (modified in-place)
 * @return Pointer to the processed string (same as input)
 */
char* trim_lower(char* str);

/**
 * Fuzzy scan for a pattern allowing whitespace variations.
 * Looks for pattern in target, ignoring extra whitespace, newlines, etc.
 * Useful for parsing JSON that might have formatting variations.
 *
 * @param target String to search in
 * @param pattern Pattern to find (whitespace will be flexible)
 * @return Pointer to match location in target, or NULL if not found
 */
const char* fuzzy_scan(const char* target, const char* pattern);

/* ===== Argo-Specific String Utilities ===== */

/**
 * Validate CI name format.
 * CI names must be alphanumeric with optional hyphens/underscores.
 *
 * @param name Name to validate
 * @return true if valid, false otherwise
 */
bool validate_ci_name(const char* name);

/**
 * Validate role name format.
 * Roles must be lowercase alphanumeric with optional hyphens.
 *
 * @param role Role to validate
 * @return true if valid, false otherwise
 */
bool validate_role_name(const char* role);

/**
 * Safe string copy with length limit and null termination guarantee.
 * Unlike strncpy, always null-terminates and doesn't pad with zeros.
 *
 * @param dst Destination buffer
 * @param src Source string
 * @param size Size of destination buffer
 * @return Number of characters copied (not including null terminator)
 */
size_t safe_strncpy(char* dst, const char* src, size_t size);

#endif /* ARGO_STRING_UTILS_H */
