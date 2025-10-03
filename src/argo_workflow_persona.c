/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External library - header only for struct definitions */
#define JSMN_HEADER
#include "jsmn.h"

/* Project includes */
#include "argo_workflow_persona.h"
#include "argo_workflow_json.h"
#include "argo_error.h"
#include "argo_log.h"

/* Create persona registry */
persona_registry_t* persona_registry_create(void) {
    persona_registry_t* registry = calloc(1, sizeof(persona_registry_t));
    if (!registry) {
        argo_report_error(E_SYSTEM_MEMORY, "persona_registry_create", "allocation failed");
        return NULL;
    }

    registry->count = 0;
    registry->default_persona[0] = '\0';

    LOG_DEBUG("Created persona registry");
    return registry;
}

/* Destroy persona registry */
void persona_registry_destroy(persona_registry_t* registry) {
    if (!registry) return;
    free(registry);
}

/* Add persona to registry */
int persona_registry_add(persona_registry_t* registry,
                         const char* name,
                         const char* role,
                         const char* style,
                         const char* greeting) {
    if (!registry || !name || !role) {
        argo_report_error(E_INPUT_NULL, "persona_registry_add", "parameter is NULL");
        return E_INPUT_NULL;
    }

    if (registry->count >= PERSONA_MAX_COUNT) {
        argo_report_error(E_INPUT_TOO_LARGE, "persona_registry_add", "max personas reached");
        return E_INPUT_TOO_LARGE;
    }

    workflow_persona_t* persona = &registry->personas[registry->count];

    strncpy(persona->name, name, sizeof(persona->name) - 1);
    strncpy(persona->role, role, sizeof(persona->role) - 1);

    if (style) {
        strncpy(persona->style, style, sizeof(persona->style) - 1);
    } else {
        persona->style[0] = '\0';
    }

    if (greeting) {
        strncpy(persona->greeting, greeting, sizeof(persona->greeting) - 1);
    } else {
        persona->greeting[0] = '\0';
    }

    registry->count++;

    LOG_DEBUG("Added persona '%s' (role: %s)", name, role);
    return ARGO_SUCCESS;
}

/* Find persona by name */
workflow_persona_t* persona_registry_find(persona_registry_t* registry,
                                          const char* name) {
    if (!registry || !name) {
        return NULL;
    }

    for (int i = 0; i < registry->count; i++) {
        if (strcmp(registry->personas[i].name, name) == 0) {
            return &registry->personas[i];
        }
    }

    return NULL;
}

/* Get default persona */
workflow_persona_t* persona_registry_get_default(persona_registry_t* registry) {
    if (!registry) {
        return NULL;
    }

    if (registry->default_persona[0] == '\0') {
        return NULL;
    }

    return persona_registry_find(registry, registry->default_persona);
}

/* Set default persona */
int persona_registry_set_default(persona_registry_t* registry,
                                 const char* name) {
    if (!registry || !name) {
        argo_report_error(E_INPUT_NULL, "persona_registry_set_default", "parameter is NULL");
        return E_INPUT_NULL;
    }

    strncpy(registry->default_persona, name, sizeof(registry->default_persona) - 1);
    LOG_DEBUG("Set default persona to '%s'", name);
    return ARGO_SUCCESS;
}

/* Parse personas from workflow JSON */
int persona_registry_parse_json(persona_registry_t* registry,
                                const char* json,
                                void* token_ptr,
                                int token_count) {
    (void)token_count;  /* Not used - token count determined from token structure */

    if (!registry || !json || !token_ptr) {
        argo_report_error(E_INPUT_NULL, "persona_registry_parse_json", "parameter is NULL");
        return E_INPUT_NULL;
    }

    jsmntok_t* tokens = (jsmntok_t*)token_ptr;

    /* Find personas object in workflow */
    int personas_idx = workflow_json_find_field(json, tokens, 0, "personas");
    if (personas_idx < 0 || tokens[personas_idx].type != JSMN_OBJECT) {
        /* No personas defined - not an error */
        LOG_DEBUG("No personas section in workflow");
        return ARGO_SUCCESS;
    }

    /* Parse personas object */
    jsmntok_t* personas_obj = &tokens[personas_idx];
    int child_count = personas_obj->size;
    int current_token = personas_idx + 1;

    for (int i = 0; i < child_count; i++) {
        /* Get key name */
        if (tokens[current_token].type != JSMN_STRING) {
            current_token++;
            continue;
        }

        char key[PERSONA_NAME_MAX];
        int key_len = tokens[current_token].end - tokens[current_token].start;
        if (key_len >= (int)sizeof(key)) {
            key_len = sizeof(key) - 1;
        }
        strncpy(key, json + tokens[current_token].start, key_len);
        key[key_len] = '\0';
        current_token++;

        /* Check if this is the default field */
        if (strcmp(key, "default") == 0) {
            /* Get default persona name */
            if (tokens[current_token].type == JSMN_STRING) {
                char default_name[PERSONA_NAME_MAX];
                workflow_json_extract_string(json, &tokens[current_token],
                                           default_name, sizeof(default_name));
                persona_registry_set_default(registry, default_name);
            }
            current_token++;
            continue;
        }

        /* This should be a persona object */
        if (tokens[current_token].type != JSMN_OBJECT) {
            current_token++;
            continue;
        }

        /* Parse persona fields */
        char name[PERSONA_NAME_MAX] = "";
        char role[PERSONA_ROLE_MAX] = "";
        char style[PERSONA_STYLE_MAX] = "";
        char greeting[PERSONA_GREETING_MAX] = "";

        /* Use JSON key as persona identifier (for lookup) */
        strncpy(name, key, sizeof(name) - 1);

        /* Find role field */
        int role_idx = workflow_json_find_field(json, tokens, current_token, "role");
        if (role_idx >= 0) {
            workflow_json_extract_string(json, &tokens[role_idx], role, sizeof(role));
        }

        /* Find style field */
        int style_idx = workflow_json_find_field(json, tokens, current_token, "style");
        if (style_idx >= 0) {
            workflow_json_extract_string(json, &tokens[style_idx], style, sizeof(style));
        }

        /* Find greeting field */
        int greeting_idx = workflow_json_find_field(json, tokens, current_token, "greeting");
        if (greeting_idx >= 0) {
            workflow_json_extract_string(json, &tokens[greeting_idx], greeting, sizeof(greeting));
        }

        /* Add persona to registry */
        persona_registry_add(registry, name, role, style, greeting);

        /* Skip to next persona */
        int persona_tokens = workflow_json_count_tokens(tokens, current_token);
        current_token += persona_tokens;
    }

    LOG_DEBUG("Parsed %d personas from workflow", registry->count);
    return ARGO_SUCCESS;
}
