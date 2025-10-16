/* © 2025 Casey Koons All rights reserved */
/* Daemon Workflow Helpers - JSON parsing and ID generation */

#ifndef ARGO_DAEMON_WORKFLOW_HELPERS_H
#define ARGO_DAEMON_WORKFLOW_HELPERS_H

#include "argo_workflow_registry.h"

/* Parse args array from JSON body */
int parse_args_from_json(const char* json_body, char*** args_out, int* arg_count_out);

/* Parse env object from JSON body */
int parse_env_from_json(const char* json_body, char*** env_keys_out,
                        char*** env_values_out, int* env_count_out);

/* Free allocated args/env arrays */
void free_workflow_params(char** args, int arg_count, char** env_keys,
                          char** env_values, int env_count);

/* Generate workflow instance ID from template and instance suffix */
int generate_workflow_id(workflow_registry_t* registry, const char* template_name,
                        const char* instance_suffix, char* workflow_id,
                        size_t workflow_id_size);

#endif /* ARGO_DAEMON_WORKFLOW_HELPERS_H */
