/* © 2025 Casey Koons All rights reserved */

/* Provider integration test */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/argo_provider.h"
#include "../include/argo_registry.h"
#include "../include/argo_ollama.h"
#include "../include/argo_claude.h"
#include "../include/argo_api_providers.h"
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
        printf("✗ %s\n", msg); \
        tests_failed++; \
    } while(0)

/* Test provider registry creation */
static void test_registry_creation(void) {
    TEST("Provider registry creation");

    provider_registry_t* registry = provider_registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    if (registry->count != 0) {
        FAIL("Initial count should be 0");
        provider_registry_destroy(registry);
        return;
    }

    provider_registry_destroy(registry);
    PASS();
}

/* Test provider registration */
static void test_provider_registration(void) {
    TEST("Provider registration");

    provider_registry_t* registry = provider_registry_create();

    /* Create Claude Code provider */
    ci_provider_t* provider = claude_code_create_provider("test");
    if (!provider) {
        FAIL("Failed to create Claude Code provider");
        provider_registry_destroy(registry);
        return;
    }

    /* Register it */
    int result = provider_registry_add(registry, provider, PROVIDER_TYPE_CLI, false);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to register provider");
        provider_registry_destroy(registry);
        return;
    }

    if (registry->count != 1) {
        FAIL("Count should be 1");
        provider_registry_destroy(registry);
        return;
    }

    provider_registry_destroy(registry);
    PASS();
}

/* Test provider discovery */
static void test_provider_discovery(void) {
    TEST("Provider discovery");

    provider_registry_t* registry = provider_registry_create();

    /* Add Claude Code provider */
    ci_provider_t* claude = claude_code_create_provider("test");
    provider_registry_add(registry, claude, PROVIDER_TYPE_CLI, false);

    /* Run discovery */
    int result = provider_registry_discover_all(registry);
    if (result != ARGO_SUCCESS) {
        FAIL("Discovery failed");
        provider_registry_destroy(registry);
        return;
    }

    /* Claude Code should be available */
    if (registry->available_count < 1) {
        FAIL("No providers available");
        provider_registry_destroy(registry);
        return;
    }

    provider_registry_destroy(registry);
    PASS();
}

/* Test finding providers */
static void test_find_provider(void) {
    TEST("Finding providers");

    provider_registry_t* registry = provider_registry_create();

    ci_provider_t* claude = claude_code_create_provider("test");
    provider_registry_add(registry, claude, PROVIDER_TYPE_CLI, false);

    /* Find by name */
    provider_entry_t* entry = provider_registry_find(registry, "claude_code");
    if (!entry) {
        FAIL("Failed to find claude_code provider");
        provider_registry_destroy(registry);
        return;
    }

    if (strcmp(entry->provider->name, "claude_code") != 0) {
        FAIL("Provider name mismatch");
        provider_registry_destroy(registry);
        return;
    }

    provider_registry_destroy(registry);
    PASS();
}

/* Test default provider */
static void test_default_provider(void) {
    TEST("Default provider");

    provider_registry_t* registry = provider_registry_create();

    ci_provider_t* claude = claude_code_create_provider("test");
    provider_registry_add(registry, claude, PROVIDER_TYPE_CLI, false);
    provider_registry_discover_all(registry);

    /* Set default */
    int result = provider_registry_set_default(registry, "claude_code");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to set default");
        provider_registry_destroy(registry);
        return;
    }

    ci_provider_t* default_provider = provider_registry_get_default(registry);
    if (!default_provider) {
        FAIL("Failed to get default provider");
        provider_registry_destroy(registry);
        return;
    }

    if (strcmp(default_provider->name, "claude_code") != 0) {
        FAIL("Default provider mismatch");
        provider_registry_destroy(registry);
        return;
    }

    provider_registry_destroy(registry);
    PASS();
}

