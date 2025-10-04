/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

/* Project includes */
#include "argo_json.h"
#include "argo_error.h"
#include "argo_workflow_executor.h"
#include "argo_output.h"

/* External library */
#define JSMN_HEADER
#include "jsmn.h"

/* Workflow step structure */
typedef struct {
    char step_name[STEP_NAME_MAX];
    char step_type[STEP_TYPE_MAX];
    char prompt[STEP_PROMPT_MAX];
} workflow_step_t;

/* Workflow state structure */
typedef struct {
    char workflow_id[WORKFLOW_ID_MAX];
    char template_path[TEMPLATE_PATH_MAX];
    char branch[BRANCH_NAME_MAX];
    int current_step;
    int total_steps;
    bool is_paused;
    workflow_step_t steps[MAX_WORKFLOW_STEPS];
} workflow_state_t;

/* Signal handling state */
static volatile sig_atomic_t pause_requested = 0;
static volatile sig_atomic_t shutdown_requested = 0;

/* Signal handlers */
static void handle_pause(int sig) {
    (void)sig;
    pause_requested = 1;
}

static void handle_resume(int sig) {
    (void)sig;
    pause_requested = 0;
}

static void handle_shutdown(int sig) {
    (void)sig;
    shutdown_requested = 1;
}

/* Setup signal handlers for workflow control */
static void setup_signal_handlers(void) {
    struct sigaction sa_pause;
    struct sigaction sa_resume;
    struct sigaction sa_shutdown;

    /* SIGUSR1: Pause at next checkpoint */
    memset(&sa_pause, 0, sizeof(sa_pause));
    sa_pause.sa_handler = handle_pause;
    sigemptyset(&sa_pause.sa_mask);
    sa_pause.sa_flags = 0;
    sigaction(SIGUSR1, &sa_pause, NULL);

    /* SIGUSR2: Resume execution */
    memset(&sa_resume, 0, sizeof(sa_resume));
    sa_resume.sa_handler = handle_resume;
    sigemptyset(&sa_resume.sa_mask);
    sa_resume.sa_flags = 0;
    sigaction(SIGUSR2, &sa_resume, NULL);

    /* SIGTERM: Graceful shutdown */
    memset(&sa_shutdown, 0, sizeof(sa_shutdown));
    sa_shutdown.sa_handler = handle_shutdown;
    sigemptyset(&sa_shutdown.sa_mask);
    sa_shutdown.sa_flags = 0;
    sigaction(SIGTERM, &sa_shutdown, NULL);
}

/* Get checkpoint file path */
static void get_checkpoint_path(const char* workflow_id, char* path, size_t path_size) {
    const char* home = getenv("HOME");
    if (!home) home = ".";
    snprintf(path, path_size, "%s/%s/%s.json", home, CHECKPOINT_DIR, workflow_id);
}

