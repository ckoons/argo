/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_CI_H
#define ARGO_CI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* CI name and role limits */
#define CI_NAME_MAX 32
#define CI_ROLE_MAX 32
#define CI_MODEL_MAX 64

/* CI roles (configurations) */
#define CI_ROLE_BUILDER "builder"
#define CI_ROLE_COORDINATOR "coordinator"
#define CI_ROLE_REQUIREMENTS "requirements"
#define CI_ROLE_ANALYSIS "analysis"

/* Forward declarations */
typedef struct ci_provider ci_provider_t;
typedef struct ci_session ci_session_t;
typedef struct ci_response ci_response_t;
typedef struct ci_context ci_context_t;
typedef struct ci_memory_digest ci_memory_digest_t;

/* CI response structure */
struct ci_response {
    char* content;
    size_t content_len;
    int status_code;
    char* error_message;
    void* provider_data;
};

/* CI context for a session */
struct ci_context {
    /* Identity */
    char name[CI_NAME_MAX];        /* "Argo", "Maia", etc */
    char role[CI_ROLE_MAX];        /* "builder", "requirements" */

    /* Memory */
    ci_memory_digest_t* memory;
    char* sunset_notes;             /* From previous session */
    char* sunrise_brief;            /* For this session */

    /* Session info */
    char* task_description;
    char* project_overview;
    char* team_roles;               /* Who does what */
    char* relationships;            /* How to interact */

    /* Provider context */
    void* provider_context;
};

/* Function pointers for provider operations */
typedef int (*ci_init_fn)(void* config);
typedef int (*ci_connect_fn)(ci_context_t* ctx);
typedef ci_response_t* (*ci_query_fn)(ci_context_t* ctx, const char* prompt);
typedef int (*ci_stream_fn)(ci_context_t* ctx, const char* prompt,
                           void (*callback)(const char* chunk, void* data),
                           void* user_data);
typedef void (*ci_cleanup_fn)(ci_context_t* ctx);

/* CI provider structure */
struct ci_provider {
    /* Provider identity */
    char name[CI_NAME_MAX];
    char model[CI_MODEL_MAX];

    /* Operations */
    ci_init_fn init;
    ci_connect_fn connect;
    ci_query_fn query;
    ci_stream_fn stream;
    ci_cleanup_fn cleanup;

    /* Capabilities */
    bool supports_streaming;
    bool supports_memory;
    bool supports_sunset_sunrise;
    size_t max_context;

    /* Provider-specific data */
    void* provider_data;
};

/* Configuration structures for different roles */
typedef struct builder_config {
    const char* branch_prefix;
    int max_file_size;
    bool auto_format;
} builder_config_t;

typedef struct coordinator_config {
    int max_sessions;
    bool enable_broadcast;
    int coordination_timeout_ms;
} coordinator_config_t;

typedef struct requirements_config {
    bool strict_validation;
    const char* template_path;
    bool require_acceptance_criteria;
} requirements_config_t;

typedef struct analysis_config {
    int analysis_depth;
    bool include_metrics;
    const char* report_format;
} analysis_config_t;

/* CI session management */
ci_session_t* ci_create_session(ci_provider_t* provider,
                                const char* name,
                                const char* role,
                                const char* task);
int ci_destroy_session(ci_session_t* session);
int ci_set_memory(ci_session_t* session, ci_memory_digest_t* memory);
int ci_set_sunset_notes(ci_session_t* session, const char* notes);
char* ci_get_sunrise_notes(ci_session_t* session);

/* CI provider operations */
int ci_register_provider(ci_provider_t* provider);
ci_provider_t* ci_get_provider(const char* name);
int ci_set_default_provider(const char* name);

/* CI communication */
ci_response_t* ci_query(ci_session_t* session, const char* prompt);
int ci_stream_query(ci_session_t* session,
                   const char* prompt,
                   void (*callback)(const char* chunk, void* data),
                   void* user_data);
void ci_free_response(ci_response_t* response);

/* CI onboarding */
int ci_onboard(ci_session_t* session,
              const char* project_overview,
              const char* team_roles,
              const char* relationships);

/* Utility functions */
const char* ci_role_to_string(const char* role);
bool ci_is_valid_role(const char* role);
size_t ci_get_context_limit(ci_provider_t* provider);

#endif /* ARGO_CI_H */