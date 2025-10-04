/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_TERM_H
#define ARGO_TERM_H

#include <stdlib.h>

/* Configuration */
#define ARGO_TERM_DEFAULT_PROMPT "argo> "
#define ARGO_TERM_CONFIG_FILE ".argorc"
#define ARGO_TERM_CONFIG_KEY "ARGO_TERM_PROMPT="
#define ARGO_TERM_CONFIG_KEY_LEN 17
#define ARGO_TERM_INPUT_BUFFER_SIZE 4096
#define ARGO_TERM_PROMPT_MAX_SIZE 1024

/* Exit commands */
#define ARGO_TERM_CMD_EXIT "exit"
#define ARGO_TERM_CMD_QUIT "quit"

/* Exit codes */
#define ARGO_TERM_EXIT_SUCCESS 0
#define ARGO_TERM_EXIT_ERROR 1

/* ANSI color codes */
#define ARGO_TERM_COLOR_RESET "\033[0m"
#define ARGO_TERM_COLOR_BLACK "\033[30m"
#define ARGO_TERM_COLOR_RED "\033[31m"
#define ARGO_TERM_COLOR_GREEN "\033[32m"
#define ARGO_TERM_COLOR_YELLOW "\033[33m"
#define ARGO_TERM_COLOR_BLUE "\033[34m"
#define ARGO_TERM_COLOR_MAGENTA "\033[35m"
#define ARGO_TERM_COLOR_CYAN "\033[36m"
#define ARGO_TERM_COLOR_WHITE "\033[37m"

/* Config functions */
int argo_term_load_config(const char* config_file, char* prompt, size_t prompt_size);

/* Prompt expansion functions */
int argo_term_expand_prompt(const char* format, char* output, size_t output_size);

#endif /* ARGO_TERM_H */
