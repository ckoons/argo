/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* External library - header only for struct definitions */
#define JSMN_HEADER
#include "jsmn.h"

/* Project includes */
#include "argo_workflow.h"
#include "argo_workflow_steps.h"
#include "argo_workflow_persona.h"
#include "argo_shutdown.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"
#include "argo_json.h"
#include "argo_env_utils.h"
#include "argo_claude.h"
#include "argo_api_providers.h"
#include "argo_ollama.h"
#include "argo_mock.h"

/* Helper: Generate unique task ID (thread-safe) */
static void generate_task_id(char* id_out, size_t len) {
    static int task_counter = 0;
    static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_lock(&counter_mutex);
    int counter = task_counter++;
    pthread_mutex_unlock(&counter_mutex);

    snprintf(id_out, len, "task-%ld-%d", (long)time(NULL), counter);
}

/* Create provider by name and model */
ci_provider_t* workflow_create_provider_by_name(const char* provider_name,
                                                const char* model_name,
                                                const char* workflow_id) {
    if (!provider_name || !model_name || !workflow_id) {
        LOG_WARN("Invalid provider configuration, using default");
        return claude_code_create_provider(workflow_id);
    }

    LOG_INFO("Creating provider: %s (model: %s)", provider_name, model_name);

    /* Claude Code provider (file-based, no API key) */
    if (strcmp(provider_name, "claude_code") == 0) {
        return claude_code_create_provider(workflow_id);
    }

    /* Claude API provider */
    if (strcmp(provider_name, "claude_api") == 0) {
        if (!claude_api_is_available()) {
            LOG_WARN("Claude API not available (missing ANTHROPIC_API_KEY), using claude_code");
            return claude_code_create_provider(workflow_id);
        }
        return claude_api_create_provider(model_name);
    }

    /* OpenAI API provider */
    if (strcmp(provider_name, "openai_api") == 0) {
        if (!openai_api_is_available()) {
            LOG_WARN("OpenAI API not available (missing OPENAI_API_KEY), using claude_code");
            return claude_code_create_provider(workflow_id);
        }
        return openai_api_create_provider(model_name);
    }

    /* Gemini API provider */
    if (strcmp(provider_name, "gemini_api") == 0) {
        if (!gemini_api_is_available()) {
            LOG_WARN("Gemini API not available (missing GEMINI_API_KEY), using claude_code");
            return claude_code_create_provider(workflow_id);
        }
        return gemini_api_create_provider(model_name);
    }

    /* Grok API provider */
    if (strcmp(provider_name, "grok_api") == 0) {
        if (!grok_api_is_available()) {
            LOG_WARN("Grok API not available (missing GROK_API_KEY), using claude_code");
            return claude_code_create_provider(workflow_id);
        }
        return grok_api_create_provider(model_name);
    }

    /* DeepSeek API provider */
    if (strcmp(provider_name, "deepseek_api") == 0) {
        if (!deepseek_api_is_available()) {
            LOG_WARN("DeepSeek API not available (missing DEEPSEEK_API_KEY), using claude_code");
            return claude_code_create_provider(workflow_id);
        }
        return deepseek_api_create_provider(model_name);
    }

    /* OpenRouter provider */
    if (strcmp(provider_name, "openrouter") == 0) {
        if (!openrouter_is_available()) {
            LOG_WARN("OpenRouter not available (missing OPENROUTER_API_KEY), using claude_code");
            return claude_code_create_provider(workflow_id);
        }
        return openrouter_create_provider(model_name);
    }

    /* Ollama local provider */
    if (strcmp(provider_name, "ollama") == 0) {
        if (!ollama_is_running()) {
            LOG_WARN("Ollama not running (check port %d), using claude_code", OLLAMA_DEFAULT_PORT);
            return claude_code_create_provider(workflow_id);
        }
        return ollama_create_provider(model_name);
    }

    /* Mock provider (for testing) */
    if (strcmp(provider_name, "mock") == 0) {
        return mock_provider_create(model_name);
    }

    /* Unknown provider - fall back to claude_code */
    LOG_WARN("Unknown provider '%s', using claude_code", provider_name);
    return claude_code_create_provider(workflow_id);
}