/* Load workflow template from JSON file */
static int load_template(const char* template_path, workflow_state_t* state) {
    FILE* file = fopen(template_path, "r");
    if (!file) {
        LOG_WORKFLOW_ERROR("Failed to open template: %s\n", template_path);
        return -1;
    }

    char buffer[TEMPLATE_BUFFER_SIZE];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[bytes_read] = '\0';

    /* Parse JSON */
    jsmn_parser parser;
    jsmntok_t tokens[256];
    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, buffer, bytes_read, tokens, 256);
    if (token_count < 0) {
        LOG_WORKFLOW_ERROR("Failed to parse template JSON\n");
        return -1;
    }

    /* Find steps array */
    int steps_idx = -1;
    for (int i = 0; i < token_count; i++) {
        if (tokens[i].type == JSMN_STRING) {
            int len = tokens[i].end - tokens[i].start;
            if (len == JSON_FIELD_STEPS_LEN &&
                strncmp(buffer + tokens[i].start, "steps", JSON_FIELD_STEPS_LEN) == 0) {
                steps_idx = i + 1;
                break;
            }
        }
    }

    if (steps_idx == -1 || tokens[steps_idx].type != JSMN_ARRAY) {
        LOG_WORKFLOW_ERROR("No steps array found in template\n");
        return -1;
    }

    /* Parse steps */
    state->total_steps = tokens[steps_idx].size;
    if (state->total_steps > MAX_WORKFLOW_STEPS) {
        state->total_steps = MAX_WORKFLOW_STEPS;
    }

    int current_token = steps_idx + 1;
    for (int step_num = 0; step_num < state->total_steps; step_num++) {
        workflow_step_t* step = &state->steps[step_num];

        /* Find step object */
        if (tokens[current_token].type != JSMN_OBJECT) break;

        int obj_size = tokens[current_token].size;
        current_token++;

        /* Parse step fields */
        for (int field = 0; field < obj_size; field++) {
            if (tokens[current_token].type != JSMN_STRING) break;

            int key_len = tokens[current_token].end - tokens[current_token].start;
            const char* key = buffer + tokens[current_token].start;
            current_token++;

            int val_len = tokens[current_token].end - tokens[current_token].start;
            const char* val = buffer + tokens[current_token].start;

            if (key_len == JSON_FIELD_STEP_LEN &&
                strncmp(key, "step", JSON_FIELD_STEP_LEN) == 0) {
                snprintf(step->step_name, sizeof(step->step_name), "%.*s", val_len, val);
            } else if (key_len == JSON_FIELD_TYPE_LEN &&
                       strncmp(key, "type", JSON_FIELD_TYPE_LEN) == 0) {
                snprintf(step->step_type, sizeof(step->step_type), "%.*s", val_len, val);
            } else if (key_len == JSON_FIELD_PROMPT_LEN &&
                       strncmp(key, "prompt", JSON_FIELD_PROMPT_LEN) == 0) {
                snprintf(step->prompt, sizeof(step->prompt), "%.*s", val_len, val);
            }
            current_token++;
        }
    }

    return 0;
}

/* Save checkpoint */
static int save_checkpoint(workflow_state_t* state) {
    char checkpoint_path[CHECKPOINT_PATH_MAX];
    get_checkpoint_path(state->workflow_id, checkpoint_path, sizeof(checkpoint_path));

    /* Create checkpoint directory */
    char mkdir_cmd[CHECKPOINT_PATH_MAX + 50];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p $(dirname %s)", checkpoint_path);
    system(mkdir_cmd);

    FILE* file = fopen(checkpoint_path, "w");
    if (!file) {
        LOG_WORKFLOW_ERROR("Failed to create checkpoint file: %s\n", checkpoint_path);
        return -1;
    }

    LOG_WORKFLOW("{\n");
    LOG_WORKFLOW("  \"workflow_id\": \"%s\",\n", state->workflow_id);
    LOG_WORKFLOW("  \"template_path\": \"%s\",\n", state->template_path);
    LOG_WORKFLOW("  \"branch\": \"%s\",\n", state->branch);
    LOG_WORKFLOW("  \"current_step\": %d,\n", state->current_step);
    LOG_WORKFLOW("  \"total_steps\": %d,\n", state->total_steps);
    LOG_WORKFLOW("  \"is_paused\": %s\n", state->is_paused ? "true" : "false");
    LOG_WORKFLOW("}\n");

    fclose(file);
    return 0;
}

/* Load checkpoint if it exists */
static int load_checkpoint(workflow_state_t* state) {
    char checkpoint_path[CHECKPOINT_PATH_MAX];
    get_checkpoint_path(state->workflow_id, checkpoint_path, sizeof(checkpoint_path));

    FILE* file = fopen(checkpoint_path, "r");
    if (!file) {
        return -1;  /* No checkpoint exists */
    }

    char buffer[1024];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[bytes_read] = '\0';

    /* Simple parsing - find current_step value */
    const char* current_step_str = strstr(buffer, JSON_CURRENT_STEP_FIELD);
    if (current_step_str) {
        sscanf(current_step_str + JSON_CURRENT_STEP_OFFSET, "%d", &state->current_step);
    }

    return 0;
}

/* Execute a single workflow step */
static void execute_step(workflow_step_t* step, int step_num, int total_steps) {
    LOG_WORKFLOW("\n");
    LOG_WORKFLOW("========================================\n");
    LOG_WORKFLOW("Step %d/%d: %s\n", step_num + 1, total_steps, step->step_name);
    LOG_WORKFLOW("========================================\n");
    LOG_WORKFLOW("Type: %s\n", step->step_type);
    LOG_WORKFLOW("Task: %s\n", step->prompt);
    LOG_WORKFLOW("----------------------------------------\n");
    LOG_WORKFLOW("Executing...\n");

    /* Simulate step execution */
    sleep(STEP_EXECUTION_DELAY_SEC);

    LOG_WORKFLOW("Step completed successfully\n");
}

