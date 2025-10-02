/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_ENV_INTERNAL_H
#define ARGO_ENV_INTERNAL_H

#include <pthread.h>
#include <stdbool.h>

/* Shared environment state */
extern char** argo_env;
extern int argo_env_count;
extern int argo_env_capacity;
extern pthread_mutex_t argo_env_mutex;
extern bool argo_env_initialized;

/* Internal helper functions */
int grow_env_array(void);
int find_env_index(const char* name);
int set_env_internal(const char* name, const char* value);

#endif /* ARGO_ENV_INTERNAL_H */
