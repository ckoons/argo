/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* External library - header only for struct definitions */
#define JSMN_HEADER
#include "jsmn.h"

/* Project includes */
#include "argo_workflow_steps.h"
#include "argo_workflow.h"
#include "argo_workflow_persona.h"
#include "argo_workflow_json.h"
#include "argo_workflow_conditions.h"
#include "argo_error.h"
#include "argo_log.h"

/* Step: user_ask */
int step_user_ask(const char* json, jsmntok_t* tokens, int step_index,
                  workflow_context_t* ctx) {
    if (!json || !tokens || !ctx) {
        argo_report_error(E_INPUT_NULL, "step_user_ask", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find prompt field */
    int prompt_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PROMPT);
    if (prompt_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_user_ask", "missing prompt");
        return E_PROTOCOL_FORMAT;
    }

    char prompt[STEP_PROMPT_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[prompt_idx], prompt, sizeof(prompt));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find save_to field */
    int save_to_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_SAVE_TO);
    if (save_to_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_user_ask", "missing save_to");
        return E_PROTOCOL_FORMAT;
    }

    char save_to[STEP_SAVE_TO_BUFFER_SIZE];
    result = workflow_json_extract_string(json, &tokens[save_to_idx], save_to, sizeof(save_to));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Show prompt */
    printf("%s ", prompt);
    fflush(stdout);

    /* Read user input */
    char input[STEP_INPUT_BUFFER_SIZE];
    if (!fgets(input, sizeof(input), stdin)) {
        argo_report_error(E_INPUT_INVALID, "step_user_ask", "failed to read input");
        return E_INPUT_INVALID;
    }

    /* Remove trailing newline */
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
    }

    /* Save to context */
    result = workflow_context_set(ctx, save_to, input);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    LOG_DEBUG("User input saved to '%s': %s", save_to, input);
    return ARGO_SUCCESS;
}

