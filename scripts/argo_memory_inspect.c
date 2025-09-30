/* © 2025 Casey Koons All rights reserved */

/* Memory digest inspection utility */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "../include/argo_memory.h"
#include "../include/argo_error.h"

static void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --session ID, -s ID      Session ID to inspect\n");
    printf("  --type TYPE, -t TYPE     Filter by memory type\n");
    printf("  --min-relevance N, -r N  Minimum relevance score (0.0-1.0)\n");
    printf("  --size                   Show size information only\n");
    printf("  --json, -j               JSON output format\n");
    printf("  --help, -h               Show this help\n");
    printf("\n");
    printf("Memory types:\n");
    printf("  FACT, DECISION, APPROACH, ERROR, SUCCESS, BREADCRUMB, RELATIONSHIP\n");
    printf("\n");
}

static const char* type_name(memory_type_t type) {
    static const char* names[] = {
        "FACT", "DECISION", "APPROACH", "ERROR",
        "SUCCESS", "BREADCRUMB", "RELATIONSHIP"
    };
    if (type >= 0 && type <= MEMORY_TYPE_RELATIONSHIP) {
        return names[type];
    }
    return "UNKNOWN";
}

static memory_type_t parse_type(const char* str) {
    if (strcasecmp(str, "FACT") == 0) return MEMORY_TYPE_FACT;
    if (strcasecmp(str, "DECISION") == 0) return MEMORY_TYPE_DECISION;
    if (strcasecmp(str, "APPROACH") == 0) return MEMORY_TYPE_APPROACH;
    if (strcasecmp(str, "ERROR") == 0) return MEMORY_TYPE_ERROR;
    if (strcasecmp(str, "SUCCESS") == 0) return MEMORY_TYPE_SUCCESS;
    if (strcasecmp(str, "BREADCRUMB") == 0) return MEMORY_TYPE_BREADCRUMB;
    if (strcasecmp(str, "RELATIONSHIP") == 0) return MEMORY_TYPE_RELATIONSHIP;
    return (memory_type_t)-1;
}

static void print_header(const char* session_id) {
    printf("\n");
    printf("ARGO MEMORY DIGEST\n");
    printf("=================================================\n");
    printf("Session: %s\n", session_id);
    printf("=================================================\n");
}

