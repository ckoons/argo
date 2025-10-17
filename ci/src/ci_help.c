/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <string.h>
#include "ci_commands.h"
#include "argo_output.h"

/* General help */
static void show_general_help(void) {
    printf("ci - Companion Intelligence\n\n");
    printf("Direct interface to AI providers from workflows and command line.\n\n");
    printf("Usage:\n");
    printf("  ci \"<prompt>\"                  Ask AI a question (simple!)\n");
    printf("  echo <text> | ci \"<prompt>\"    Process piped input\n");
    printf("  ci help [command]              Show help\n\n");
    printf("Examples:\n");
    printf("  ci \"how do I translate text to klingon in bash?\"\n");
    printf("  echo \"translate this\" | ci \"to klingon\"\n");
    printf("  cat file.txt | ci \"summarize this\"\n");
    printf("  tail -f app.log | ci \"watch for errors\"\n\n");
    printf("Options:\n");
    printf("  --provider <name>     Select AI provider (default: claude_code)\n");
    printf("  --model <model>       Select model (default: provider's default)\n\n");
    printf("Available Providers:\n");
    printf("  claude_code          Claude Code CLI (default, no API key)\n");
    printf("  claude_api           Claude API (requires ANTHROPIC_API_KEY)\n");
    printf("  openai_api           OpenAI API (requires OPENAI_API_KEY)\n");
    printf("  gemini_api           Google Gemini (requires GEMINI_API_KEY)\n");
    printf("  grok_api             xAI Grok (requires GROK_API_KEY)\n");
    printf("  deepseek_api         DeepSeek (requires DEEPSEEK_API_KEY)\n");
    printf("  openrouter           OpenRouter (requires OPENROUTER_API_KEY)\n");
    printf("  ollama               Ollama local (requires ollama server)\n\n");
    printf("Prerequisites:\n");
    printf("  Daemon must be running: argo-daemon --port 9876\n\n");
    printf("Note: 'ci query' syntax still works for backwards compatibility.\n");
}

/* Help for specific commands */
static void show_command_help(const char* command) {
    if (strcmp(command, "query") == 0) {
        printf("ci \"<prompt>\" - Ask AI a question\n\n");
        printf("Direct interface to Companion Intelligence.\n\n");
        printf("Arguments:\n");
        printf("  prompt    - Question or prompt for AI\n\n");
        printf("Input Methods:\n");
        printf("  1. Direct:  ci \"your question\"\n");
        printf("  2. Piped:   echo \"data\" | ci\n");
        printf("  3. Both:    echo \"data\" | ci \"analyze this\"\n\n");
        printf("Options:\n");
        printf("  --provider <name>   AI provider to use\n");
        printf("  --model <model>     Specific model\n\n");
        printf("Examples:\n");
        printf("  ci \"explain grep command\"\n");
        printf("  cat errors.log | ci \"what's causing this?\"\n");
        printf("  ci \"write a bash function\" --provider ollama\n");
        printf("  tail -f app.log | ci \"monitor for errors\"\n\n");
        printf("Note: 'ci query' syntax also works for backwards compatibility.\n");
    }
    else {
        LOG_USER_ERROR("Unknown command: %s\n", command);
        LOG_USER_INFO("Use 'ci help' to see available commands.\n");
    }
}

/* ci help command handler */
int ci_cmd_help(int argc, char** argv) {
    if (argc < 1) {
        /* No specific command - show general help */
        show_general_help();
        return CI_EXIT_SUCCESS;
    }

    /* Show help for specific command */
    show_command_help(argv[0]);
    return CI_EXIT_SUCCESS;
}