/* Step: display */
int step_display(const char* json, jsmntok_t* tokens, int step_index,
                 workflow_context_t* ctx) {
    if (!json || !tokens || !ctx) {
        argo_report_error(E_INPUT_NULL, "step_display", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find message field */
    int message_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_MESSAGE);
    if (message_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_display", "missing message");
        return E_PROTOCOL_FORMAT;
    }

    char template[STEP_OUTPUT_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[message_idx],
                                              template, sizeof(template));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Substitute variables */
    char output[STEP_OUTPUT_BUFFER_SIZE];
    result = workflow_context_substitute(ctx, template, output, sizeof(output));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Display message */
    printf("%s\n", output);

    LOG_DEBUG("Displayed message: %s", output);
    return ARGO_SUCCESS;
}

/* Step: save_file */
int step_save_file(const char* json, jsmntok_t* tokens, int step_index,
                   workflow_context_t* ctx) {
    if (!json || !tokens || !ctx) {
        argo_report_error(E_INPUT_NULL, "step_save_file", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find destination field */
    int dest_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_DESTINATION);
    if (dest_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_save_file", "missing destination");
        return E_PROTOCOL_FORMAT;
    }

    char destination[STEP_DESTINATION_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[dest_idx],
                                              destination, sizeof(destination));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find data field */
    int data_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_DATA);
    if (data_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_save_file", "missing data");
        return E_PROTOCOL_FORMAT;
    }

    /* Add timestamp to context if not present */
    if (!workflow_context_has(ctx, WORKFLOW_JSON_FIELD_TIMESTAMP)) {
        time_t now = time(NULL);
        char timestamp[STEP_TIMESTAMP_BUFFER_SIZE];
        snprintf(timestamp, sizeof(timestamp), "%ld", (long)now);
        workflow_context_set(ctx, WORKFLOW_JSON_FIELD_TIMESTAMP, timestamp);
    }

    /* Extract data object content (without outer braces) */
    jsmntok_t* data_token = &tokens[data_idx];

    /* Skip opening '{' and closing '}' */
    const char* content_start = json + data_token->start + 1;
    int content_len = data_token->end - data_token->start - 2;

    char data_str[STEP_OUTPUT_BUFFER_SIZE];
    if (content_len >= (int)sizeof(data_str) - 3) {  /* -3 for { } \0 */
        argo_report_error(E_INPUT_TOO_LARGE, "step_save_file", "data object too large");
        return E_INPUT_TOO_LARGE;
    }

    strncpy(data_str, content_start, content_len);
    data_str[content_len] = '\0';

    /* Perform variable substitution on content */
    char substituted[STEP_OUTPUT_BUFFER_SIZE];
    result = workflow_context_substitute(ctx, data_str, substituted, sizeof(substituted) - 3);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Write to file with braces restored */
    FILE* f = fopen(destination, "w");
    if (!f) {
        argo_report_error(E_SYSTEM_FILE, "step_save_file", destination);
        return E_SYSTEM_FILE;
    }

    fprintf(f, "{%s}\n", substituted);
    fclose(f);

    printf("Saved workflow data to: %s\n", destination);
    LOG_DEBUG("Saved file: %s", destination);
    return ARGO_SUCCESS;
}

/* Step: decide */
int step_decide(const char* json, jsmntok_t* tokens, int step_index,
                workflow_context_t* ctx,
                char* next_step, size_t next_step_size) {
    if (!json || !tokens || !ctx || !next_step) {
        argo_report_error(E_INPUT_NULL, "step_decide", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find condition field */
    int condition_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_CONDITION);
    if (condition_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_decide", "missing condition");
        return E_PROTOCOL_FORMAT;
    }

    char condition[STEP_OUTPUT_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[condition_idx],
                                              condition, sizeof(condition));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Evaluate condition */
    int condition_result = 0;
    result = workflow_evaluate_condition(ctx, condition, &condition_result);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find appropriate next step */
    const char* next_field = condition_result ? STEP_FIELD_IF_TRUE : STEP_FIELD_IF_FALSE;
    int next_idx = workflow_json_find_field(json, tokens, step_index, next_field);
    if (next_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_decide", next_field);
        return E_PROTOCOL_FORMAT;
    }

    /* Extract next step (can be string or number) */
    if (tokens[next_idx].type == JSMN_STRING) {
        result = workflow_json_extract_string(json, &tokens[next_idx], next_step, next_step_size);
    } else if (tokens[next_idx].type == JSMN_PRIMITIVE) {
        int next_num;
        result = workflow_json_extract_int(json, &tokens[next_idx], &next_num);
        if (result == ARGO_SUCCESS) {
            snprintf(next_step, next_step_size, "%d", next_num);
        }
    } else {
        argo_report_error(E_INPUT_INVALID, "step_decide", "invalid next_step type");
        return E_INPUT_INVALID;
    }

    LOG_DEBUG("Decide: condition=%d, next_step=%s", condition_result, next_step);
    return result;
}

/* Step: ci_ask */
int step_ci_ask(workflow_controller_t* workflow,
                const char* json, jsmntok_t* tokens, int step_index) {
    if (!workflow || !json || !tokens) {
        argo_report_error(E_INPUT_NULL, "step_ci_ask", "parameter is NULL");
        return E_INPUT_NULL;
    }

    workflow_context_t* ctx = workflow->context;

    /* Find persona field (optional) */
    workflow_persona_t* persona = NULL;
    int persona_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PERSONA);
    if (persona_idx >= 0) {
        char persona_name[STEP_PERSONA_BUFFER_SIZE];
        workflow_json_extract_string(json, &tokens[persona_idx],
                                     persona_name, sizeof(persona_name));
        persona = persona_registry_find(workflow->personas, persona_name);
        if (!persona) {
            LOG_DEBUG("Persona '%s' not found, using default", persona_name);
            persona = persona_registry_get_default(workflow->personas);
        }
    }

    /* Find prompt_template field */
    int prompt_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PROMPT_TEMPLATE);
    if (prompt_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_ask", "missing prompt_template");
        return E_PROTOCOL_FORMAT;
    }

    char prompt_template[STEP_PROMPT_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[prompt_idx],
                                              prompt_template, sizeof(prompt_template));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Substitute variables in prompt */
    char prompt[STEP_OUTPUT_BUFFER_SIZE];
    result = workflow_context_substitute(ctx, prompt_template, prompt, sizeof(prompt));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find save_to field */
    int save_to_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_SAVE_TO);
    if (save_to_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_ask", "missing save_to");
        return E_PROTOCOL_FORMAT;
    }

    char save_to[STEP_SAVE_TO_BUFFER_SIZE];
    result = workflow_json_extract_string(json, &tokens[save_to_idx], save_to, sizeof(save_to));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Show persona greeting if available */
    if (persona && persona->greeting[0] != '\0') {
        printf("%s\n", persona->greeting);
    }

    /* Show AI persona's prompt */
    if (persona && persona->name[0] != '\0') {
        printf("[%s] %s ", persona->name, prompt);
    } else {
        printf("%s ", prompt);
    }
    fflush(stdout);

    /* Read user input */
    char input[STEP_INPUT_BUFFER_SIZE];
    if (!fgets(input, sizeof(input), stdin)) {
        argo_report_error(E_INPUT_INVALID, "step_ci_ask", "failed to read input");
        return E_INPUT_INVALID;
    }

    /* Remove trailing newline */
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
    }

    /* Save to context */
    result = workflow_context_set(ctx, save_to, input);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    LOG_DEBUG("CI ask: persona=%s, saved to '%s': %s",
              persona ? persona->name : "none", save_to, input);
    return ARGO_SUCCESS;
}

