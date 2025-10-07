/* © 2025 Casey Koons All rights reserved */

/* Workflow loader test - verify workflow definition loading and execution */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../include/argo_workflow_loader.h"
#include "../include/argo_orchestrator.h"
#include "../include/argo_error.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        printf("Testing: %s ... ", name); \
        tests_run++; \
    } while(0)

#define PASS() \
    do { \
        printf("✓\n"); \
        tests_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        printf("✗ (%s)\n", msg); \
        tests_failed++; \
    } while(0)

/* Test workflow definition creation and validation */
static void test_workflow_definition_validation(void) {
    TEST("Workflow definition validation");

    workflow_definition_t* def = calloc(1, sizeof(workflow_definition_t));
    if (!def) {
        FAIL("Failed to allocate definition");
        return;
    }

    /* Empty definition should fail */
    int result = workflow_validate_definition(def);
    if (result == ARGO_SUCCESS) {
        FAIL("Empty definition should not validate");
        free(def);
        return;
    }

    /* Add minimal required fields */
    strncpy(def->name, "test-workflow", sizeof(def->name) - 1);
    def->phase_count = 1;
    def->phases[0].phase = WORKFLOW_PLAN;
    strncpy(def->phases[0].name, "Planning", sizeof(def->phases[0].name) - 1);
    def->phases[0].task_count = 1;
    strncpy(def->phases[0].tasks[0].description, "Test task",
            sizeof(def->phases[0].tasks[0].description) - 1);

    def->personnel_count = 1;
    strncpy(def->personnel[0].role, "coordinator", sizeof(def->personnel[0].role) - 1);
    def->personnel[0].min_count = 1;
    def->personnel[0].max_count = 2;

    /* Should now validate */
    result = workflow_validate_definition(def);
    if (result != ARGO_SUCCESS) {
        FAIL("Valid definition should validate");
        free(def);
        return;
    }

    free(def);
    PASS();
}

/* Test workflow definition JSON serialization */
static void test_workflow_json_serialization(void) {
    TEST("Workflow JSON serialization");

    workflow_definition_t* def = calloc(1, sizeof(workflow_definition_t));
    if (!def) {
        FAIL("Failed to allocate definition");
        return;
    }

    /* Create a simple workflow */
    strncpy(def->name, "feature-development", sizeof(def->name) - 1);
    strncpy(def->description, "Standard feature development workflow",
            sizeof(def->description) - 1);
    strncpy(def->category, "development", sizeof(def->category) - 1);
    strncpy(def->event, "feature-request", sizeof(def->event) - 1);

    def->personnel_count = 2;
    strncpy(def->personnel[0].role, "requirements", sizeof(def->personnel[0].role) - 1);
    def->personnel[0].min_count = 1;
    def->personnel[0].max_count = 1;
    strncpy(def->personnel[1].role, "builder", sizeof(def->personnel[1].role) - 1);
    def->personnel[1].min_count = 1;
    def->personnel[1].max_count = 2;

    def->phase_count = 2;
    def->phases[0].phase = WORKFLOW_PLAN;
    strncpy(def->phases[0].name, "Planning", sizeof(def->phases[0].name) - 1);
    def->phases[0].task_count = 2;
    strncpy(def->phases[0].tasks[0].description, "Analyze requirements",
            sizeof(def->phases[0].tasks[0].description) - 1);
    strncpy(def->phases[0].tasks[1].description, "Design solution",
            sizeof(def->phases[0].tasks[1].description) - 1);

    def->phases[1].phase = WORKFLOW_DEVELOP;
    strncpy(def->phases[1].name, "Development", sizeof(def->phases[1].name) - 1);
    def->phases[1].task_count = 1;
    strncpy(def->phases[1].tasks[0].description, "Implement feature",
            sizeof(def->phases[1].tasks[0].description) - 1);

    /* Convert to JSON */
    char* json = workflow_definition_to_json(def);
    if (!json) {
        FAIL("Failed to serialize to JSON");
        free(def);
        return;
    }

    /* Verify JSON contains expected fields */
    if (strstr(json, "\"name\": \"feature-development\"") == NULL) {
        FAIL("JSON missing name");
        free(json);
        free(def);
        return;
    }

    if (strstr(json, "\"personnel\"") == NULL) {
        FAIL("JSON missing personnel");
        free(json);
        free(def);
        return;
    }

    if (strstr(json, "\"phases\"") == NULL) {
        FAIL("JSON missing phases");
        free(json);
        free(def);
        return;
    }

    free(json);
    free(def);
    PASS();
}

/* Test workflow path building */
static void test_workflow_path_building(void) {
    TEST("Workflow path building");

    char path[WORKFLOW_MAX_PATH];
    int result = workflow_build_path(path, sizeof(path),
                                    "development", "feature-request", "standard");

    if (result != ARGO_SUCCESS) {
        FAIL("Failed to build path");
        return;
    }

    /* Verify path format */
    if (strstr(path, "development") == NULL ||
        strstr(path, "feature-request") == NULL ||
        strstr(path, "standard") == NULL) {
        FAIL("Path missing components");
        return;
    }

    if (strstr(path, ".json") == NULL) {
        FAIL("Path missing .json extension");
        return;
    }

    PASS();
}

