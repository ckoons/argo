/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_PERSONA_H
#define ARGO_WORKFLOW_PERSONA_H

/* Persona buffer sizes */
#define PERSONA_NAME_MAX 64
#define PERSONA_ROLE_MAX 128
#define PERSONA_STYLE_MAX 256
#define PERSONA_GREETING_MAX 512
#define PERSONA_MAX_COUNT 10

/* Persona definition */
typedef struct workflow_persona {
    char name[PERSONA_NAME_MAX];
    char role[PERSONA_ROLE_MAX];
    char style[PERSONA_STYLE_MAX];
    char greeting[PERSONA_GREETING_MAX];
} workflow_persona_t;

/* Persona registry for a workflow */
typedef struct persona_registry {
    workflow_persona_t personas[PERSONA_MAX_COUNT];
    int count;
    char default_persona[PERSONA_NAME_MAX];
} persona_registry_t;

/* Create persona registry */
persona_registry_t* persona_registry_create(void);

/* Destroy persona registry */
void persona_registry_destroy(persona_registry_t* registry);

/* Add persona to registry */
int persona_registry_add(persona_registry_t* registry,
                         const char* name,
                         const char* role,
                         const char* style,
                         const char* greeting);

/* Find persona by name */
workflow_persona_t* persona_registry_find(persona_registry_t* registry,
                                          const char* name);

/* Get default persona */
workflow_persona_t* persona_registry_get_default(persona_registry_t* registry);

/* Set default persona */
int persona_registry_set_default(persona_registry_t* registry,
                                 const char* name);

/* Parse personas from workflow JSON */
int persona_registry_parse_json(persona_registry_t* registry,
                                const char* json,
                                void* tokens,
                                int token_count);

/* Build AI prompt with persona context */
int workflow_persona_build_prompt(workflow_persona_t* persona,
                                   const char* prompt,
                                   char* output,
                                   size_t output_size);

#endif /* ARGO_WORKFLOW_PERSONA_H */