/* Step: user_choose */
int step_user_choose(const char* json, jsmntok_t* tokens, int step_index,
                     workflow_context_t* ctx,
                     char* next_step, size_t next_step_size) {
    if (!json || !tokens || !ctx || !next_step) {
        argo_report_error(E_INPUT_NULL, "step_user_choose", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find prompt field */
    int prompt_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PROMPT);
    if (prompt_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_user_choose", "missing prompt");
        return E_PROTOCOL_FORMAT;
    }

    char prompt[STEP_PROMPT_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[prompt_idx],
                                              prompt, sizeof(prompt));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find options array */
    int options_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_OPTIONS);
    if (options_idx < 0 || tokens[options_idx].type != JSMN_ARRAY) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_user_choose", "missing or invalid options");
        return E_PROTOCOL_FORMAT;
    }

    int option_count = tokens[options_idx].size;
    if (option_count == 0) {
        argo_report_error(E_INPUT_INVALID, "step_user_choose", "no options provided");
        return E_INPUT_INVALID;
    }

    /* Display prompt and options */
    printf("\n%s\n", prompt);

    int option_token = options_idx + 1;
    for (int i = 0; i < option_count; i++) {
        if (tokens[option_token].type != JSMN_OBJECT) {
            continue;
        }

        /* Get label */
        int label_idx = workflow_json_find_field(json, tokens, option_token, STEP_FIELD_LABEL);
        if (label_idx >= 0) {
            char label[STEP_SAVE_TO_BUFFER_SIZE];
            workflow_json_extract_string(json, &tokens[label_idx], label, sizeof(label));
            printf("  %d. %s\n", i + 1, label);
        }

        /* Skip to next option */
        int option_tokens = workflow_json_count_tokens(tokens, option_token);
        option_token += option_tokens;
    }

    /* Read user input */
    printf("\nSelect option (1-%d): ", option_count);
    fflush(stdout);

    char input[STEP_INPUT_BUFFER_SIZE];
    if (!fgets(input, sizeof(input), stdin)) {
        argo_report_error(E_INPUT_INVALID, "step_user_choose", "failed to read input");
        return E_INPUT_INVALID;
    }

    /* Remove trailing newline */
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
    }

    /* Parse selection */
    char* endptr;
    long selection = strtol(input, &endptr, 10);
    if (*endptr != '\0' || selection < 1 || selection > option_count) {
        argo_report_error(E_INPUT_INVALID, "step_user_choose", "invalid selection");
        return E_INPUT_INVALID;
    }

    /* Find selected option's next_step */
    option_token = options_idx + 1;
    for (int i = 0; i < option_count; i++) {
        if (i == (int)(selection - 1)) {
            /* This is the selected option */
            int next_idx = workflow_json_find_field(json, tokens, option_token, WORKFLOW_JSON_FIELD_NEXT_STEP);
            if (next_idx < 0) {
                argo_report_error(E_PROTOCOL_FORMAT, "step_user_choose", "option missing next_step");
                return E_PROTOCOL_FORMAT;
            }

            /* Extract next step */
            if (tokens[next_idx].type == JSMN_STRING) {
                result = workflow_json_extract_string(json, &tokens[next_idx], next_step, next_step_size);
            } else if (tokens[next_idx].type == JSMN_PRIMITIVE) {
                int next_num;
                result = workflow_json_extract_int(json, &tokens[next_idx], &next_num);
                if (result == ARGO_SUCCESS) {
                    snprintf(next_step, next_step_size, "%d", next_num);
                }
            } else {
                argo_report_error(E_INPUT_INVALID, "step_user_choose", "invalid next_step type");
                return E_INPUT_INVALID;
            }

            LOG_DEBUG("User selected option %ld, next_step=%s", selection, next_step);
            return result;
        }

        /* Skip to next option */
        int option_tokens = workflow_json_count_tokens(tokens, option_token);
        option_token += option_tokens;
    }

    /* Should never reach here */
    argo_report_error(E_INTERNAL_LOGIC, "step_user_choose", "failed to find selected option");
    return E_INTERNAL_LOGIC;
}

