/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_SHUTDOWN_H
#define ARGO_SHUTDOWN_H

/* Graceful shutdown tracking
 *
 * Tracks active workflows, registries, and lifecycle managers for cleanup.
 * Automatically cleans up on argo_exit() or signal handlers (SIGTERM/SIGINT).
 */

/* Forward declarations */
typedef struct workflow_controller workflow_controller_t;
typedef struct ci_registry ci_registry_t;
typedef struct lifecycle_manager lifecycle_manager_t;
typedef struct shared_services shared_services_t;

/* Register active objects for cleanup tracking */
void argo_register_workflow(workflow_controller_t* workflow);
void argo_unregister_workflow(workflow_controller_t* workflow);

void argo_register_registry(ci_registry_t* registry);
void argo_unregister_registry(ci_registry_t* registry);

void argo_register_lifecycle(lifecycle_manager_t* lifecycle);
void argo_unregister_lifecycle(lifecycle_manager_t* lifecycle);

void argo_set_shared_services(shared_services_t* services);

/* Cleanup all tracked objects (called by argo_exit()) */
void argo_shutdown_cleanup(void);

/* Install signal handlers for graceful shutdown on SIGTERM/SIGINT */
void argo_install_signal_handlers(void);

#endif /* ARGO_SHUTDOWN_H */
