/* © 2025 Casey Koons All rights reserved */

/* Registry core - CI management, port allocation, status tracking */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_registry.h"
#include "argo_shutdown.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"
#include "argo_limits.h"
#include "argo_urls.h"

/* Create registry */
ci_registry_t* registry_create(void) {
    ci_registry_t* registry = calloc(1, sizeof(ci_registry_t));
    if (!registry) {
        argo_report_error(E_SYSTEM_MEMORY, "registry_create", ERR_MSG_REGISTRY_ALLOC_FAILED);
        return NULL;
    }

    registry->entries = NULL;
    registry->count = 0;
    registry->initialized = true;

    /* Set default port configuration */
    registry->port_config.base_port = REGISTRY_BASE_PORT;
    registry->port_config.port_max = REGISTRY_PORT_RANGE;
    registry->port_config.builder_offset = REGISTRY_PORT_OFFSET_BUILDER;
    registry->port_config.coordinator_offset = REGISTRY_PORT_OFFSET_COORDINATOR;
    registry->port_config.requirements_offset = REGISTRY_PORT_OFFSET_REQUIREMENTS;
    registry->port_config.analysis_offset = REGISTRY_PORT_OFFSET_ANALYSIS;
    registry->port_config.reserved_offset = REGISTRY_PORT_OFFSET_RESERVED;

    /* Register for graceful shutdown tracking */
    argo_register_registry(registry);

    LOG_INFO("Registry created with base port %d", REGISTRY_BASE_PORT);
    return registry;
}

/* Destroy registry */
void registry_destroy(ci_registry_t* registry) {
    if (!registry) return;

    /* Unregister from shutdown tracking */
    argo_unregister_registry(registry);

    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        ci_registry_entry_t* next = entry->next;
        free(entry);
        entry = next;
    }

    LOG_INFO("Registry destroyed, had %d CIs", registry->count);
    free(registry);
}

/* Load configuration (stub for now) */
int registry_load_config(ci_registry_t* registry, const char* config_path) {
    if (!registry) return E_INPUT_NULL;
    (void)config_path;
    return ARGO_SUCCESS;
}

/* Add CI to registry */
int registry_add_ci(ci_registry_t* registry,
                   const char* name,
                   const char* role,
                   const char* model,
                   int port) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(name);
    ARGO_CHECK_NULL(role);
    ARGO_CHECK_NULL(model);

    if (registry->count >= REGISTRY_MAX_CIS) {
        argo_report_error(E_PROTOCOL_QUEUE, "registry_add_ci", ERR_MSG_REGISTRY_FULL);
        return E_PROTOCOL_QUEUE;
    }

    /* Check if already exists */
    if (registry_find_ci(registry, name)) {
        argo_report_error(E_INPUT_INVALID, "registry_add_ci", ERR_MSG_CI_ALREADY_EXISTS);
        return E_INPUT_INVALID;
    }

    /* Create entry */
    ci_registry_entry_t* entry = calloc(1, sizeof(ci_registry_entry_t));
    if (!entry) {
        return E_SYSTEM_MEMORY;
    }

    strncpy(entry->name, name, REGISTRY_NAME_MAX - 1);
    strncpy(entry->role, role, REGISTRY_ROLE_MAX - 1);
    strncpy(entry->model, model, REGISTRY_MODEL_MAX - 1);
    strncpy(entry->host, DEFAULT_DAEMON_HOST, REGISTRY_HOST_MAX - 1);
    entry->port = port;
    entry->socket_fd = -1;
    entry->status = CI_STATUS_OFFLINE;
    entry->registered_at = time(NULL);
    entry->last_heartbeat = time(NULL);

    /* Add to list */
    entry->next = registry->entries;
    registry->entries = entry;
    registry->count++;

    LOG_INFO("Registered CI: %s (role=%s, model=%s, port=%d)",
             name, role, model, port);

    return ARGO_SUCCESS;
}