/* Step: ci_analyze */
int step_ci_analyze(workflow_controller_t* workflow,
                    const char* json, jsmntok_t* tokens, int step_index) {
    if (!workflow || !json || !tokens) {
        argo_report_error(E_INPUT_NULL, "step_ci_analyze", "parameter is NULL");
        return E_INPUT_NULL;
    }

    workflow_context_t* ctx = workflow->context;

    /* Find persona field (optional) */
    workflow_persona_t* persona = NULL;
    int persona_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PERSONA);
    if (persona_idx >= 0) {
        char persona_name[STEP_PERSONA_BUFFER_SIZE];
        workflow_json_extract_string(json, &tokens[persona_idx],
                                     persona_name, sizeof(persona_name));
        persona = persona_registry_find(workflow->personas, persona_name);
        if (!persona) {
            LOG_DEBUG("Persona '%s' not found, using default", persona_name);
            persona = persona_registry_get_default(workflow->personas);
        }
    }

    /* Find task field */
    int task_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_TASK);
    if (task_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_analyze", "missing task");
        return E_PROTOCOL_FORMAT;
    }

    char task[STEP_TASK_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[task_idx], task, sizeof(task));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find save_to field */
    int save_to_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_SAVE_TO);
    if (save_to_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_analyze", "missing save_to");
        return E_PROTOCOL_FORMAT;
    }

    char save_to[STEP_SAVE_TO_BUFFER_SIZE];
    result = workflow_json_extract_string(json, &tokens[save_to_idx], save_to, sizeof(save_to));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* TODO: Call CI provider to perform analysis */
    /* For now, show persona and task */
    if (persona && persona->name[0] != '\0') {
        printf("[%s - Analysis] %s\n", persona->name, task);
    } else {
        printf("[CI Analysis] %s\n", task);
    }

    LOG_DEBUG("CI analyze: persona=%s, task='%s', save_to='%s'",
              persona ? persona->name : "none", task, save_to);

    /* Save placeholder result to context */
    result = workflow_context_set(ctx, save_to, "{\"analyzed\": true}");

    return result;
}

