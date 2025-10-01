/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_LOADER_H
#define ARGO_WORKFLOW_LOADER_H

#include "argo_workflow.h"

/* Workflow definition paths */
#define WORKFLOW_BASE_DIR "argo/workflows"
#define WORKFLOW_MAX_PATH 512

/* Workflow artifact locations define lifecycle */
#define ARTIFACT_TEMP "/tmp"              /* Ephemeral - cleaned on session end */
#define ARTIFACT_WORKFLOW "argo/workflow" /* Persistent - tracked */
#define ARTIFACT_USER "file"              /* User-specified path */

/* Workflow definition from JSON */
typedef struct workflow_definition {
    char name[128];
    char description[256];
    char category[64];
    char event[64];

    /* Personnel requirements */
    struct {
        char role[32];
        char provider[32];
        int min_count;
        int max_count;
    } personnel[16];
    int personnel_count;

    /* Phase definitions */
    struct {
        workflow_phase_t phase;
        char name[64];
        char description[256];

        /* Tasks for this phase */
        struct {
            char description[256];
            char required_role[32];
            bool parallel_allowed;
        } tasks[32];
        int task_count;

        /* Checkpoint after this phase? */
        bool checkpoint;
    } phases[16];
    int phase_count;

    /* Artifact specifications */
    struct {
        char name[128];
        char location[256];     /* Where to create */
        char lifecycle[32];     /* temp, workflow, user */
        bool required;          /* Must exist for success */
    } artifacts[32];
    int artifact_count;

    /* Success criteria */
    struct {
        bool all_tasks_complete;
        bool all_tests_pass;
        bool no_conflicts;
        char custom_check[256];  /* Optional command to run */
    } success_criteria;

} workflow_definition_t;

/* Workflow loader functions */
workflow_definition_t* workflow_load_from_file(const char* path);
workflow_definition_t* workflow_load_by_name(const char* category,
                                             const char* event,
                                             const char* name);
void workflow_definition_free(workflow_definition_t* def);

/* Workflow execution from definition */
int workflow_execute_definition(argo_orchestrator_t* orch,
                               workflow_definition_t* def,
                               const char* session_id);

/* Validation */
int workflow_validate_definition(workflow_definition_t* def);

/* JSON conversion */
char* workflow_definition_to_json(workflow_definition_t* def);
workflow_definition_t* workflow_definition_from_json(const char* json);

/* Path helpers */
int workflow_build_path(char* buffer, size_t size,
                       const char* category,
                       const char* event,
                       const char* name);

#endif /* ARGO_WORKFLOW_LOADER_H */