/* Create workflow controller */
workflow_controller_t* workflow_create(ci_registry_t* registry,
                                      lifecycle_manager_t* lifecycle,
                                      const char* workflow_id) {
    if (!registry || !lifecycle || !workflow_id) {
        argo_report_error(E_INPUT_NULL, "workflow_create", ERR_MSG_NULL_POINTER);
        return NULL;
    }

    workflow_controller_t* workflow = calloc(1, sizeof(workflow_controller_t));
    if (!workflow) {
        argo_report_error(E_SYSTEM_MEMORY, "workflow_create", ERR_MSG_WORKFLOW_ALLOC_FAILED);
        return NULL;
    }

    strncpy(workflow->workflow_id, workflow_id, sizeof(workflow->workflow_id) - 1);
    workflow->current_phase = WORKFLOW_INIT;
    workflow->state = WORKFLOW_STATE_IDLE;
    workflow->registry = registry;
    workflow->lifecycle = lifecycle;

    /* Get provider configuration from environment */
    const char* env_provider = argo_getenv("ARGO_DEFAULT_PROVIDER");
    const char* env_model = argo_getenv("ARGO_DEFAULT_MODEL");

    /* Store configuration in workflow (can be overridden by JSON later) */
    strncpy(workflow->provider_name, env_provider ? env_provider : "claude_code",
            sizeof(workflow->provider_name) - 1);
    strncpy(workflow->model_name, env_model ? env_model : "claude-sonnet-4-5",
            sizeof(workflow->model_name) - 1);

    /* Create provider based on configuration */
    workflow->provider = workflow_create_provider_by_name(workflow->provider_name,
                                                          workflow->model_name,
                                                          workflow_id);
    if (!workflow->provider) {
        LOG_WARN("Failed to create provider, workflow will have no AI");
    } else {
        LOG_INFO("Workflow using provider: %s (model: %s)",
                 workflow->provider_name, workflow->model_name);
    }

    workflow->tasks = NULL;
    workflow->total_tasks = 0;
    workflow->completed_tasks = 0;

    /* Initialize JSON workflow fields */
    workflow->json_workflow = NULL;
    workflow->json_size = 0;
    workflow->tokens = NULL;
    workflow->token_count = 0;
    workflow->context = NULL;
    workflow->current_step_id[0] = '\0';
    workflow->step_count = 0;

    /* Initialize loop tracking */
    workflow->previous_step_id[0] = '\0';
    workflow->loop_start_step_id[0] = '\0';
    workflow->loop_iteration_count = 0;

    /* Initialize persona registry */
    workflow->personas = NULL;

    /* Initialize workflow chaining */
    workflow->recursion_depth = 0;

    /* Initialize retry configuration with defaults */
    workflow->retry_config.max_retries = EXECUTOR_DEFAULT_MAX_RETRIES;
    workflow->retry_config.retry_delay_ms = EXECUTOR_DEFAULT_RETRY_DELAY_MS;
    workflow->retry_config.backoff_multiplier = EXECUTOR_DEFAULT_BACKOFF_MULTIPLIER;

#if ARGO_HAS_DRYRUN
    /* Initialize dry-run mode (disabled by default) */
    workflow->dry_run = 0;
#endif

    /* Register for graceful shutdown tracking */
    argo_register_workflow(workflow);

    LOG_INFO("Created workflow: %s", workflow_id);
    return workflow;
}

/* Destroy workflow controller */
void workflow_destroy(workflow_controller_t* workflow) {
    if (!workflow) return;

    /* Unregister from shutdown tracking */
    argo_unregister_workflow(workflow);

    /* Free all tasks */
    ci_task_t* task = workflow->tasks;
    while (task) {
        ci_task_t* next = task->next;
        free(task);
        task = next;
    }

    /* Free JSON workflow resources */
    free(workflow->json_workflow);
    free(workflow->tokens);
    if (workflow->context) {
        workflow_context_destroy(workflow->context);
    }

    /* Free persona registry */
    if (workflow->personas) {
        persona_registry_destroy(workflow->personas);
    }

    LOG_INFO("Destroyed workflow: %s", workflow->workflow_id);
    free(workflow);
}

/* Start workflow */
int workflow_start(workflow_controller_t* workflow, const char* base_branch) {
    if (!workflow || !base_branch) {
        return E_INVALID_PARAMS;
    }

    if (workflow->state != WORKFLOW_STATE_IDLE) {
        LOG_WARN("Workflow %s already started", workflow->workflow_id);
        return E_INVALID_STATE;
    }

    strncpy(workflow->base_branch, base_branch, sizeof(workflow->base_branch) - 1);
    workflow->state = WORKFLOW_STATE_RUNNING;
    workflow->workflow_start_time = time(NULL);
    workflow->phase_start_time = time(NULL);

    LOG_INFO("Started workflow %s on branch %s", workflow->workflow_id, base_branch);
    return ARGO_SUCCESS;
}

