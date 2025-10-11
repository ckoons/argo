/* © 2025 Casey Koons All rights reserved */
/* Workflow scripting functionality test suite */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* External library - header only for struct definitions */
#define JSMN_HEADER
#include "jsmn.h"

#include "argo_workflow.h"
#include "argo_workflow_context.h"
#include "argo_workflow_loader.h"
#include "argo_workflow_json.h"
#include "argo_error.h"
#include "argo_log.h"

/* Test counter */
static int tests_passed = 0;
static int tests_failed = 0;

/* Test helper macros */
#define TEST_START(name) \
    printf("\n=== TEST: %s ===\n", name); \
    fflush(stdout);

#define TEST_ASSERT(condition, message) \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        tests_failed++; \
        return -1; \
    } else { \
        printf("PASS: %s\n", message); \
        tests_passed++; \
    }

#define TEST_END() \
    printf("=== TEST COMPLETE ===\n\n"); \
    fflush(stdout);

/* Test 1: Variable substitution with {{variable}} syntax */
static int test_variable_substitution(void) {
    TEST_START("Variable Substitution");

    workflow_context_t* ctx = workflow_context_create();
    TEST_ASSERT(ctx != NULL, "Context created");

    /* Set test variables */
    workflow_context_set(ctx, "name", "Casey");
    workflow_context_set(ctx, "number", "12345");

    /* Test single variable */
    char output1[256];
    int result = workflow_context_substitute(ctx, "Hello {{name}}", output1, sizeof(output1));
    TEST_ASSERT(result == ARGO_SUCCESS, "Substitution succeeded");
    TEST_ASSERT(strcmp(output1, "Hello Casey") == 0, "Single variable substituted correctly");
    printf("  Result: '%s'\n", output1);

    /* Test multiple variables */
    char output2[256];
    result = workflow_context_substitute(ctx, "{{name}}: {{number}}", output2, sizeof(output2));
    TEST_ASSERT(result == ARGO_SUCCESS, "Multiple substitution succeeded");
    TEST_ASSERT(strcmp(output2, "Casey: 12345") == 0, "Multiple variables substituted");
    printf("  Result: '%s'\n", output2);

    /* Test missing variable (should keep placeholder) */
    char output3[256];
    result = workflow_context_substitute(ctx, "{{missing}}", output3, sizeof(output3));
    TEST_ASSERT(result == ARGO_SUCCESS, "Missing variable handled");
    TEST_ASSERT(strcmp(output3, "{{missing}}") == 0, "Placeholder preserved for missing variable");
    printf("  Result: '%s'\n", output3);

    /* Test no variables */
    char output4[256];
    result = workflow_context_substitute(ctx, "No variables here", output4, sizeof(output4));
    TEST_ASSERT(result == ARGO_SUCCESS, "No variables handled");
    TEST_ASSERT(strcmp(output4, "No variables here") == 0, "Text without variables unchanged");
    printf("  Result: '%s'\n", output4);

    workflow_context_destroy(ctx);
    TEST_END();
    return 0;
}

/* Test 2: Context variable set/get */
static int test_context_operations(void) {
    TEST_START("Context Operations");

    workflow_context_t* ctx = workflow_context_create();
    TEST_ASSERT(ctx != NULL, "Context created");

    /* Set variables */
    int result = workflow_context_set(ctx, "var1", "value1");
    TEST_ASSERT(result == ARGO_SUCCESS, "Set var1");

    result = workflow_context_set(ctx, "var2", "value2");
    TEST_ASSERT(result == ARGO_SUCCESS, "Set var2");

    /* Get variables */
    const char* val1 = workflow_context_get(ctx, "var1");
    TEST_ASSERT(val1 != NULL, "Get var1 returned non-NULL");
    TEST_ASSERT(strcmp(val1, "value1") == 0, "Got correct value for var1");
    printf("  var1 = '%s'\n", val1);

    const char* val2 = workflow_context_get(ctx, "var2");
    TEST_ASSERT(val2 != NULL, "Get var2 returned non-NULL");
    TEST_ASSERT(strcmp(val2, "value2") == 0, "Got correct value for var2");
    printf("  var2 = '%s'\n", val2);

    /* Update existing variable */
    result = workflow_context_set(ctx, "var1", "updated");
    TEST_ASSERT(result == ARGO_SUCCESS, "Update var1");
    val1 = workflow_context_get(ctx, "var1");
    TEST_ASSERT(strcmp(val1, "updated") == 0, "Variable updated correctly");
    printf("  var1 = '%s' (updated)\n", val1);

    /* Check count */
    TEST_ASSERT(ctx->count == 2, "Context has correct count");
    printf("  Context count: %d\n", ctx->count);

    workflow_context_destroy(ctx);
    TEST_END();
    return 0;
}

/* Test 3: JSON workflow parsing */
static int test_json_parsing(void) {
    TEST_START("JSON Workflow Parsing");

    const char* test_json =
        "{"
        "  \"workflow_name\": \"test\","
        "  \"phases\": ["
        "    {"
        "      \"phase\": \"TEST\","
        "      \"steps\": ["
        "        {"
        "          \"step\": 1,"
        "          \"type\": \"display\","
        "          \"message\": \"Hello\","
        "          \"next_step\": 2"
        "        },"
        "        {"
        "          \"step\": 2,"
        "          \"type\": \"user_ask\","
        "          \"prompt\": \"Name:\","
        "          \"save_to\": \"name\","
        "          \"next_step\": \"EXIT\""
        "        }"
        "      ]"
        "    }"
        "  ]"
        "}";

    /* Parse JSON */
    jsmntok_t* tokens = malloc(sizeof(jsmntok_t) * 100);
    TEST_ASSERT(tokens != NULL, "Tokens allocated");

    int token_count = workflow_json_parse(test_json, tokens, 100);
    TEST_ASSERT(token_count > 0, "JSON parsed successfully");
    printf("  Token count: %d\n", token_count);

    /* Find workflow_name */
    int name_idx = workflow_json_find_field(test_json, tokens, 0, "workflow_name");
    TEST_ASSERT(name_idx >= 0, "Found workflow_name field");

    char name[64];
    int result = workflow_json_extract_string(test_json, &tokens[name_idx], name, sizeof(name));
    TEST_ASSERT(result == ARGO_SUCCESS, "Extracted workflow_name");
    TEST_ASSERT(strcmp(name, "test") == 0, "Workflow name is correct");
    printf("  Workflow name: '%s'\n", name);

    free(tokens);
    TEST_END();
    return 0;
}