/* Remove CI from registry */
int registry_remove_ci(ci_registry_t* registry, const char* name) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(name);

    ci_registry_entry_t** pp = &registry->entries;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            ci_registry_entry_t* entry = *pp;
            *pp = entry->next;
            registry->count--;
            LOG_INFO("Unregistered CI: %s", name);
            free(entry);
            return ARGO_SUCCESS;
        }
        pp = &(*pp)->next;
    }

    argo_report_error(E_INPUT_INVALID, "registry_remove_ci", ERR_MSG_CI_NOT_FOUND);
    return E_INPUT_INVALID;
}

/* Generic find helper with predicate */
typedef bool (*registry_match_fn)(ci_registry_entry_t* entry, const void* criteria);

static ci_registry_entry_t* registry_find_generic(ci_registry_t* registry,
                                                  registry_match_fn match,
                                                  const void* criteria) {
    if (!registry || !match) return NULL;
    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (match(entry, criteria)) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/* Match by name */
static bool match_by_name(ci_registry_entry_t* entry, const void* criteria) {
    const char* name = (const char*)criteria;
    return strcmp(entry->name, name) == 0;
}

/* Match by role */
static bool match_by_role(ci_registry_entry_t* entry, const void* criteria) {
    const char* role = (const char*)criteria;
    return strcmp(entry->role, role) == 0;
}

/* Match by role and ready status */
typedef struct {
    const char* role;
    ci_status_t status;
} role_status_criteria_t;

static bool match_by_role_status(ci_registry_entry_t* entry, const void* criteria) {
    const role_status_criteria_t* rs = (const role_status_criteria_t*)criteria;
    return strcmp(entry->role, rs->role) == 0 && entry->status == rs->status;
}

/* Find CI by name */
ci_registry_entry_t* registry_find_ci(ci_registry_t* registry, const char* name) {
    if (!name) return NULL;
    return registry_find_generic(registry, match_by_name, name);
}

/* Get port offset for role */
static int get_role_offset(ci_registry_t* registry, const char* role) {
    if (strcmp(role, "builder") == 0) {
        return registry->port_config.builder_offset;
    } else if (strcmp(role, "coordinator") == 0) {
        return registry->port_config.coordinator_offset;
    } else if (strcmp(role, "requirements") == 0) {
        return registry->port_config.requirements_offset;
    } else if (strcmp(role, "analysis") == 0) {
        return registry->port_config.analysis_offset;
    }
    return registry->port_config.reserved_offset;
}

/* Helper: Reclaim port from offline CI in role's range */
static int reclaim_port_from_offline_ci(ci_registry_t* registry, const char* role) {
    int offset = get_role_offset(registry, role);
    int base = registry->port_config.base_port + offset;
    int range_end = base + REGISTRY_PORTS_PER_ROLE;

    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        /* Check if CI is offline and has port in this role's range */
        if (entry->status == CI_STATUS_OFFLINE &&
            entry->port >= base &&
            entry->port < range_end) {
            int reclaimed_port = entry->port;
            LOG_INFO("Reclaiming port %d from offline CI: %s", reclaimed_port, entry->name);

            /* Remove the offline CI from registry */
            registry_remove_ci(registry, entry->name);

            return reclaimed_port;
        }
        entry = entry->next;
    }

    return -1;  /* No offline CI found */
}

/* Allocate port for role */
int registry_allocate_port(ci_registry_t* registry, const char* role) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(role);

    int offset = get_role_offset(registry, role);
    int base = registry->port_config.base_port + offset;

    /* Find first available port in role's range */
    for (int i = 0; i < REGISTRY_PORTS_PER_ROLE; i++) {
        int port = base + i;
        if (registry_is_port_available(registry, port)) {
            return port;
        }
    }

    /* No free port - try to reclaim from offline CI */
    int reclaimed_port = reclaim_port_from_offline_ci(registry, role);
    if (reclaimed_port >= 0) {
        return reclaimed_port;
    }

    argo_report_error(E_PROTOCOL_QUEUE, "registry_allocate_port", ERR_MSG_PORT_ALLOCATION_FAILED);
    return -1;
}

/* Get port for role instance */
int registry_get_port_for_role(ci_registry_t* registry,
                              const char* role,
                              int instance) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(role);
    ARGO_CHECK_RANGE(instance, 0, 9);

    int offset = get_role_offset(registry, role);
    return registry->port_config.base_port + offset + instance;
}

