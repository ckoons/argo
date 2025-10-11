/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_MOCK_H
#define ARGO_MOCK_H

#include "argo_ci.h"

/* Mock provider for testing workflows without real AI */

/* Mock provider constants */
#define MOCK_MODEL_SIZE 256
#define MOCK_CONTEXT_WINDOW 100000  /* Arbitrary large number for testing */

/* Create mock provider with default responses */
ci_provider_t* mock_provider_create(const char* model);

/* Set custom response for next query */
int mock_provider_set_response(ci_provider_t* provider, const char* response);

/* Set response pattern (will cycle through responses) */
int mock_provider_set_responses(ci_provider_t* provider, const char** responses, int count);

/* Get query history for verification */
const char* mock_provider_get_last_prompt(ci_provider_t* provider);
int mock_provider_get_query_count(ci_provider_t* provider);

#endif /* ARGO_MOCK_H */
