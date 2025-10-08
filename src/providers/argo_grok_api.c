/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Project includes */
#include "argo_ci.h"
#include "argo_api_providers.h"
#include "argo_api_keys.h"
#include "argo_api_common.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_limits.h"
#include "argo_api_provider_generator.h"

/* Grok API Configuration */
#define GROK_API_KEY_ENV "XAI_API_KEY"

/* Generate Grok provider using macro */
DEFINE_OPENAI_COMPATIBLE_PROVIDER(
    GROK,
    grok,
    GROK_API_URL,
    GROK_API_KEY_ENV,
    GROK_DEFAULT_MODEL,
    GROK_CONTEXT_WINDOW
)
