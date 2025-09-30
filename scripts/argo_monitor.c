/* © 2025 Casey Koons All rights reserved */

/* Registry monitoring utility - display CI status */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include "../include/argo_registry.h"
#include "../include/argo_error.h"

static void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --watch, -w          Continuous monitoring (refresh every 2s)\n");
    printf("  --role ROLE, -r ROLE Filter by role\n");
    printf("  --json, -j           JSON output format\n");
    printf("  --help, -h           Show this help\n");
    printf("\n");
}

static void print_header(void) {
    printf("\n");
    printf("ARGO REGISTRY STATUS\n");
    printf("=================================================\n");
    printf("%-6s %-15s %-12s %-20s %-8s %s\n",
           "PORT", "NAME", "ROLE", "MODEL", "STATUS", "HEARTBEAT");
    printf("-------------------------------------------------\n");
}

static const char* status_name(ci_status_t status) {
    static const char* names[] = {
        "OFFLINE", "STARTING", "READY", "BUSY", "ERROR", "SHUTDOWN"
    };
    if (status >= 0 && status <= CI_STATUS_SHUTDOWN) {
        return names[status];
    }
    return "UNKNOWN";
}

static void print_entry(ci_registry_entry_t* entry, bool show_heartbeat) {
    const char* status_str = status_name(entry->status);

    if (show_heartbeat) {
        time_t now = time(NULL);
        long age = (long)(now - entry->last_heartbeat);

        printf("%-6d %-15s %-12s %-20s %-8s %lds ago\n",
               entry->port, entry->name, entry->role,
               entry->model, status_str, age);
    } else {
        printf("%-6d %-15s %-12s %-20s %-8s\n",
               entry->port, entry->name, entry->role,
               entry->model, status_str);
    }
}

static void print_registry_status(ci_registry_t* registry, const char* role_filter) {
    print_header();

    int shown = 0;
    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (!role_filter || strcmp(entry->role, role_filter) == 0) {
            print_entry(entry, true);
            shown++;
        }
        entry = entry->next;
    }

    printf("-------------------------------------------------\n");
    if (role_filter) {
        printf("Total: %d CIs (role=%s)\n", shown, role_filter);
    } else {
        printf("Total: %d CIs\n", shown);
    }
    printf("\n");
}

static void print_registry_json(ci_registry_t* registry, const char* role_filter) {
    printf("{\"registry\":{\"count\":%d,\"entries\":[", registry->count);

    bool first = true;
    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (!role_filter || strcmp(entry->role, role_filter) == 0) {
            if (!first) printf(",");
            printf("{\"name\":\"%s\",\"role\":\"%s\",\"model\":\"%s\","
                   "\"port\":%d,\"status\":\"%s\",\"heartbeat\":%ld}",
                   entry->name, entry->role, entry->model,
                   entry->port, status_name(entry->status),
                   (long)entry->last_heartbeat);
            first = false;
        }
        entry = entry->next;
    }

    printf("]}}\n");
}

int main(int argc, char* argv[]) {
    bool watch_mode = false;
    bool json_mode = false;
    const char* role_filter = NULL;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--watch") == 0 || strcmp(argv[i], "-w") == 0) {
            watch_mode = true;
        } else if (strcmp(argv[i], "--json") == 0 || strcmp(argv[i], "-j") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--role") == 0 || strcmp(argv[i], "-r") == 0) {
            if (i + 1 < argc) {
                role_filter = argv[++i];
            } else {
                fprintf(stderr, "Error: --role requires argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Create a test registry for demonstration */
    ci_registry_t* registry = registry_create();
    if (!registry) {
        fprintf(stderr, "Failed to create registry\n");
        return 1;
    }

    /* Add some test entries */
    registry_add_ci(registry, "builder-1", "builder", "gpt-4o", 9000);
    registry_add_ci(registry, "coordinator", "coordinator", "claude-sonnet-4-5", 9010);
    registry_add_ci(registry, "requirements-a", "requirements", "gemini-2.5-flash", 9020);

    /* Update some statuses */
    registry_update_status(registry, "builder-1", CI_STATUS_READY);
    registry_update_status(registry, "coordinator", CI_STATUS_BUSY);
    registry_update_status(registry, "requirements-a", CI_STATUS_READY);

    /* Simulate heartbeats */
    registry_heartbeat(registry, "builder-1");
    registry_heartbeat(registry, "coordinator");
    registry_heartbeat(registry, "requirements-a");

    do {
        if (json_mode) {
            print_registry_json(registry, role_filter);
        } else {
            if (watch_mode) {
                printf("\033[2J\033[H");  /* Clear screen */
            }
            print_registry_status(registry, role_filter);
        }

        if (watch_mode && !json_mode) {
            sleep(2);
            /* In real implementation, would reload registry state */
        }
    } while (watch_mode && !json_mode);

    registry_destroy(registry);
    return 0;
}