/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "argo_term.h"

/* Helper: append string to buffer with bounds checking */
static int append_string(char** dest, size_t* remaining, const char* src) {
    size_t len = strlen(src);
    if (len >= *remaining) {
        return -1;  /* Buffer too small */
    }
    strcpy(*dest, src);
    *dest += len;
    *remaining -= len;
    return 0;
}

/* Helper: map color name to ANSI code */
static const char* get_color_code(const char* color_name) {
    if (strcmp(color_name, "black") == 0) return ARGO_TERM_COLOR_BLACK;
    if (strcmp(color_name, "red") == 0) return ARGO_TERM_COLOR_RED;
    if (strcmp(color_name, "green") == 0) return ARGO_TERM_COLOR_GREEN;
    if (strcmp(color_name, "yellow") == 0) return ARGO_TERM_COLOR_YELLOW;
    if (strcmp(color_name, "blue") == 0) return ARGO_TERM_COLOR_BLUE;
    if (strcmp(color_name, "magenta") == 0) return ARGO_TERM_COLOR_MAGENTA;
    if (strcmp(color_name, "cyan") == 0) return ARGO_TERM_COLOR_CYAN;
    if (strcmp(color_name, "white") == 0) return ARGO_TERM_COLOR_WHITE;
    return NULL;  /* Unknown color */
}

/* Helper: get home-relative path */
static int get_home_relative_path(char* output, size_t output_size) {
    char cwd[1024];
    const char* home = getenv("HOME");

    if (!getcwd(cwd, sizeof(cwd))) {
        return -1;
    }

    /* If HOME is set and cwd starts with HOME, replace with ~ */
    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        size_t home_len = strlen(home);
        if (cwd[home_len] == '\0') {
            /* Exactly at home */
            strncpy(output, "~", output_size - 1);
        } else if (cwd[home_len] == '/') {
            /* Inside home directory */
            snprintf(output, output_size, "~%s", cwd + home_len);
        } else {
            /* HOME is a prefix but not a path component */
            strncpy(output, cwd, output_size - 1);
        }
    } else {
        /* Not in home directory */
        strncpy(output, cwd, output_size - 1);
    }

    output[output_size - 1] = '\0';
    return 0;
}


/* Expand prompt format specifiers */
int argo_term_expand_prompt(const char* format, char* output, size_t output_size) {
    char hostname[256];
    char cwd[1024];
    char home_cwd[1024];
    const char* src = format;
    char* dest = output;
    size_t remaining = output_size;

    if (!format || !output || output_size == 0) {
        return -1;
    }

    /* Pre-fetch values that might be needed */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
    }
    hostname[sizeof(hostname) - 1] = '\0';

    if (!getcwd(cwd, sizeof(cwd))) {
        strncpy(cwd, "/", sizeof(cwd) - 1);
    }

    if (get_home_relative_path(home_cwd, sizeof(home_cwd)) != 0) {
        strncpy(home_cwd, cwd, sizeof(home_cwd) - 1);
    }

    /* Process format string */
    while (*src && remaining > 1) {
        if (*src == '%') {
            src++;
            if (*src == '\0') {
                /* Trailing % - treat as literal */
                *dest++ = '%';
                remaining--;
                break;
            }

            switch (*src) {
                case 'h':  /* hostname */
                    if (append_string(&dest, &remaining, hostname) != 0) {
                        return -1;
                    }
                    break;

                case 'd':  /* current working directory */
                    if (append_string(&dest, &remaining, cwd) != 0) {
                        return -1;
                    }
                    break;

                case '~':  /* home-relative directory */
                    if (append_string(&dest, &remaining, home_cwd) != 0) {
                        return -1;
                    }
                    break;

                case 'b':  /* workflow branch (placeholder) */
                    /* TODO: Query registry for active_workflow->active_branch
                     * Requires: registry interface, workflow context, arc switch
                     * For now: always empty string */
                    break;

                case 'F':  /* color start %F{color} */
                    src++;
                    if (*src == '{') {
                        char color_name[32];
                        size_t color_len = 0;
                        src++;
                        /* Extract color name */
                        while (*src && *src != '}' && color_len < sizeof(color_name) - 1) {
                            color_name[color_len++] = *src++;
                        }
                        color_name[color_len] = '\0';

                        if (*src == '}') {
                            /* Valid color syntax */
                            const char* color_code = get_color_code(color_name);
                            if (color_code) {
                                if (append_string(&dest, &remaining, color_code) != 0) {
                                    return -1;
                                }
                            }
                            /* If unknown color, just skip it */
                        } else {
                            /* Malformed - missing closing brace */
                            src--;  /* Back up so we don't skip the char after color_name */
                        }
                    } else {
                        /* Not followed by { - output literally */
                        *dest++ = '%';
                        *dest++ = 'F';
                        remaining -= 2;
                        src--;  /* Back up to reprocess this character */
                    }
                    break;

                case 'f':  /* color reset */
                    if (append_string(&dest, &remaining, ARGO_TERM_COLOR_RESET) != 0) {
                        return -1;
                    }
                    break;

                case '%':  /* literal % */
                    *dest++ = '%';
                    remaining--;
                    break;

                default:
                    /* Unknown specifier - output literally */
                    *dest++ = '%';
                    *dest++ = *src;
                    remaining -= 2;
                    break;
            }
            src++;
        } else if (*src == '\\' && *(src + 1) == 'n') {
            /* Newline escape sequence */
            *dest++ = '\n';
            remaining--;
            src += 2;
        } else {
            /* Literal character */
            *dest++ = *src++;
            remaining--;
        }
    }

    *dest = '\0';
    return 0;
}
