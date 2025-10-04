/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "argo_term.h"

/* Load configuration from file */
int argo_term_load_config(const char* config_file, char* prompt, size_t prompt_size) {
    FILE* fp = NULL;
    char line[1024];
    char filepath[512];
    int found = 0;

    if (!config_file || !prompt || prompt_size == 0) {
        return -1;
    }

    /* Build full path to config file */
    const char* home = getenv("HOME");
    if (!home) {
        return -1;
    }

    snprintf(filepath, sizeof(filepath), "%s/%s", home, config_file);

    /* Open config file */
    fp = fopen(filepath, "r");
    if (!fp) {
        /* Config file not found is not an error - use defaults */
        return 0;
    }

    /* Read line by line looking for ARGO_TERM_PROMPT= */
    while (fgets(line, sizeof(line), fp)) {
        /* Skip empty lines and comments */
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }

        /* Look for ARGO_TERM_PROMPT=value */
        if (strncmp(line, ARGO_TERM_CONFIG_KEY, ARGO_TERM_CONFIG_KEY_LEN) == 0) {
            char* value = line + ARGO_TERM_CONFIG_KEY_LEN;
            size_t len = strlen(value);

            /* Remove trailing newline */
            if (len > 0 && value[len - 1] == '\n') {
                value[len - 1] = '\0';
                len--;
            }

            /* Strip surrounding quotes if present */
            if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
                value++;      /* Skip opening quote */
                len -= 2;     /* Remove both quotes from length */
                value[len] = '\0';  /* Terminate at closing quote position */
            }

            /* Copy to output buffer */
            if (len > 0 && len < prompt_size) {
                strncpy(prompt, value, prompt_size - 1);
                prompt[prompt_size - 1] = '\0';
                found = 1;
                break;
            }
        }
    }

    fclose(fp);
    return found ? 1 : 0;
}