static void print_item(memory_item_t* item) {
    char created_str[32];
    struct tm* tm_info = localtime(&item->created);
    strftime(created_str, sizeof(created_str), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("  [%u] %-12s relevance=%.2f  accessed=%dx\n",
           item->id, type_name(item->type),
           item->relevance.score, item->relevance.access_count);
    printf("      %s\n", item->content);
    printf("      Created: %s", created_str);
    if (item->creator_ci) {
        printf(" by %s", item->creator_ci);
    }
    printf("\n\n");
}

static void print_digest_summary(ci_memory_digest_t* digest,
                                memory_type_t type_filter,
                                float min_relevance) {
    print_header(digest->session_id);

    printf("CI: %s\n", digest->ci_name);

    char created_str[32];
    struct tm* tm_info = localtime(&digest->created);
    strftime(created_str, sizeof(created_str), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("Created: %s\n", created_str);

    size_t current_size = memory_calculate_size(digest);
    printf("Size: %zu / %zu bytes (%.1f%%)\n",
           current_size, digest->max_allowed_size,
           (current_size * 100.0) / digest->max_allowed_size);

    printf("\n");
    printf("Memory Items (%d):\n", digest->selected_count);
    printf("-------------------------------------------------\n");

    int shown = 0;
    for (int i = 0; i < digest->selected_count; i++) {
        memory_item_t* item = digest->selected[i];
        if (!item) continue;

        if (type_filter != (memory_type_t)-1 && item->type != type_filter) {
            continue;
        }

        if (item->relevance.score < min_relevance) {
            continue;
        }

        print_item(item);
        shown++;
    }

    if (shown == 0) {
        printf("  (no items match filter)\n\n");
    }

    if (digest->breadcrumb_count > 0) {
        printf("Breadcrumbs (%d):\n", digest->breadcrumb_count);
        printf("-------------------------------------------------\n");
        for (int i = 0; i < digest->breadcrumb_count; i++) {
            printf("  - %s\n", digest->breadcrumbs[i]);
        }
        printf("\n");
    }

    if (digest->sunset_notes) {
        printf("Sunset Notes:\n");
        printf("-------------------------------------------------\n");
        printf("%s\n\n", digest->sunset_notes);
    }

    if (digest->sunrise_brief) {
        printf("Sunrise Brief:\n");
        printf("-------------------------------------------------\n");
        printf("%s\n\n", digest->sunrise_brief);
    }
}

static void print_size_info(ci_memory_digest_t* digest) {
    size_t current_size = memory_calculate_size(digest);

    printf("\n");
    printf("Memory Digest Size Information\n");
    printf("=================================================\n");
    printf("Session:      %s\n", digest->session_id);
    printf("Current size: %zu bytes\n", current_size);
    printf("Max allowed:  %zu bytes\n", digest->max_allowed_size);
    printf("Used:         %.1f%%\n",
           (current_size * 100.0) / digest->max_allowed_size);
    printf("Remaining:    %zu bytes\n",
           digest->max_allowed_size - current_size);
    printf("Items:        %d\n", digest->selected_count);
    printf("Breadcrumbs:  %d\n", digest->breadcrumb_count);
    printf("\n");
}

int main(int argc, char* argv[]) {
    const char* session_id = "demo-session";
    memory_type_t type_filter = (memory_type_t)-1;
    float min_relevance = 0.0f;
    bool size_only = false;
    bool json_mode = false;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--session") == 0 || strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) {
                session_id = argv[++i];
            }
        } else if (strcmp(argv[i], "--type") == 0 || strcmp(argv[i], "-t") == 0) {
            if (i + 1 < argc) {
                type_filter = parse_type(argv[++i]);
                if (type_filter == (memory_type_t)-1) {
                    fprintf(stderr, "Invalid memory type\n");
                    return 1;
                }
            }
        } else if (strcmp(argv[i], "--min-relevance") == 0 || strcmp(argv[i], "-r") == 0) {
            if (i + 1 < argc) {
                min_relevance = atof(argv[++i]);
            }
        } else if (strcmp(argv[i], "--size") == 0) {
            size_only = true;
        } else if (strcmp(argv[i], "--json") == 0 || strcmp(argv[i], "-j") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    /* Create demo digest */
    ci_memory_digest_t* digest = memory_digest_create(8000);
    if (!digest) {
        fprintf(stderr, "Failed to create memory digest\n");
        return 1;
    }

    strncpy(digest->session_id, session_id, sizeof(digest->session_id) - 1);
    strncpy(digest->ci_name, "demo-ci", sizeof(digest->ci_name) - 1);

    /* Add sample memory items */
    memory_add_item(digest, MEMORY_TYPE_FACT,
                   "Project uses C11 standard with strict compilation flags",
                   "builder-1");
    memory_add_item(digest, MEMORY_TYPE_DECISION,
                   "Chose 50% context limit for memory digests",
                   "coordinator");
    memory_add_item(digest, MEMORY_TYPE_SUCCESS,
                   "Registry and memory tests all passed",
                   "builder-1");
    memory_add_item(digest, MEMORY_TYPE_APPROACH,
                   "Using C scripts linked against libargo_core.a",
                   "coordinator");
    memory_add_item(digest, MEMORY_TYPE_BREADCRUMB,
                   "Remember to implement lifecycle management next",
                   "coordinator");

    /* Add breadcrumbs */
    memory_add_breadcrumb(digest, "Consider performance optimizations");
    memory_add_breadcrumb(digest, "Update documentation after completion");

    /* Set sunset/sunrise */
    memory_set_sunset_notes(digest, "Work in progress on C script utilities");
    memory_set_sunrise_brief(digest, "Continue with lifecycle management implementation");

    /* Update some relevance scores */
    if (digest->selected_count > 1) {
        memory_update_relevance(digest->selected[1], 0.9f);
    }

    /* Display */
    if (json_mode) {
        char* json = memory_digest_to_json(digest);
        if (json) {
            printf("%s\n", json);
            free(json);
        }
    } else if (size_only) {
        print_size_info(digest);
    } else {
        print_digest_summary(digest, type_filter, min_relevance);
    }

    memory_digest_destroy(digest);
    return 0;
}