/* Pause workflow */
int workflow_pause(workflow_controller_t* workflow) {
    if (!workflow) {
        return E_INVALID_PARAMS;
    }

    if (workflow->state != WORKFLOW_STATE_RUNNING) {
        return E_INVALID_STATE;
    }

    workflow->state = WORKFLOW_STATE_PAUSED;
    LOG_INFO("Paused workflow: %s", workflow->workflow_id);
    return ARGO_SUCCESS;
}

/* Resume workflow */
int workflow_resume(workflow_controller_t* workflow) {
    if (!workflow) {
        return E_INVALID_PARAMS;
    }

    if (workflow->state != WORKFLOW_STATE_PAUSED) {
        return E_INVALID_STATE;
    }

    workflow->state = WORKFLOW_STATE_RUNNING;
    LOG_INFO("Resumed workflow: %s", workflow->workflow_id);
    return ARGO_SUCCESS;
}

/* Get phase name */
const char* workflow_phase_name(workflow_phase_t phase) {
    switch (phase) {
        case WORKFLOW_INIT: return "Initialize";
        case WORKFLOW_PLAN: return "Planning";
        case WORKFLOW_DEVELOP: return "Development";
        case WORKFLOW_REVIEW: return "Review";
        case WORKFLOW_TEST: return "Testing";
        case WORKFLOW_MERGE: return "Merge";
        case WORKFLOW_COMPLETE: return "Complete";
        default: return "Unknown";
    }
}

/* Check if workflow can advance to next phase */
int workflow_can_advance(workflow_controller_t* workflow) {
    if (!workflow) return 0;

    /* Must be in running state */
    if (workflow->state != WORKFLOW_STATE_RUNNING) {
        return 0;
    }

    /* Check all tasks for current phase are complete */
    ci_task_t* task = workflow->tasks;
    while (task) {
        if (task->phase == workflow->current_phase && !task->completed) {
            return 0;  /* Found incomplete task for current phase */
        }
        task = task->next;
    }

    return 1;  /* All tasks for current phase are complete */
}

/* Advance to next workflow phase */
int workflow_advance_phase(workflow_controller_t* workflow) {
    if (!workflow) {
        return E_INVALID_PARAMS;
    }

    if (!workflow_can_advance(workflow)) {
        LOG_WARN("Cannot advance workflow %s - tasks incomplete", workflow->workflow_id);
        return E_INVALID_STATE;
    }

    /* Move to next phase */
    workflow_phase_t old_phase = workflow->current_phase;

    switch (workflow->current_phase) {
        case WORKFLOW_INIT:
            workflow->current_phase = WORKFLOW_PLAN;
            break;
        case WORKFLOW_PLAN:
            workflow->current_phase = WORKFLOW_DEVELOP;
            break;
        case WORKFLOW_DEVELOP:
            workflow->current_phase = WORKFLOW_REVIEW;
            break;
        case WORKFLOW_REVIEW:
            workflow->current_phase = WORKFLOW_TEST;
            break;
        case WORKFLOW_TEST:
            workflow->current_phase = WORKFLOW_MERGE;
            break;
        case WORKFLOW_MERGE:
            workflow->current_phase = WORKFLOW_COMPLETE;
            workflow->state = WORKFLOW_STATE_DONE;
            break;
        case WORKFLOW_COMPLETE:
            LOG_WARN("Workflow already complete");
            return E_INVALID_STATE;
    }

    workflow->phase_start_time = time(NULL);

    LOG_INFO("Workflow %s advanced: %s -> %s",
             workflow->workflow_id,
             workflow_phase_name(old_phase),
             workflow_phase_name(workflow->current_phase));

    return ARGO_SUCCESS;
}

/* Create a task */
ci_task_t* workflow_create_task(workflow_controller_t* workflow,
                               const char* description,
                               workflow_phase_t phase) {
    if (!workflow || !description) {
        argo_report_error(E_INPUT_NULL, "workflow_create_task", ERR_MSG_NULL_POINTER);
        return NULL;
    }

    ci_task_t* task = calloc(1, sizeof(ci_task_t));
    if (!task) {
        argo_report_error(E_SYSTEM_MEMORY, "workflow_create_task", ERR_MSG_TASK_ALLOC_FAILED);
        return NULL;
    }

    generate_task_id(task->id, sizeof(task->id));
    strncpy(task->description, description, sizeof(task->description) - 1);
    task->phase = phase;
    task->completed = 0;
    task->assigned_at = 0;
    task->completed_at = 0;
    task->assigned_to[0] = '\0';

    /* Add to task list */
    task->next = workflow->tasks;
    workflow->tasks = task;
    workflow->total_tasks++;

    LOG_INFO("Created task %s: %s (phase: %s)",
             task->id, description, workflow_phase_name(phase));

    return task;
}