/* Test CI assignment */
static void test_ci_assignment(void) {
    TEST("CI assignment");

    provider_registry_t* provider_reg = provider_registry_create();
    ci_registry_t* ci_reg = registry_create();

    /* Create CI */
    registry_add_ci(ci_reg, "test-ci", "builder", "default-model", 9000);

    /* Create and register provider with NULL model (uses default) */
    ci_provider_t* claude = claude_code_create_provider(NULL);
    provider_registry_add(provider_reg, claude, PROVIDER_TYPE_CLI, false);
    provider_registry_discover_all(provider_reg);

    /* Assign provider to CI */
    int result = provider_assign_ci(provider_reg, ci_reg, "test-ci", "claude_code");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to assign provider to CI");
        provider_registry_destroy(provider_reg);
        registry_destroy(ci_reg);
        return;
    }

    /* Verify assignment - should get default model "claude-sonnet-4" */
    ci_registry_entry_t* ci = registry_find_ci(ci_reg, "test-ci");
    if (strcmp(ci->model, "claude-sonnet-4") != 0) {
        FAIL("Model not updated");
        provider_registry_destroy(provider_reg);
        registry_destroy(ci_reg);
        return;
    }

    provider_registry_destroy(provider_reg);
    registry_destroy(ci_reg);
    PASS();
}

/* Test provider activation */
static void test_provider_activation(void) {
    TEST("Provider activation");

    provider_registry_t* registry = provider_registry_create();

    /* Create a provider that requires activation */
    ci_provider_t* openai = openai_api_create_provider(NULL);
    if (!openai) {
        FAIL("Failed to create OpenAI provider");
        provider_registry_destroy(registry);
        return;
    }

    provider_registry_add(registry, openai, PROVIDER_TYPE_API, true);

    /* Should not be activated initially */
    if (provider_registry_is_activated(registry, "openai-api")) {
        FAIL("Provider should not be activated initially");
        provider_registry_destroy(registry);
        return;
    }

    /* Activate it */
    int result = provider_registry_activate(registry, "openai-api");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to activate provider");
        provider_registry_destroy(registry);
        return;
    }

    /* Should now be activated */
    if (!provider_registry_is_activated(registry, "openai-api")) {
        FAIL("Provider should be activated");
        provider_registry_destroy(registry);
        return;
    }

    provider_registry_destroy(registry);
    PASS();
}

/* Test message creation */
static void test_message_creation(void) {
    TEST("Message creation");

    provider_message_t* msg = provider_message_create(
        MSG_TYPE_TASK_REQUEST,
        "builder-1",
        "Build the authentication module"
    );

    if (!msg) {
        FAIL("Failed to create message");
        return;
    }

    if (!msg->type || strcmp(msg->type, MSG_TYPE_TASK_REQUEST) != 0) {
        FAIL("Message type mismatch");
        provider_message_destroy(msg);
        return;
    }

    if (!msg->ci_name || strcmp(msg->ci_name, "builder-1") != 0) {
        FAIL("CI name mismatch");
        provider_message_destroy(msg);
        return;
    }

    provider_message_destroy(msg);
    PASS();
}

/* Test message JSON conversion */
static void test_message_json(void) {
    TEST("Message JSON conversion");

    provider_message_t* msg = provider_message_create(
        MSG_TYPE_TASK_REQUEST,
        "builder-1",
        "Test content"
    );

    /* Convert to JSON */
    char* json = provider_message_to_json(msg);
    if (!json) {
        FAIL("Failed to convert to JSON");
        provider_message_destroy(msg);
        return;
    }

    /* Check JSON contains expected fields */
    if (!strstr(json, "\"type\":\"task_request\"")) {
        FAIL("JSON missing type field");
        free(json);
        provider_message_destroy(msg);
        return;
    }

    if (!strstr(json, "\"ci_name\":\"builder-1\"")) {
        FAIL("JSON missing ci_name field");
        free(json);
        provider_message_destroy(msg);
        return;
    }

    /* Parse back from JSON */
    provider_message_t* parsed = provider_message_from_json(json);
    if (!parsed) {
        FAIL("Failed to parse JSON");
        free(json);
        provider_message_destroy(msg);
        return;
    }

    if (!parsed->type || strcmp(parsed->type, MSG_TYPE_TASK_REQUEST) != 0) {
        FAIL("Parsed message type mismatch");
        free(json);
        provider_message_destroy(msg);
        provider_message_destroy(parsed);
        return;
    }

    free(json);
    provider_message_destroy(msg);
    provider_message_destroy(parsed);
    PASS();
}

int main(void) {
    printf("\n========================================\n");
    printf("ARGO PROVIDER TESTS\n");
    printf("========================================\n\n");

    test_registry_creation();
    test_provider_registration();
    test_provider_discovery();
    test_find_provider();
    test_default_provider();
    test_ci_assignment();
    test_provider_activation();
    test_message_creation();
    test_message_json();

    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
