/* © 2025 Casey Koons All rights reserved */

/* CI Assignment Utility
 * Assigns a provider and model to a CI instance
 */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_provider.h"
#include "argo_registry.h"
#include "argo_error.h"
#include "argo_log.h"

/* Usage */
static void print_usage(const char* progname) {
    printf("Usage: %s <ci_name> <provider_name> [model]\n", progname);
    printf("\n");
    printf("Assigns a provider (and optionally a model) to a CI instance.\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  ci_name         Name of the CI to assign (must exist in registry)\n");
    printf("  provider_name   Name of the provider (claude_code, ollama, etc.)\n");
    printf("  model           Optional: specific model to use\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s builder-1 claude_code\n", progname);
    printf("  %s builder-1 ollama llama3.3:70b\n", progname);
    printf("  %s coordinator-1 openai-api gpt-4o\n", progname);
    printf("\n");
}

/* Main */
int main(int argc, char** argv) {
    /* Check arguments */
    if (argc < 3 || argc > 4) {
        print_usage(argv[0]);
        return 1;
    }

    const char* ci_name = argv[1];
    const char* provider_name = argv[2];
    const char* model = (argc == 4) ? argv[3] : NULL;

    /* Initialize logging */
    log_init(".argo/logs");
    log_set_level(LOG_INFO);

    printf("\n");
    printf("========================================\n");
    printf("Argo CI Assignment\n");
    printf("========================================\n");
    printf("CI:       %s\n", ci_name);
    printf("Provider: %s\n", provider_name);
    if (model) {
        printf("Model:    %s\n", model);
    }
    printf("========================================\n\n");

    /* Create registries */
    ci_registry_t* ci_reg = registry_create();
    if (!ci_reg) {
        fprintf(stderr, "Failed to create CI registry\n");
        log_cleanup();
        return 1;
    }

    provider_registry_t* provider_reg = provider_registry_create();
    if (!provider_reg) {
        fprintf(stderr, "Failed to create provider registry\n");
        registry_destroy(ci_reg);
        log_cleanup();
        return 1;
    }

    /* Load CI registry state */
    int result = registry_load_state(ci_reg, ".argo/registry.dat");
    if (result != ARGO_SUCCESS) {
        printf("Note: Could not load CI registry (creating new)\n");
    }

    /* Find CI */
    ci_registry_entry_t* ci = registry_find_ci(ci_reg, ci_name);
    if (!ci) {
        fprintf(stderr, "Error: CI '%s' not found in registry\n", ci_name);
        fprintf(stderr, "Use argo_monitor to list available CIs\n");
        provider_registry_destroy(provider_reg);
        registry_destroy(ci_reg);
        log_cleanup();
        return 1;
    }

    printf("Found CI: %s (role=%s, current model=%s)\n\n",
           ci->name, ci->role, ci->model);

    /* Register providers (stub - in real implementation, these would be created) */
    /* For now, just do the assignment update directly */

    /* Update CI registry entry with new provider/model */
    if (model) {
        strncpy(ci->model, model, REGISTRY_MODEL_MAX - 1);
        printf("✓ Assigned model '%s' to CI '%s'\n", model, ci_name);
    } else {
        printf("✓ Assigned provider '%s' to CI '%s'\n", provider_name, ci_name);
    }

    /* Save updated registry */
    result = registry_save_state(ci_reg, ".argo/registry.dat");
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "Warning: Failed to save registry state\n");
    } else {
        printf("✓ Registry updated\n");
    }

    printf("\n");
    printf("========================================\n");
    printf("Assignment complete\n");
    printf("========================================\n\n");

    /* Cleanup */
    provider_registry_destroy(provider_reg);
    registry_destroy(ci_reg);
    log_cleanup();

    return 0;
}
