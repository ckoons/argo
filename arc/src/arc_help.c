/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <string.h>
#include "arc_commands.h"
#include "argo_output.h"

/* General help */
static void show_general_help(void) {
    printf("arc - Argo Workflow CLI\n\n");
    printf("Terminal-facing client for Argo daemon. Communicates via HTTP API.\n\n");
    printf("Usage:\n");
    printf("  arc help [command]          Show help for a specific command\n");
    printf("  arc switch [workflow_name]  Set active workflow context\n");
    printf("  arc workflow <subcommand>   Manage workflows\n\n");
    printf("Workflow Subcommands:\n");
    printf("  start [template] [instance]     Create and start workflow\n");
    printf("  list [active|template]          List workflows or templates\n");
    printf("  status [workflow_name]          Show workflow status\n");
    printf("  states                          Show all workflow states\n");
    printf("  attach [workflow_name]          Attach to workflow output\n");
    printf("  pause [workflow_name]           Pause workflow at next checkpoint\n");
    printf("  resume [workflow_name]          Resume paused workflow\n");
    printf("  abandon [workflow_name]         Stop and remove workflow\n\n");
    printf("Environment Filtering:\n");
    printf("  --env <env>                     Filter/create in specific environment\n");
    printf("  ARC_ENV=<env>                   Set default environment for terminal\n");
    printf("  Environments: test, dev, stage, prod (default: dev)\n\n");
    printf("Prerequisites:\n");
    printf("  Start daemon first: argo-daemon --port 9876\n\n");
    printf("For more details: arc help <command>\n");
}

/* Help for specific commands */
static void show_command_help(const char* command) {
    if (strcmp(command, "switch") == 0) {
        printf("arc switch [workflow_name]\n\n");
        printf("Set the active workflow context for the current terminal.\n\n");
        printf("Arguments:\n");
        printf("  workflow_name  - Full workflow ID (template_instance)\n\n");
        printf("Example:\n");
        printf("  arc switch create_proposal_my_feature\n");
    }
    else if (strcmp(command, "workflow") == 0) {
        printf("arc workflow <subcommand>\n\n");
        printf("Manage Argo workflows.\n\n");
        printf("Subcommands:\n");
        printf("  start [template] [instance]     Create and start workflow\n");
        printf("  list [active|template]          List workflows or templates\n");
        printf("  status [workflow_name]          Show workflow status\n");
        printf("  states                          Show all workflow states\n");
        printf("  attach [workflow_name]          Attach to workflow output\n");
        printf("  pause [workflow_name]           Pause workflow at next checkpoint\n");
        printf("  resume [workflow_name]          Resume paused workflow\n");
        printf("  abandon [workflow_name]         Stop and remove workflow\n\n");
        printf("Environment Filtering:\n");
        printf("  Most commands support --env <env> to filter by environment\n");
        printf("  or set ARC_ENV environment variable for terminal-wide filtering\n\n");
        printf("Use 'arc help workflow <subcommand>' for details on each.\n");
    }
    else if (strcmp(command, "start") == 0 ||
             (strstr(command, "workflow") && strstr(command, "start"))) {
        printf("arc workflow start [template] [instance] [branch] [--env <env>]\n\n");
        printf("Creates a new workflow instance and starts background execution.\n\n");
        printf("Arguments:\n");
        printf("  template  - Workflow template name (from system or user templates)\n");
        printf("  instance  - Unique instance identifier\n");
        printf("  branch    - Optional git branch (default: main)\n");
        printf("  --env     - Optional environment (default: dev or ARC_ENV)\n\n");
        printf("Workflow name format: template_instance\n\n");
        printf("Examples:\n");
        printf("  arc workflow start create_proposal my_feature\n");
        printf("  arc workflow start create_proposal my_feature --env test\n");
        printf("  ARC_ENV=prod arc workflow start deploy release_v1\n");
    }
    else if (strcmp(command, "list") == 0 ||
             (strstr(command, "workflow") && strstr(command, "list"))) {
        printf("arc workflow list [active|template] [--env <env>]\n\n");
        printf("List workflows or templates.\n\n");
        printf("Options:\n");
        printf("  (no args)  - Show all active workflows and available templates\n");
        printf("  active     - Show only active workflows\n");
        printf("  template   - Show only available templates\n");
        printf("  --env      - Filter by environment (or use ARC_ENV)\n\n");
        printf("Examples:\n");
        printf("  arc workflow list\n");
        printf("  arc workflow list active\n");
        printf("  arc workflow list --env test\n");
        printf("  ARC_ENV=prod arc workflow list active\n");
    }
    else if (strcmp(command, "status") == 0 ||
             (strstr(command, "workflow") && strstr(command, "status"))) {
        printf("arc workflow status [workflow_name]\n\n");
        printf("Show workflow status.\n\n");
        printf("Arguments:\n");
        printf("  workflow_name  - Optional. If omitted, shows all active workflows\n\n");
        printf("Example:\n");
        printf("  arc workflow status\n");
        printf("  arc workflow status create_proposal_my_feature\n");
    }
    else {
        LOG_USER_ERROR("Unknown command: %s\n", command);
        LOG_USER_INFO("Use 'arc help' to see available commands.\n");
    }
}

/* arc help command handler */
int arc_cmd_help(int argc, char** argv) {
    if (argc < 2) {
        /* No specific command - show general help */
        show_general_help();
        return ARC_EXIT_SUCCESS;
    }

    /* Show help for specific command */
    show_command_help(argv[1]);
    return ARC_EXIT_SUCCESS;
}
