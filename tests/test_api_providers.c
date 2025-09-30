/* © 2025 Casey Koons All rights reserved */

/* Test API provider availability - does NOT make actual API calls */

#include <stdio.h>
#include <stdlib.h>
#include "argo_api_providers.h"

int main(void) {
    printf("\nArgo API Provider Availability Test\n");
    printf("====================================\n");
    printf("(This only checks if API keys are configured)\n\n");

    printf("Provider Status:\n");
    printf("  Claude API:    %s\n", claude_api_is_available() ? "✓ Available" : "✗ Not configured");
    printf("  OpenAI:        %s\n", openai_api_is_available() ? "✓ Available" : "✗ Not configured");
    printf("  Gemini:        %s\n", gemini_api_is_available() ? "✓ Available" : "✗ Not configured");
    printf("  Grok:          %s\n", grok_api_is_available() ? "✓ Available" : "✗ Not configured");
    printf("  DeepSeek:      %s\n", deepseek_api_is_available() ? "✓ Available" : "✗ Not configured");
    printf("  OpenRouter:    %s\n", openrouter_is_available() ? "✓ Available" : "✗ Not configured");

    printf("\n");
    printf("To test actual API calls, use test_api_calls\n");
    printf("WARNING: API calls will incur costs!\n\n");

    return 0;
}