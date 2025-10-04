/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "argo_term.h"

int main(void) {
    char prompt_format[ARGO_TERM_PROMPT_MAX_SIZE];
    char prompt_expanded[ARGO_TERM_PROMPT_MAX_SIZE];
    char input[ARGO_TERM_INPUT_BUFFER_SIZE];

    /* Initialize prompt with default */
    strncpy(prompt_format, ARGO_TERM_DEFAULT_PROMPT, sizeof(prompt_format) - 1);
    prompt_format[sizeof(prompt_format) - 1] = '\0';

    /* Try to load custom prompt from config */
    argo_term_load_config(ARGO_TERM_CONFIG_FILE, prompt_format, sizeof(prompt_format));

    /* Main REPL loop */
    while (1) {
        /* Expand prompt format specifiers */
        if (argo_term_expand_prompt(prompt_format, prompt_expanded, sizeof(prompt_expanded)) != 0) {
            /* Expansion failed - use format as-is */
            strncpy(prompt_expanded, prompt_format, sizeof(prompt_expanded) - 1);
            prompt_expanded[sizeof(prompt_expanded) - 1] = '\0';
        }

        /* Display prompt */
        printf("%s", prompt_expanded);
        fflush(stdout);

        /* Read input */
        if (!fgets(input, sizeof(input), stdin)) {
            /* EOF or error - exit gracefully */
            printf("\n");
            break;
        }

        /* Remove trailing newline */
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }

        /* Check for exit commands */
        if (strcmp(input, ARGO_TERM_CMD_EXIT) == 0 ||
            strcmp(input, ARGO_TERM_CMD_QUIT) == 0) {
            break;
        }

        /* For now, just echo that we received input */
        /* Future: parse and execute arc commands */
    }

    return ARGO_TERM_EXIT_SUCCESS;
}