/* Test 4: Step field extraction */
static int test_step_fields(void) {
    TEST_START("Step Field Extraction");

    const char* test_json =
        "{"
        "  \"steps\": ["
        "    {"
        "      \"step\": 1,"
        "      \"type\": \"user_ask\","
        "      \"prompt\": \"Enter name:\","
        "      \"save_to\": \"user_name\","
        "      \"next_step\": 2"
        "    }"
        "  ]"
        "}";

    jsmntok_t* tokens = malloc(sizeof(jsmntok_t) * 100);
    int token_count = workflow_json_parse(test_json, tokens, 100);
    TEST_ASSERT(token_count > 0, "JSON parsed");

    /* Find steps array */
    int steps_idx = workflow_json_find_field(test_json, tokens, 0, "steps");
    TEST_ASSERT(steps_idx >= 0, "Found steps array");

    /* Get first step (should be at steps_idx + 1) */
    int step_idx = steps_idx + 1;
    TEST_ASSERT(tokens[step_idx].type == JSMN_OBJECT, "First step is object");

    /* Extract type */
    int type_idx = workflow_json_find_field(test_json, tokens, step_idx, "type");
    TEST_ASSERT(type_idx >= 0, "Found type field");

    char type[64];
    workflow_json_extract_string(test_json, &tokens[type_idx], type, sizeof(type));
    TEST_ASSERT(strcmp(type, "user_ask") == 0, "Type is user_ask");
    printf("  Type: '%s'\n", type);

    /* Extract prompt */
    int prompt_idx = workflow_json_find_field(test_json, tokens, step_idx, "prompt");
    TEST_ASSERT(prompt_idx >= 0, "Found prompt field");

    char prompt[64];
    workflow_json_extract_string(test_json, &tokens[prompt_idx], prompt, sizeof(prompt));
    TEST_ASSERT(strcmp(prompt, "Enter name:") == 0, "Prompt is correct");
    printf("  Prompt: '%s'\n", prompt);

    /* Extract save_to */
    int save_to_idx = workflow_json_find_field(test_json, tokens, step_idx, "save_to");
    TEST_ASSERT(save_to_idx >= 0, "Found save_to field");

    char save_to[64];
    workflow_json_extract_string(test_json, &tokens[save_to_idx], save_to, sizeof(save_to));
    TEST_ASSERT(strcmp(save_to, "user_name") == 0, "save_to is correct");
    printf("  save_to: '%s'\n", save_to);

    free(tokens);
    TEST_END();
    return 0;
}

/* Test 5: Variable substitution in workflow steps */
static int test_workflow_variable_flow(void) {
    TEST_START("Workflow Variable Flow");

    workflow_context_t* ctx = workflow_context_create();
    TEST_ASSERT(ctx != NULL, "Context created");

    /* Simulate user_ask step saving variable */
    workflow_context_set(ctx, "user_input", "Hello World");
    printf("  Set user_input = 'Hello World'\n");

    /* Simulate ci_analyze step substituting variable in task */
    const char* task_template = "{{user_input}}";
    char task[256];
    int result = workflow_context_substitute(ctx, task_template, task, sizeof(task));
    TEST_ASSERT(result == ARGO_SUCCESS, "Task substitution succeeded");
    TEST_ASSERT(strcmp(task, "Hello World") == 0, "Task has user input");
    printf("  Task after substitution: '%s'\n", task);

    /* Simulate CI response being saved */
    workflow_context_set(ctx, "ci_response", "I received: Hello World");
    printf("  Set ci_response = 'I received: Hello World'\n");

    /* Simulate display step substituting response */
    const char* display_template = "Claude: {{ci_response}}";
    char display[256];
    result = workflow_context_substitute(ctx, display_template, display, sizeof(display));
    TEST_ASSERT(result == ARGO_SUCCESS, "Display substitution succeeded");
    TEST_ASSERT(strstr(display, "I received: Hello World") != NULL, "Display has CI response");
    printf("  Display after substitution: '%s'\n", display);

    workflow_context_destroy(ctx);
    TEST_END();
    return 0;
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("=========================================\n");
    printf("Workflow Scripting Test Suite\n");
    printf("=========================================\n");

    /* Initialize logging */
    log_init(NULL);
    log_set_level(LOG_ERROR);  /* Only errors during tests */

    /* Run all tests */
    test_variable_substitution();
    test_context_operations();
    test_json_parsing();
    test_step_fields();
    test_workflow_variable_flow();

    /* Print summary */
    printf("\n");
    printf("=========================================\n");
    printf("Test Summary\n");
    printf("=========================================\n");
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);
    printf("=========================================\n");

    if (tests_failed > 0) {
        printf("RESULT: FAILED\n");
        return 1;
    } else {
        printf("RESULT: SUCCESS\n");
        return 0;
    }
}
