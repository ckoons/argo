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

/* DeepSeek API Configuration */
#define DEEPSEEK_API_URL "https://api.deepseek.com/v1/chat/completions"
#define DEEPSEEK_API_KEY_ENV "DEEPSEEK_API_KEY"

/* Generate DeepSeek provider using macro */
DEFINE_OPENAI_COMPATIBLE_PROVIDER(
    DEEPSEEK,
    deepseek,
    DEEPSEEK_API_URL,
    DEEPSEEK_API_KEY_ENV,
    DEEPSEEK_DEFAULT_MODEL,
    DEEPSEEK_CONTEXT_WINDOW
)