/* Step: ci_ask_series */
int step_ci_ask_series(workflow_controller_t* workflow,
                       const char* json, jsmntok_t* tokens, int step_index) {
    if (!workflow || !json || !tokens) {
        argo_report_error(E_INPUT_NULL, "step_ci_ask_series", "parameter is NULL");
        return E_INPUT_NULL;
    }

    workflow_context_t* ctx = workflow->context;

    /* Find persona field (optional) */
    workflow_persona_t* persona = NULL;
    int persona_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PERSONA);
    if (persona_idx >= 0) {
        char persona_name[STEP_PERSONA_BUFFER_SIZE];
        workflow_json_extract_string(json, &tokens[persona_idx],
                                     persona_name, sizeof(persona_name));
        persona = persona_registry_find(workflow->personas, persona_name);
        if (!persona) {
            LOG_DEBUG("Persona '%s' not found, using default", persona_name);
            persona = persona_registry_get_default(workflow->personas);
        }
    }

    /* Show persona greeting if available */
    if (persona && persona->greeting[0] != '\0') {
        printf("\n%s\n", persona->greeting);
    }

    /* Find optional intro field */
    int intro_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_INTRO);
    if (intro_idx >= 0) {
        char intro[STEP_PROMPT_BUFFER_SIZE];
        workflow_json_extract_string(json, &tokens[intro_idx], intro, sizeof(intro));
        if (persona && persona->name[0] != '\0') {
            printf("[%s] %s\n", persona->name, intro);
        } else {
            printf("\n%s\n", intro);
        }
    }

    /* Find questions array */
    int questions_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_QUESTIONS);
    if (questions_idx < 0 || tokens[questions_idx].type != JSMN_ARRAY) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_ask_series", "missing or invalid questions");
        return E_PROTOCOL_FORMAT;
    }

    int question_count = tokens[questions_idx].size;
    if (question_count == 0) {
        argo_report_error(E_INPUT_INVALID, "step_ci_ask_series", "no questions provided");
        return E_INPUT_INVALID;
    }

    /* Find save_to field */
    int save_to_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_SAVE_TO);
    if (save_to_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_ask_series", "missing save_to");
        return E_PROTOCOL_FORMAT;
    }

    char save_to[STEP_SAVE_TO_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[save_to_idx], save_to, sizeof(save_to));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Iterate through questions */
    int question_token = questions_idx + 1;
    for (int i = 0; i < question_count; i++) {
        if (tokens[question_token].type != JSMN_OBJECT) {
            question_token++;
            continue;
        }

        /* Get question text */
        int q_idx = workflow_json_find_field(json, tokens, question_token, "question");
        if (q_idx >= 0) {
            char question[STEP_PROMPT_BUFFER_SIZE];
            workflow_json_extract_string(json, &tokens[q_idx], question, sizeof(question));

            /* Ask question with persona name if available */
            if (persona && persona->name[0] != '\0') {
                printf("\n[%s] %d. %s ", persona->name, i + 1, question);
            } else {
                printf("\n%d. %s ", i + 1, question);
            }
            fflush(stdout);

            /* Read answer */
            char answer[STEP_INPUT_BUFFER_SIZE];
            if (fgets(answer, sizeof(answer), stdin)) {
                size_t len = strlen(answer);
                if (len > 0 && answer[len - 1] == '\n') {
                    answer[len - 1] = '\0';
                }

                /* Get question ID and save answer */
                int id_idx = workflow_json_find_field(json, tokens, question_token, "id");
                if (id_idx >= 0) {
                    char id[STEP_SAVE_TO_BUFFER_SIZE];
                    workflow_json_extract_string(json, &tokens[id_idx], id, sizeof(id));

                    /* Build context path: save_to.id */
                    char full_path[STEP_SAVE_TO_BUFFER_SIZE * 2];
                    snprintf(full_path, sizeof(full_path), "%s.%s", save_to, id);
                    workflow_context_set(ctx, full_path, answer);
                }
            }
        }

        /* Skip to next question */
        int question_tokens = workflow_json_count_tokens(tokens, question_token);
        question_token += question_tokens;
    }

    LOG_DEBUG("CI ask_series: persona=%s, completed %d questions, saved to '%s'",
              persona ? persona->name : "none", question_count, save_to);
    printf("\n");
    return ARGO_SUCCESS;
}

/* Step: ci_present */
int step_ci_present(workflow_controller_t* workflow,
                    const char* json, jsmntok_t* tokens, int step_index) {
    if (!workflow || !json || !tokens) {
        argo_report_error(E_INPUT_NULL, "step_ci_present", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find persona field (optional) */
    workflow_persona_t* persona = NULL;
    int persona_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PERSONA);
    if (persona_idx >= 0) {
        char persona_name[STEP_PERSONA_BUFFER_SIZE];
        workflow_json_extract_string(json, &tokens[persona_idx],
                                     persona_name, sizeof(persona_name));
        persona = persona_registry_find(workflow->personas, persona_name);
        if (!persona) {
            LOG_DEBUG("Persona '%s' not found, using default", persona_name);
            persona = persona_registry_get_default(workflow->personas);
        }
    }

    /* Find data field (context path to data) */
    int data_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_DATA);
    if (data_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_present", "missing data");
        return E_PROTOCOL_FORMAT;
    }

    char data_path[STEP_SAVE_TO_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[data_idx], data_path, sizeof(data_path));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find format field */
    int format_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_FORMAT);
    char format[STEP_SAVE_TO_BUFFER_SIZE] = "text";
    if (format_idx >= 0) {
        workflow_json_extract_string(json, &tokens[format_idx], format, sizeof(format));
    }

    /* TODO: Call CI provider to format and present data */
    /* For now, display with persona information */
    printf("\n");
    printf("========================================\n");
    if (persona && persona->name[0] != '\0') {
        printf("[%s] PRESENTATION (%s format)\n", persona->name, format);
    } else {
        printf("PRESENTATION (%s format)\n", format);
    }
    printf("========================================\n");
    printf("Data source: %s\n", data_path);
    printf("(Full presentation would be generated by CI)\n");
    printf("========================================\n");
    printf("\n");

    LOG_DEBUG("CI present: persona=%s, format='%s', data='%s'",
              persona ? persona->name : "none", format, data_path);
    return ARGO_SUCCESS;
}
