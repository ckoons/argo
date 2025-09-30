/* © 2025 Casey Koons All rights reserved */

/* ACTUAL API CALL TEST - THIS WILL INCUR COSTS! */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "argo_ci.h"
#include "argo_api_providers.h"
#include "argo_error.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* Simple callback to capture response */
static void test_callback(const ci_response_t* response, void* userdata) {
    const char* provider = (const char*)userdata;

    if (response->success) {
        printf("  ✓ %s responded: %.50s...\n", provider, response->content);
        g_tests_passed++;
    } else {
        printf("  ✗ %s failed with error: %s\n", provider,
               argo_error_string(response->error_code));
        g_tests_failed++;
    }
}

/* Test a single provider */
static int test_provider(const char* name, ci_provider_t* provider) {
    g_tests_run++;

    printf("\n%s:\n", name);

    if (!provider) {
        printf("  ✗ Failed to create provider\n");
        g_tests_failed++;
        return -1;
    }

    /* Initialize */
    int result = provider->init(provider);
    if (result != ARGO_SUCCESS) {
        printf("  ✗ Failed to initialize: %s\n", argo_error_string(result));
        provider->cleanup(provider);
        g_tests_failed++;
        return -1;
    }

    /* Simple test prompt - minimal tokens */
    const char* prompt = "Reply with just 'OK' and nothing else.";

    /* Query */
    result = provider->query(provider, prompt, test_callback, (void*)name);
    if (result != ARGO_SUCCESS) {
        printf("  ✗ Query failed: %s\n", argo_error_string(result));
        provider->cleanup(provider);
        g_tests_failed++;
        return -1;
    }

    /* Cleanup */
    provider->cleanup(provider);
    return 0;
}

int main(int argc, char* argv[]) {
    printf("\n========================================\n");
    printf("ARGO API PROVIDER TEST\n");
    printf("WARNING: THIS WILL MAKE ACTUAL API CALLS\n");
    printf("AND INCUR COSTS!\n");
    printf("========================================\n\n");

    if (argc < 2 || strcmp(argv[1], "--yes-i-want-to-spend-money") != 0) {
        printf("To run this test, use:\n");
        printf("  %s --yes-i-want-to-spend-money\n\n", argv[0]);
        printf("Or to test a specific provider:\n");
        printf("  %s --yes-i-want-to-spend-money claude\n", argv[0]);
        printf("  %s --yes-i-want-to-spend-money openai\n", argv[0]);
        printf("  %s --yes-i-want-to-spend-money gemini\n", argv[0]);
        printf("  %s --yes-i-want-to-spend-money grok\n", argv[0]);
        printf("  %s --yes-i-want-to-spend-money deepseek\n", argv[0]);
        printf("  %s --yes-i-want-to-spend-money openrouter\n", argv[0]);
        return 1;
    }

    /* Check if specific provider requested */
    const char* specific = (argc > 2) ? argv[2] : NULL;

    printf("Testing API Providers (Simple Test):\n");
    printf("=====================================\n");

    /* Test each provider if available */
    if (!specific || strcmp(specific, "claude") == 0) {
        if (claude_api_is_available()) {
            test_provider("Claude API", claude_api_create_provider(NULL));
        } else {
            printf("\nClaude API: Not configured\n");
        }
    }

    if (!specific || strcmp(specific, "openai") == 0) {
        if (openai_api_is_available()) {
            test_provider("OpenAI", openai_api_create_provider(NULL));
        } else {
            printf("\nOpenAI: Not configured\n");
        }
    }

    if (!specific || strcmp(specific, "gemini") == 0) {
        if (gemini_api_is_available()) {
            test_provider("Gemini", gemini_api_create_provider(NULL));
        } else {
            printf("\nGemini: Not configured\n");
        }
    }

    if (!specific || strcmp(specific, "grok") == 0) {
        if (grok_api_is_available()) {
            test_provider("Grok", grok_api_create_provider(NULL));
        } else {
            printf("\nGrok: Not configured\n");
        }
    }

    if (!specific || strcmp(specific, "deepseek") == 0) {
        if (deepseek_api_is_available()) {
            test_provider("DeepSeek", deepseek_api_create_provider(NULL));
        } else {
            printf("\nDeepSeek: Not configured\n");
        }
    }

    if (!specific || strcmp(specific, "openrouter") == 0) {
        if (openrouter_is_available()) {
            test_provider("OpenRouter", openrouter_create_provider(NULL));
        } else {
            printf("\nOpenRouter: Not configured\n");
        }
    }

    /* Summary */
    printf("\n=====================================\n");
    printf("Tests run:    %d\n", g_tests_run);
    printf("Tests passed: %d\n", g_tests_passed);
    printf("Tests failed: %d\n", g_tests_failed);
    printf("=====================================\n\n");

    return (g_tests_failed > 0) ? 1 : 0;
}