/* Test workflow file save/load roundtrip */
static void test_workflow_file_persistence(void) {
    TEST("Workflow file persistence");

    const char* test_file = "/tmp/argo_test_workflow.json";

    /* Create a workflow definition */
    workflow_definition_t* def = calloc(1, sizeof(workflow_definition_t));
    if (!def) {
        FAIL("Failed to allocate definition");
        return;
    }

    strncpy(def->name, "test-workflow", sizeof(def->name) - 1);
    strncpy(def->description, "Test workflow description",
            sizeof(def->description) - 1);
    def->phase_count = 1;
    def->phases[0].phase = WORKFLOW_PLAN;
    strncpy(def->phases[0].name, "Planning", sizeof(def->phases[0].name) - 1);
    def->phases[0].task_count = 1;
    strncpy(def->phases[0].tasks[0].description, "Plan the work",
            sizeof(def->phases[0].tasks[0].description) - 1);
    def->personnel_count = 1;
    strncpy(def->personnel[0].role, "coordinator", sizeof(def->personnel[0].role) - 1);
    def->personnel[0].min_count = 1;
    def->personnel[0].max_count = 1;

    /* Save to file */
    char* json = workflow_definition_to_json(def);
    if (!json) {
        FAIL("Failed to convert to JSON");
        free(def);
        return;
    }

    FILE* fp = fopen(test_file, "w");
    if (!fp) {
        FAIL("Failed to open file for writing");
        free(json);
        free(def);
        return;
    }

    fputs(json, fp);
    fclose(fp);
    free(json);

    /* Load from file */
    workflow_definition_t* loaded = workflow_load_from_file(test_file);
    if (!loaded) {
        FAIL("Failed to load from file");
        free(def);
        unlink(test_file);
        return;
    }

    /* Verify loaded definition */
    if (strcmp(loaded->name, "test-workflow") != 0) {
        FAIL("Loaded name mismatch");
        workflow_definition_free(def);
        workflow_definition_free(loaded);
        unlink(test_file);
        return;
    }

    /* Cleanup */
    workflow_definition_free(def);
    workflow_definition_free(loaded);
    unlink(test_file);

    PASS();
}

/* Test workflow execution from definition */
static void test_workflow_execution(void) {
    TEST("Workflow execution from definition");

    /* Create orchestrator */
    argo_orchestrator_t* orch = orchestrator_create("exec-test", "main");
    if (!orch) {
        FAIL("Failed to create orchestrator");
        return;
    }

    /* Add a CI */
    orchestrator_add_ci(orch, "TestCI", "coordinator", "claude");
    orchestrator_start_ci(orch, "TestCI");

    /* Create workflow definition */
    workflow_definition_t* def = calloc(1, sizeof(workflow_definition_t));
    if (!def) {
        FAIL("Failed to allocate definition");
        orchestrator_destroy(orch);
        return;
    }

    strncpy(def->name, "exec-test", sizeof(def->name) - 1);
    def->phase_count = 1;
    def->phases[0].phase = WORKFLOW_PLAN;
    strncpy(def->phases[0].name, "Planning", sizeof(def->phases[0].name) - 1);
    def->phases[0].task_count = 2;
    strncpy(def->phases[0].tasks[0].description, "Task 1",
            sizeof(def->phases[0].tasks[0].description) - 1);
    strncpy(def->phases[0].tasks[1].description, "Task 2",
            sizeof(def->phases[0].tasks[1].description) - 1);
    def->personnel_count = 1;
    strncpy(def->personnel[0].role, "coordinator", sizeof(def->personnel[0].role) - 1);
    def->personnel[0].min_count = 1;
    def->personnel[0].max_count = 1;

    /* Execute workflow */
    int result = workflow_execute_definition(orch, def, "test-session");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to execute workflow");
        workflow_definition_free(def);
        orchestrator_destroy(orch);
        return;
    }

    /* Verify workflow is running */
    if (!orch->running) {
        FAIL("Workflow not running");
        workflow_definition_free(def);
        orchestrator_destroy(orch);
        return;
    }

    /* Verify tasks were created */
    if (orch->workflow->total_tasks != 2) {
        FAIL("Task count mismatch");
        workflow_definition_free(def);
        orchestrator_destroy(orch);
        return;
    }

    workflow_definition_free(def);
    orchestrator_destroy(orch);
    PASS();
}

int main(void) {
    printf("\n========================================\n");
    printf("ARGO WORKFLOW LOADER TESTS\n");
    printf("========================================\n\n");

    test_workflow_definition_validation();
    test_workflow_json_serialization();
    test_workflow_path_building();
    test_workflow_file_persistence();
    test_workflow_execution();

    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
