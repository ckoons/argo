/* © 2025 Casey Koons All rights reserved */

/* Test Harness: Terminal Interface
 *
 * Purpose: Interactive terminal for exploring Argo functionality
 * Features:
 *   - Environment variable inspection
 *   - Basic command interface
 *   - Foundation for expanding into full terminal app
 *
 * Commands:
 *   help        - Show command list
 *   env         - Show all environment variables
 *   env NAME    - Show specific variable
 *   set NAME VALUE - Set environment variable
 *   unset NAME  - Remove environment variable
 *   reload      - Reload environment from files
 *   quit/exit   - Exit terminal
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "argo_init.h"
#include "argo_env_utils.h"
#include "argo_error.h"

static void print_banner(void);
static void print_help(void);
static void handle_command(const char* cmd);
static void trim_string(char* str);

int main(void) {
    /* Initialize Argo */
    if (argo_init() != ARGO_SUCCESS) {
        fprintf(stderr, "Fatal: Argo initialization failed\n");
        fprintf(stderr, "Ensure .env.argo exists in current or parent directory\n");
        return 1;
    }

    print_banner();

    /* Command loop */
    char command[1024];
    while (true) {
        printf("argo> ");
        fflush(stdout);

        /* Read command */
        if (!fgets(command, sizeof(command), stdin)) {
            break;  /* EOF */
        }

        /* Remove newline */
        command[strcspn(command, "\n")] = 0;
        trim_string(command);

        /* Skip empty lines */
        if (command[0] == '\0') {
            continue;
        }

        /* Check for exit */
        if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
            break;
        }

        /* Handle command */
        handle_command(command);
    }

    /* Cleanup */
    argo_exit();
    printf("\nGoodbye!\n");
    return 0;
}

static void print_banner(void) {
    printf("\n");
    printf("========================================\n");
    printf("ARGO TERMINAL\n");
    printf("========================================\n");
    printf("\n");
    printf("ARGO_ROOT: %s\n", argo_getenv("ARGO_ROOT") ?: "(not set)");
    printf("\n");
    printf("Type 'help' for commands, 'quit' to exit\n");
    printf("\n");
}

static void print_help(void) {
    printf("\n");
    printf("Available Commands:\n");
    printf("-------------------\n");
    printf("  help             - Show this help\n");
    printf("  env              - Show all environment variables\n");
    printf("  env NAME         - Show specific variable\n");
    printf("  set NAME VALUE   - Set environment variable\n");
    printf("  unset NAME       - Remove environment variable\n");
    printf("  reload           - Reload environment from files\n");
    printf("  quit / exit      - Exit terminal\n");
    printf("\n");
}

static void handle_command(const char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        print_help();
    }
    else if (strcmp(cmd, "env") == 0) {
        printf("\n");
        argo_env_print();
        printf("\n");
    }
    else if (strncmp(cmd, "env ", 4) == 0) {
        const char* name = cmd + 4;
        trim_string((char*)name);
        const char* val = argo_getenv(name);
        if (val) {
            printf("%s=%s\n", name, val);
        } else {
            printf("%s: not set\n", name);
        }
    }
    else if (strncmp(cmd, "set ", 4) == 0) {
        char* args = (char*)(cmd + 4);
        trim_string(args);

        /* Find space between NAME and VALUE */
        char* space = strchr(args, ' ');
        if (!space) {
            printf("Usage: set NAME VALUE\n");
            return;
        }

        *space = '\0';
        char* name = args;
        char* value = space + 1;
        trim_string(name);
        trim_string(value);

        if (argo_setenv(name, value) == ARGO_SUCCESS) {
            printf("Set %s=%s\n", name, value);
        } else {
            printf("Error: Failed to set variable\n");
        }
    }
    else if (strncmp(cmd, "unset ", 6) == 0) {
        const char* name = cmd + 6;
        trim_string((char*)name);

        if (argo_unsetenv(name) == ARGO_SUCCESS) {
            printf("Unset %s\n", name);
        } else {
            printf("Error: Failed to unset variable\n");
        }
    }
    else if (strcmp(cmd, "reload") == 0) {
        printf("Reloading environment...\n");
        argo_exit();
        if (argo_init() == ARGO_SUCCESS) {
            printf("Reload complete\n");
            printf("ARGO_ROOT: %s\n", argo_getenv("ARGO_ROOT") ?: "(not set)");
        } else {
            printf("Error: Reload failed\n");
        }
    }
    else {
        printf("Unknown command: %s\n", cmd);
        printf("Type 'help' for command list\n");
    }
}

static void trim_string(char* str) {
    if (!str) return;

    /* Trim leading whitespace */
    char* start = str;
    while (*start && (*start == ' ' || *start == '\t')) {
        start++;
    }

    /* Trim trailing whitespace */
    char* end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t')) {
        end--;
    }
    *(end + 1) = '\0';

    /* Move trimmed string to beginning */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}