/* Check if paused and wait for resume */
static void check_pause_state(workflow_state_t* state) {
    if (pause_requested && !state->is_paused) {
        state->is_paused = true;
        save_checkpoint(state);
        LOG_WORKFLOW("\n>>> Workflow PAUSED at step %d/%d\n",
                state->current_step + 1, state->total_steps);
    }

    while (pause_requested && !shutdown_requested) {
        sleep(PAUSE_POLL_DELAY_SEC);
    }

    if (state->is_paused && !pause_requested) {
        state->is_paused = false;
        save_checkpoint(state);
        LOG_WORKFLOW(">>> Workflow RESUMED from step %d/%d\n\n",
                state->current_step + 1, state->total_steps);
    }
}

/* Main workflow executor */
int main(int argc, char** argv) {
    if (argc < 4) {
        LOG_WORKFLOW_ERROR("Usage: %s <workflow_id> <template_path> <branch>\n", argv[0]);
        return 1;
    }

    /* Initialize workflow state */
    workflow_state_t state;
    memset(&state, 0, sizeof(state));
    strncpy(state.workflow_id, argv[1], sizeof(state.workflow_id) - 1);
    strncpy(state.template_path, argv[2], sizeof(state.template_path) - 1);
    strncpy(state.branch, argv[3], sizeof(state.branch) - 1);

    /* Setup signal handlers */
    setup_signal_handlers();

    LOG_WORKFLOW("========================================\n");
    LOG_WORKFLOW("Argo Workflow Executor\n");
    LOG_WORKFLOW("========================================\n");
    LOG_WORKFLOW("Workflow ID: %s\n", state.workflow_id);
    LOG_WORKFLOW("Template:    %s\n", state.template_path);
    LOG_WORKFLOW("Branch:      %s\n", state.branch);
    LOG_WORKFLOW("PID:         %d\n", getpid());
    LOG_WORKFLOW("========================================\n");

    /* Load workflow template */
    if (load_template(state.template_path, &state) != 0) {
        LOG_WORKFLOW_ERROR("Failed to load workflow template\n");
        return 1;
    }

    LOG_WORKFLOW("Loaded template with %d steps\n", state.total_steps);

    /* Try to restore from checkpoint */
    if (load_checkpoint(&state) == 0) {
        LOG_WORKFLOW("Resuming from checkpoint at step %d\n", state.current_step + 1);
    } else {
        LOG_WORKFLOW("Starting fresh execution\n");
        state.current_step = 0;
    }

    LOG_WORKFLOW("========================================\n\n");

    /* Main execution loop */
    while (state.current_step < state.total_steps && !shutdown_requested) {
        /* Check if paused */
        check_pause_state(&state);

        if (shutdown_requested) {
            break;
        }

        /* Execute current step */
        execute_step(&state.steps[state.current_step], state.current_step, state.total_steps);

        /* Move to next step */
        state.current_step++;

        /* Save checkpoint after each step */
        save_checkpoint(&state);
    }

    /* Cleanup checkpoint file if completed */
    char checkpoint_path[CHECKPOINT_PATH_MAX];
    get_checkpoint_path(state.workflow_id, checkpoint_path, sizeof(checkpoint_path));

    if (shutdown_requested) {
        LOG_WORKFLOW("\n========================================\n");
        LOG_WORKFLOW("Workflow %s TERMINATED by signal\n", state.workflow_id);
        LOG_WORKFLOW("Completed %d/%d steps\n", state.current_step, state.total_steps);
        LOG_WORKFLOW("Checkpoint saved: %s\n", checkpoint_path);
        LOG_WORKFLOW("========================================\n");
        return 2;
    }

    LOG_WORKFLOW("\n========================================\n");
    LOG_WORKFLOW("Workflow %s COMPLETED successfully\n", state.workflow_id);
    LOG_WORKFLOW("All %d steps executed\n", state.total_steps);
    LOG_WORKFLOW("========================================\n");

    /* Remove checkpoint on successful completion */
    unlink(checkpoint_path);

    return 0;
}
