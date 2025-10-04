/* © 2025 Casey Koons All rights reserved */

/* Claude provider test suite */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/argo_ci.h"
#include "../include/argo_api_providers.h"
#include "../include/argo_claude.h"
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

/* Test Claude Code provider creation */
static void test_claude_code_provider_creation(void) {
    TEST("Claude Code provider creation");

    ci_provider_t* provider = claude_code_create_provider("test");
    if (!provider) {
        FAIL("Failed to create provider");
        return;
    }

    if (strstr(provider->name, "claude") == NULL) {
        FAIL("Provider name incorrect");
        if (provider->cleanup) provider->cleanup(provider);
        return;
    }

    if (provider->cleanup) {
        provider->cleanup(provider);
    }
    PASS();
}

/* Test Claude CLI provider creation */
static void test_claude_cli_provider_creation(void) {
    TEST("Claude CLI provider creation");

    ci_provider_t* provider = claude_create_provider("test");
    if (!provider) {
        /* This is OK - claude CLI may not be available */
        PASS();
        return;
    }

    if (strstr(provider->name, "claude") == NULL) {
        FAIL("Provider name incorrect");
        if (provider->cleanup) provider->cleanup(provider);
        return;
    }

    if (provider->cleanup) {
        provider->cleanup(provider);
    }
    PASS();
}

/* Test Claude API provider creation */
static void test_claude_api_provider_creation(void) {
    TEST("Claude API provider creation");

    /* Check availability first */
    if (!claude_api_is_available()) {
        printf("⊘ (API key not set) ");
        PASS();
        return;
    }

    ci_provider_t* provider = claude_api_create_provider(NULL);
    if (!provider) {
        FAIL("Failed to create provider");
        return;
    }

    if (strstr(provider->name, "claude") == NULL) {
        FAIL("Provider name incorrect");
        if (provider->cleanup) provider->cleanup(provider);
        return;
    }

    /* Verify provider fields */
    if (!provider->query) {
        FAIL("Query function not set");
        if (provider->cleanup) provider->cleanup(provider);
        return;
    }

    if (strlen(provider->model) == 0) {
        FAIL("Model not set");
        if (provider->cleanup) provider->cleanup(provider);
        return;
    }

    if (provider->cleanup) {
        provider->cleanup(provider);
    }
    PASS();
}

/* Test provider with custom model */
static void test_claude_custom_model(void) {
    TEST("Claude provider with custom model");

    if (!claude_api_is_available()) {
        printf("⊘ (API key not set) ");
        PASS();
        return;
    }

    const char* custom_model = "claude-3-opus-20240229";
    ci_provider_t* provider = claude_api_create_provider(custom_model);
    if (!provider) {
        FAIL("Failed to create provider");
        return;
    }

    if (strcmp(provider->model, custom_model) != 0) {
        FAIL("Custom model not set correctly");
        if (provider->cleanup) provider->cleanup(provider);
        return;
    }

    if (provider->cleanup) {
        provider->cleanup(provider);
    }
    PASS();
}

/* Test provider cleanup */
static void test_claude_provider_cleanup(void) {
    TEST("Claude provider cleanup");

    ci_provider_t* provider = claude_code_create_provider("test");
    if (!provider) {
        FAIL("Failed to create provider");
        return;
    }

    if (!provider->cleanup) {
        FAIL("Cleanup function not set");
        return;
    }

    /* Should not crash */
    provider->cleanup(provider);

    PASS();
}

/* Test null parameter handling */
static void test_claude_null_parameters(void) {
    TEST("Claude provider null parameter handling");

    /* Creating with NULL CI name should still work (uses default) */
    ci_provider_t* provider = claude_code_create_provider(NULL);
    if (!provider) {
        FAIL("Should handle NULL CI name");
        return;
    }

    if (provider->cleanup) {
        provider->cleanup(provider);
    }

    PASS();
}

/* Test provider capabilities */
static void test_claude_provider_capabilities(void) {
    TEST("Claude provider capabilities");

    ci_provider_t* provider = claude_code_create_provider("test");
    if (!provider) {
        FAIL("Failed to create provider");
        return;
    }

    /* Claude Code should support streaming */
    if (!provider->supports_streaming) {
        FAIL("Should support streaming");
        if (provider->cleanup) provider->cleanup(provider);
        return;
    }

    /* Claude Code should support memory */
    if (!provider->supports_memory) {
        FAIL("Should support memory");
        if (provider->cleanup) provider->cleanup(provider);
        return;
    }

    if (provider->cleanup) {
        provider->cleanup(provider);
    }
    PASS();
}

/* Test API provider capabilities */
static void test_claude_api_capabilities(void) {
    TEST("Claude API provider capabilities");

    if (!claude_api_is_available()) {
        printf("⊘ (API key not set) ");
        PASS();
        return;
    }

    ci_provider_t* provider = claude_api_create_provider(NULL);
    if (!provider) {
        FAIL("Failed to create provider");
        return;
    }

    /* API should support streaming */
    if (!provider->supports_streaming) {
        FAIL("API should support streaming");
        if (provider->cleanup) provider->cleanup(provider);
        return;
    }

    /* Verify context window */
    if (provider->max_context == 0) {
        FAIL("Max context not set");
        if (provider->cleanup) provider->cleanup(provider);
        return;
    }

    if (provider->cleanup) {
        provider->cleanup(provider);
    }
    PASS();
}

/* Test multiple provider instances */
static void test_claude_multiple_instances(void) {
    TEST("Multiple Claude provider instances");

    ci_provider_t* provider1 = claude_code_create_provider("test1");
    ci_provider_t* provider2 = claude_code_create_provider("test2");

    if (!provider1 || !provider2) {
        FAIL("Failed to create multiple providers");
        if (provider1 && provider1->cleanup) provider1->cleanup(provider1);
        if (provider2 && provider2->cleanup) provider2->cleanup(provider2);
        return;
    }

    /* Providers should be independent */
    if (provider1 == provider2) {
        FAIL("Providers should be different instances");
        if (provider1->cleanup) provider1->cleanup(provider1);
        if (provider2->cleanup) provider2->cleanup(provider2);
        return;
    }

    if (provider1->cleanup) provider1->cleanup(provider1);
    if (provider2->cleanup) provider2->cleanup(provider2);
    PASS();
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("Claude Provider Test Suite\n");
    printf("==========================================\n\n");

    /* Run tests */
    test_claude_code_provider_creation();
    test_claude_cli_provider_creation();
    test_claude_api_provider_creation();
    test_claude_custom_model();
    test_claude_provider_cleanup();
    test_claude_null_parameters();
    test_claude_provider_capabilities();
    test_claude_api_capabilities();
    test_claude_multiple_instances();

    /* Print summary */
    printf("\n");
    printf("==========================================\n");
    printf("Test Results\n");
    printf("==========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("==========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
