/* © 2025 Casey Koons All rights reserved */

/* String utility functions - generic and Argo-specific */

/* System includes */
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

/* Project includes */
#include "argo_string_utils.h"

/* Trim whitespace from both ends of a string (in-place) */
char* trim_whitespace(char* str) {
    if (!str) return NULL;

    /* Trim leading whitespace */
    char* start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    /* If string is all whitespace */
    if (*start == '\0') {
        str[0] = '\0';
        return str;
    }

    /* Trim trailing whitespace */
    char* end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';

    /* Move trimmed string to beginning if needed */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }

    return str;
}

/* Trim whitespace and convert to lowercase (in-place) */
char* trim_lower(char* str) {
    if (!str) return NULL;

    /* First trim whitespace */
    trim_whitespace(str);

    /* Convert to lowercase */
    for (char* p = str; *p; p++) {
        *p = tolower((unsigned char)*p);
    }

    return str;
}

/* Fuzzy scan for pattern allowing whitespace variations */
const char* fuzzy_scan(const char* target, const char* pattern) {
    if (!target || !pattern) return NULL;
    if (*pattern == '\0') return target;

    const char* t = target;
    const char* p = pattern;

    while (*t) {
        /* Skip whitespace in target if pattern expects whitespace */
        if (isspace((unsigned char)*p)) {
            /* Pattern has whitespace - skip any amount in target */
            while (isspace((unsigned char)*t)) t++;
            while (isspace((unsigned char)*p)) p++;
            if (*p == '\0') return target;  /* Pattern matched */
            continue;
        }

        /* Regular character matching */
        if (*t == *p) {
            /* Start of potential match */
            const char* match_start = t;
            const char* tp = t;
            const char* pp = p;

            while (*pp && *tp) {
                /* Handle whitespace in pattern flexibly */
                if (isspace((unsigned char)*pp)) {
                    /* Skip any whitespace in both */
                    while (isspace((unsigned char)*pp)) pp++;
                    while (isspace((unsigned char)*tp)) tp++;
                    continue;
                }

                /* Match regular characters */
                if (*tp == *pp) {
                    tp++;
                    pp++;
                } else {
                    break;  /* Mismatch */
                }
            }

            /* Check if we matched the entire pattern */
            if (*pp == '\0' || (*pp && isspace((unsigned char)*pp) && *(pp+1) == '\0')) {
                return match_start;
            }
        }

        t++;
    }

    return NULL;  /* Pattern not found */
}

/* Validate CI name format */
bool validate_ci_name(const char* name) {
    if (!name || *name == '\0') return false;

    /* First character must be alphanumeric */
    if (!isalnum((unsigned char)*name)) return false;

    /* Rest can be alphanumeric, hyphen, or underscore */
    for (const char* p = name; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_') {
            return false;
        }
    }

    return true;
}

/* Validate role name format */
bool validate_role_name(const char* role) {
    if (!role || *role == '\0') return false;

    /* All characters must be lowercase alphanumeric or hyphen */
    for (const char* p = role; *p; p++) {
        if (!islower((unsigned char)*p) && !isdigit((unsigned char)*p) && *p != '-') {
            return false;
        }
    }

    /* First character must be alphabetic */
    if (!islower((unsigned char)*role)) return false;

    return true;
}

/* Safe string copy with guaranteed null termination */
size_t safe_strncpy(char* dst, const char* src, size_t size) {
    if (!dst || !src || size == 0) return 0;

    size_t i;
    for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';

    return i;
}