/* Assign task to CI */
int workflow_assign_task(workflow_controller_t* workflow,
                        const char* task_id,
                        const char* ci_name) {
    if (!workflow || !task_id || !ci_name) {
        return E_INVALID_PARAMS;
    }

    /* Find task */
    ci_task_t* task = workflow_find_task(workflow, task_id);
    if (!task) {
        argo_report_error(E_NOT_FOUND, "workflow_assign_task", ERR_MSG_TASK_NOT_FOUND);
        return E_NOT_FOUND;
    }

    /* Verify CI exists and is ready */
    ci_registry_entry_t* ci = registry_find_ci(workflow->registry, ci_name);
    if (!ci) {
        argo_report_error(E_CI_NO_PROVIDER, "workflow_assign_task", ERR_MSG_CI_NOT_FOUND);
        return E_CI_NO_PROVIDER;
    }

    if (ci->status != CI_STATUS_READY) {
        LOG_WARN("CI %s is not ready (status: %d)", ci_name, ci->status);
        return E_CI_DISCONNECTED;
    }

    /* Assign task */
    strncpy(task->assigned_to, ci_name, sizeof(task->assigned_to) - 1);
    task->assigned_at = time(NULL);

    LOG_INFO("Assigned task %s to CI %s", task_id, ci_name);
    return ARGO_SUCCESS;
}

/* Complete a task */
int workflow_complete_task(workflow_controller_t* workflow,
                          const char* task_id) {
    if (!workflow || !task_id) {
        return E_INVALID_PARAMS;
    }

    ci_task_t* task = workflow_find_task(workflow, task_id);
    if (!task) {
        argo_report_error(E_NOT_FOUND, "workflow_complete_task", ERR_MSG_TASK_NOT_FOUND);
        return E_NOT_FOUND;
    }

    if (task->completed) {
        LOG_WARN("Task %s already completed", task_id);
        return ARGO_SUCCESS;
    }

    task->completed = 1;
    task->completed_at = time(NULL);
    workflow->completed_tasks++;

    LOG_INFO("Completed task %s (%d/%d tasks done)",
             task_id, workflow->completed_tasks, workflow->total_tasks);

    return ARGO_SUCCESS;
}

/* Find task by ID */
ci_task_t* workflow_find_task(workflow_controller_t* workflow,
                             const char* task_id) {
    if (!workflow || !task_id) return NULL;

    ci_task_t* task = workflow->tasks;
    while (task) {
        if (strcmp(task->id, task_id) == 0) {
            return task;
        }
        task = task->next;
    }

    return NULL;
}

/* Auto-assign tasks based on CI roles */
int workflow_auto_assign_tasks(workflow_controller_t* workflow) {
    if (!workflow) {
        return E_INVALID_PARAMS;
    }

    int assigned_count = 0;

    /* Iterate through tasks and assign based on phase */
    ci_task_t* task = workflow->tasks;
    while (task) {
        /* Skip already assigned tasks */
        if (task->assigned_to[0] != '\0') {
            task = task->next;
            continue;
        }

        /* Determine role based on phase */
        const char* target_role = NULL;
        switch (task->phase) {
            case WORKFLOW_PLAN:
                target_role = "requirements";
                break;
            case WORKFLOW_DEVELOP:
                target_role = "builder";
                break;
            case WORKFLOW_REVIEW:
                target_role = "analysis";
                break;
            case WORKFLOW_TEST:
                target_role = "builder";  /* Builders can run tests */
                break;
            case WORKFLOW_MERGE:
                target_role = "coordinator";
                break;
            default:
                task = task->next;
                continue;
        }

        /* Find available CI with matching role */
        ci_registry_entry_t* ci = registry_find_by_role(workflow->registry, target_role);
        if (ci && ci->status == CI_STATUS_READY) {
            int result = workflow_assign_task(workflow, task->id, ci->name);
            if (result == ARGO_SUCCESS) {
                assigned_count++;
            }
        }

        task = task->next;
    }

    LOG_INFO("Auto-assigned %d tasks", assigned_count);
    return ARGO_SUCCESS;
}

#if ARGO_HAS_DRYRUN
/* Enable/disable dry-run mode */
void workflow_set_dryrun(workflow_controller_t* workflow, int enable) {
    if (!workflow) return;
    workflow->dry_run = enable ? 1 : 0;
    LOG_INFO("Workflow %s: dry-run mode %s",
             workflow->workflow_id,
             workflow->dry_run ? "enabled" : "disabled");
}

/* Check if dry-run mode is enabled */
int workflow_is_dryrun(workflow_controller_t* workflow) {
    if (!workflow) return 0;
    return workflow->dry_run;
}
#endif
