/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_MEMORY_H
#define ARGO_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Memory limits - enforce 50% context rule */
#define MEMORY_MAX_PERCENTAGE 50
#define MEMORY_MAX_ITEMS 100
#define MEMORY_BREADCRUMB_MAX 20
#define MEMORY_SUGGESTION_MAX 10

/* Memory JSON buffer size */
#define MEMORY_JSON_BUFFER_SIZE 8192

/* Memory item types */
typedef enum memory_type {
    MEMORY_TYPE_FACT,           /* Facts about the project */
    MEMORY_TYPE_DECISION,       /* Decisions made */
    MEMORY_TYPE_APPROACH,       /* Approaches tried */
    MEMORY_TYPE_ERROR,          /* Errors encountered */
    MEMORY_TYPE_SUCCESS,        /* Successful patterns */
    MEMORY_TYPE_BREADCRUMB,     /* CI-left markers */
    MEMORY_TYPE_RELATIONSHIP    /* Team interactions */
} memory_type_t;

/* Memory relevance scoring */
typedef struct memory_relevance {
    float score;                /* 0.0 to 1.0 */
    time_t last_accessed;
    int access_count;
    bool ci_marked_important;   /* CI said this matters */
} memory_relevance_t;

/* Individual memory item */
typedef struct memory_item {
    uint32_t id;
    memory_type_t type;
    char* content;              /* JSON string */
    size_t content_size;
    time_t created;
    char* creator_ci;           /* Which CI created this */
    memory_relevance_t relevance;
    struct memory_item* next;
} memory_item_t;

/* Binary index for fast lookup */
typedef struct memory_index {
    uint32_t hash;              /* Content hash */
    uint32_t memory_id;         /* Memory item ID */
    uint32_t offset;            /* Offset in JSON */
    uint16_t relevance_score;  /* Scaled 0-65535 */
} memory_index_t;

/* Memory digest - what CI gets each turn */
typedef struct ci_memory_digest {
    /* JSON content for CI (must be < 50% of context) */
    char* json_content;
    size_t json_size;
    size_t max_allowed_size;    /* context_limit / 2 */

    /* Suggested memories from deterministic system */
    memory_item_t* suggested[MEMORY_SUGGESTION_MAX];
    int suggestion_count;

    /* CI-selected memories */
    memory_item_t* selected[MEMORY_MAX_ITEMS];
    int selected_count;

    /* CI breadcrumbs for future sessions */
    char* breadcrumbs[MEMORY_BREADCRUMB_MAX];
    int breadcrumb_count;

    /* Sunset/sunrise protocol */
    char* sunset_notes;         /* CI's handoff notes */
    char* sunrise_brief;        /* Briefing for next CI */

    /* Binary index for fast lookup */
    memory_index_t* index;
    size_t index_size;

    /* Metadata */
    char session_id[32];
    char ci_name[32];
    time_t created;
} ci_memory_digest_t;

/* Memory management functions */
ci_memory_digest_t* memory_digest_create(size_t context_limit);
void memory_digest_destroy(ci_memory_digest_t* digest);

/* Adding memories */
int memory_add_item(ci_memory_digest_t* digest,
                   memory_type_t type,
                   const char* content,
                   const char* creator_ci);
int memory_add_breadcrumb(ci_memory_digest_t* digest,
                         const char* breadcrumb);

/* Suggesting memories (deterministic) */
int memory_suggest_relevant(ci_memory_digest_t* digest,
                           const char* task_context,
                           int max_suggestions);
int memory_suggest_by_type(ci_memory_digest_t* digest,
                          memory_type_t type,
                          int max_suggestions);

/* CI selection of memories */
int memory_select_item(ci_memory_digest_t* digest,
                      uint32_t memory_id);
int memory_select_suggested(ci_memory_digest_t* digest,
                           int suggestion_indices[],
                           int count);

/* Sunset/sunrise protocol */
int memory_set_sunset_notes(ci_memory_digest_t* digest,
                           const char* notes);
int memory_set_sunrise_brief(ci_memory_digest_t* digest,
                            const char* brief);

/* JSON serialization */
char* memory_digest_to_json(ci_memory_digest_t* digest);
ci_memory_digest_t* memory_digest_from_json(const char* json,
                                           size_t context_limit);

/* Utility functions */
size_t memory_calculate_size(ci_memory_digest_t* digest);
bool memory_check_size_limit(ci_memory_digest_t* digest);

/* Relevance scoring */
float memory_calculate_relevance(memory_item_t* item,
                                const char* current_task);
int memory_update_relevance(memory_item_t* item,
                           float new_score);
int memory_decay_relevance(ci_memory_digest_t* digest,
                          float decay_factor);

/* Memory persistence */
int memory_save_to_file(ci_memory_digest_t* digest,
                       const char* filepath);
ci_memory_digest_t* memory_load_from_file(const char* filepath,
                                         size_t context_limit);

/* Debugging and inspection */
void memory_print_summary(ci_memory_digest_t* digest);
void memory_print_item(memory_item_t* item);
int memory_validate_digest(ci_memory_digest_t* digest);

#endif /* ARGO_MEMORY_H */