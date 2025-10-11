/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "argo_mock.h"
#include "argo_ci.h"
#include "argo_error.h"

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test helper macros */
#define TEST_START(name) \
    printf("Testing: %-50s ", name); \
    tests_run++;

#define TEST_PASS() \
    printf("✓\n"); \
    tests_passed++;

#define TEST_FAIL(msg) \
    printf("✗ %s\n", msg); \
    tests_failed++;

/* Callback for testing */
static char* last_response = NULL;

static void test_callback(const ci_response_t* response, void* userdata) {
    (void)userdata;
    if (response && response->content) {
        free(last_response);
        last_response = strdup(response->content);
    }
}

/* Test 1: Create and destroy mock provider */
static void test_create_destroy(void) {
    TEST_START("Create and destroy mock provider");

    ci_provider_t* provider = mock_provider_create("test-model");
    if (!provider) {
        TEST_FAIL("Failed to create provider");
        return;
    }

    if (strcmp(provider->name, "mock") != 0) {
        TEST_FAIL("Wrong provider name");
        provider->cleanup(provider);
        return;
    }

    provider->cleanup(provider);
    TEST_PASS();
}

/* Test 2: Query with default response */
static void test_default_response(void) {
    TEST_START("Query with default response");

    ci_provider_t* provider = mock_provider_create("test-model");
    if (!provider) {
        TEST_FAIL("Failed to create provider");
        return;
    }

    int result = provider->init(provider);
    if (result != ARGO_SUCCESS) {
        TEST_FAIL("Init failed");
        provider->cleanup(provider);
        return;
    }

    result = provider->connect(provider);
    if (result != ARGO_SUCCESS) {
        TEST_FAIL("Connect failed");
        provider->cleanup(provider);
        return;
    }

    free(last_response);
    last_response = NULL;

    result = provider->query(provider, "Test prompt", test_callback, NULL);
    if (result != ARGO_SUCCESS) {
        TEST_FAIL("Query failed");
        provider->cleanup(provider);
        return;
    }

    if (!last_response || strcmp(last_response, "Mock CI response") != 0) {
        TEST_FAIL("Wrong default response");
        provider->cleanup(provider);
        return;
    }

    provider->cleanup(provider);
    TEST_PASS();
}

/* Test 3: Custom single response */
static void test_custom_response(void) {
    TEST_START("Set and query custom response");

    ci_provider_t* provider = mock_provider_create("test-model");
    if (!provider) {
        TEST_FAIL("Failed to create provider");
        return;
    }

    const char* custom = "Custom test response";
    int result = mock_provider_set_response(provider, custom);
    if (result != ARGO_SUCCESS) {
        TEST_FAIL("Failed to set response");
        provider->cleanup(provider);
        return;
    }

    provider->init(provider);
    provider->connect(provider);

    free(last_response);
    last_response = NULL;

    result = provider->query(provider, "Test prompt", test_callback, NULL);
    if (result != ARGO_SUCCESS || !last_response || strcmp(last_response, custom) != 0) {
        TEST_FAIL("Wrong custom response");
        provider->cleanup(provider);
        return;
    }

    provider->cleanup(provider);
    TEST_PASS();
}

/* Test 4: Multiple responses (cycling) */
static void test_response_cycling(void) {
    TEST_START("Cycle through multiple responses");

    ci_provider_t* provider = mock_provider_create("test-model");
    if (!provider) {
        TEST_FAIL("Failed to create provider");
        return;
    }

    const char* responses[] = {"Response 1", "Response 2", "Response 3"};
    int result = mock_provider_set_responses(provider, responses, 3);
    if (result != ARGO_SUCCESS) {
        TEST_FAIL("Failed to set responses");
        provider->cleanup(provider);
        return;
    }

    provider->init(provider);
    provider->connect(provider);

    /* Test cycling through responses */
    for (int i = 0; i < 6; i++) {  /* Test 2 full cycles */
        free(last_response);
        last_response = NULL;

        result = provider->query(provider, "Test", test_callback, NULL);
        if (result != ARGO_SUCCESS || !last_response) {
            TEST_FAIL("Query failed");
            provider->cleanup(provider);
            return;
        }

        const char* expected = responses[i % 3];
        if (strcmp(last_response, expected) != 0) {
            TEST_FAIL("Wrong response in cycle");
            provider->cleanup(provider);
            return;
        }
    }

    provider->cleanup(provider);
    TEST_PASS();
}

/* Test 5: Query history tracking */
static void test_query_history(void) {
    TEST_START("Track query history");

    ci_provider_t* provider = mock_provider_create("test-model");
    if (!provider) {
        TEST_FAIL("Failed to create provider");
        return;
    }

    provider->init(provider);
    provider->connect(provider);

    const char* prompts[] = {"Prompt 1", "Prompt 2", "Prompt 3"};

    for (int i = 0; i < 3; i++) {
        provider->query(provider, prompts[i], test_callback, NULL);

        const char* last = mock_provider_get_last_prompt(provider);
        if (!last || strcmp(last, prompts[i]) != 0) {
            TEST_FAIL("Last prompt not tracked");
            provider->cleanup(provider);
            return;
        }

        int count = mock_provider_get_query_count(provider);
        if (count != i + 1) {
            TEST_FAIL("Query count incorrect");
            provider->cleanup(provider);
            return;
        }
    }

    provider->cleanup(provider);
    TEST_PASS();
}

int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("Mock Provider Tests\n");
    printf("==========================================\n");
    printf("\n");

    test_create_destroy();
    test_default_response();
    test_custom_response();
    test_response_cycling();
    test_query_history();

    printf("\n");
    printf("==========================================\n");
    printf("Test Results\n");
    printf("==========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("==========================================\n");
    printf("\n");

    free(last_response);

    return tests_failed > 0 ? 1 : 0;
}