/* Check if port is available */
bool registry_is_port_available(ci_registry_t* registry, int port) {
    if (!registry) return false;

    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (entry->port == port) {
            return false;
        }
        entry = entry->next;
    }
    return true;
}

/* Find CI by role (first match) */
ci_registry_entry_t* registry_find_by_role(ci_registry_t* registry, const char* role) {
    if (!role) return NULL;
    return registry_find_generic(registry, match_by_role, role);
}

/* Find all CIs by role */
ci_registry_entry_t** registry_find_all_by_role(ci_registry_t* registry,
                                               const char* role,
                                               int* count) {
    if (!registry || !role || !count) return NULL;
    /* Count matches using the same predicate */
    int match_count = 0;
    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (match_by_role(entry, role)) match_count++;
        entry = entry->next;
    }
    if (match_count == 0) {
        *count = 0;
        return NULL;
    }
    /* Allocate result array */
    ci_registry_entry_t** results = malloc(match_count * sizeof(ci_registry_entry_t*));
    if (!results) {
        *count = 0;
        return NULL;
    }
    /* Fill array using the same predicate */
    int idx = 0;
    entry = registry->entries;
    while (entry && idx < match_count) {
        if (match_by_role(entry, role)) {
            results[idx++] = entry;
        }
        entry = entry->next;
    }
    *count = match_count;
    return results;
}

/* Find available CI for role */
ci_registry_entry_t* registry_find_available(ci_registry_t* registry, const char* role) {
    if (!role) return NULL;
    role_status_criteria_t criteria = { role, CI_STATUS_READY };
    return registry_find_generic(registry, match_by_role_status, &criteria);
}

/* Update CI status */
int registry_update_status(ci_registry_t* registry,
                          const char* name,
                          ci_status_t status) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(name);

    ci_registry_entry_t* entry = registry_find_ci(registry, name);
    if (!entry) {
        return E_INPUT_INVALID;
    }

    entry->status = status;
    return ARGO_SUCCESS;
}

/* Record heartbeat */
int registry_heartbeat(ci_registry_t* registry, const char* name) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(name);

    ci_registry_entry_t* entry = registry_find_ci(registry, name);
    if (!entry) {
        return E_INPUT_INVALID;
    }

    entry->last_heartbeat = time(NULL);
    return ARGO_SUCCESS;
}

/* Check health of all CIs */
int registry_check_health(ci_registry_t* registry) {
    ARGO_CHECK_NULL(registry);

    time_t now = time(NULL);
    int stale_count = 0;

    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        if (entry->status != CI_STATUS_OFFLINE &&
            (now - entry->last_heartbeat) > HEALTH_CHECK_STALE_SECONDS) {
            LOG_WARN("CI %s heartbeat stale (%lds ago)",
                    entry->name, (long)(now - entry->last_heartbeat));
            stale_count++;
        }
        entry = entry->next;
    }

    return stale_count;
}

/* Socket operations (stubs) */
int registry_connect_ci(ci_registry_t* registry, const char* name) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(name);
    return ARGO_SUCCESS;
}

int registry_disconnect_ci(ci_registry_t* registry, const char* name) {
    ARGO_CHECK_NULL(registry);
    ARGO_CHECK_NULL(name);
    return ARGO_SUCCESS;
}

bool registry_is_connected(ci_registry_t* registry, const char* name) {
    if (!registry || !name) return false;

    ci_registry_entry_t* entry = registry_find_ci(registry, name);
    return entry && entry->socket_fd >= 0;
}

/* Print status */
void registry_print_status(ci_registry_t* registry) {
    if (!registry) return;

    printf("Registry Status: %d CIs\n", registry->count);
    ci_registry_entry_t* entry = registry->entries;
    while (entry) {
        registry_print_entry(entry);
        entry = entry->next;
    }
}

void registry_print_entry(ci_registry_entry_t* entry) {
    if (!entry) return;

    const char* status_names[] = {
        "OFFLINE", "STARTING", "READY", "BUSY", "ERROR", "SHUTDOWN"
    };

    printf("  %s (%s): %s on %s:%d [%s]\n",
           entry->name, entry->role, entry->model,
           entry->host, entry->port,
           status_names[entry->status]);
}
