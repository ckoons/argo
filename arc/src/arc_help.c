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
    printf("  arc <command> [args...]     Execute command\n");
    printf("  arc help [command]          Show help for command\n\n");
    printf("Common Commands:\n");
    printf("  start <template> [instance] [args...]  Start workflow from template\n");
    printf("  list                                    List active workflows\n");
    printf("  templates                               List available templates\n");
    printf("  status <workflow_id>                    Show workflow status\n");
    printf("  states                                  Show detailed status of all workflows\n");
    printf("  abandon <workflow_id>                   Stop and remove workflow\n");
    printf("  pause <workflow_id>                     Pause workflow execution\n");
    printf("  resume <workflow_id>                    Resume paused workflow\n");
    printf("  attach <workflow_id>                    Attach to workflow logs\n");
    printf("  docs <template>                         Show template documentation\n");
    printf("  test <template>                         Run template tests\n\n");
    printf("Templates:\n");
    printf("  System templates: workflows/templates/\n");
    printf("  User templates:   ~/.argo/workflows/templates/\n\n");
    printf("Prerequisites:\n");
    printf("  Daemon must be running: argo-daemon --port ARC_DEFAULT_DAEMON_PORT\n\n");
    printf("Template Management:\n");
    printf("  To create templates:   arc start create_template <name>\n");
    printf("  To remove templates:   rm -rf workflows/templates/<name>\n");
    printf("  To rename templates:   mv workflows/templates/<old> <new>\n");
    printf("  Helper functions:      source ~/.local/share/arc/helpers/arc-helpers.sh\n\n");
    printf("Examples:\n");
    printf("  arc templates                      # List available templates\n");
    printf("  arc start create_template my_flow  # Create new template (guided)\n");
    printf("  arc start my_flow test             # Start with instance name (my_flow_test)\n");
    printf("  arc list                           # Show running workflows\n");
    printf("  arc states                         # Show detailed status of all\n");
    printf("  arc status my_flow_test            # Check workflow status\n\n");
    printf("For command details: arc help <command>\n");
}

/* Help for specific commands */
static void show_command_help(const char* command) {
    if (strcmp(command, "start") == 0) {
        printf("arc start <template> [instance] [args...] [KEY=VALUE...]\n\n");
        printf("Start a workflow from a template.\n\n");
        printf("Arguments:\n");
        printf("  template   - Template name (from system or user templates)\n");
        printf("  instance   - Optional instance name (default: auto-numbered _01, _02, etc.)\n");
        printf("  args...    - Arguments passed to workflow script\n");
        printf("  KEY=VALUE  - Environment variables for workflow\n\n");
        printf("Instance Naming:\n");
        printf("  - With instance:  arc start template instance → template_instance\n");
        printf("  - Auto-numbered:  arc start template          → template_01, template_02, ...\n");
        printf("  - Auto-numbering finds highest _NN suffix and increments\n\n");
        printf("Templates are directory-based with this structure:\n");
        printf("  template_name/\n");
        printf("    workflow.sh     - Main executable script\n");
        printf("    README.md       - Documentation\n");
        printf("    metadata.yaml   - Template metadata\n");
        printf("    tests/          - Test scripts\n");
        printf("    config/         - Configuration files\n\n");
        printf("Examples:\n");
        printf("  arc start create_template my_flow            # Create new template (guided)\n");
        printf("  arc start my_flow feature                    # Named: my_flow_feature\n");
        printf("  arc start my_build arg1 arg2                 # With args\n");
        printf("  arc start deploy prod VERSION=1.2.3         # Instance + env vars\n\n");
        printf("See also: arc templates, arc docs <template>\n");
    }
    else if (strcmp(command, "templates") == 0) {
        printf("arc templates\n\n");
        printf("List all available workflow templates.\n\n");
        printf("Shows templates from:\n");
        printf("  - System: workflows/templates/ (shipped with Argo)\n");
        printf("  - User:   ~/.argo/workflows/templates/ (your templates)\n\n");
        printf("Each template includes:\n");
        printf("  - Name and description\n");
        printf("  - Source location (system or user)\n\n");
        printf("Examples:\n");
        printf("  arc templates\n");
        printf("  arc start <template_name>\n");
        printf("  arc docs <template_name>\n");
    }
    else if (strcmp(command, "list") == 0) {
        printf("arc list\n\n");
        printf("List active workflows.\n\n");
        printf("Shows:\n");
        printf("  - Workflow ID\n");
        printf("  - Script path\n");
        printf("  - State (running, paused, completed, failed, abandoned)\n");
        printf("  - Process ID\n\n");
        printf("Examples:\n");
        printf("  arc list\n");
        printf("  arc status <workflow_id>\n");
        printf("  arc abandon <workflow_id>\n");
    }
    else if (strcmp(command, "status") == 0) {
        printf("arc status <workflow_id>\n\n");
        printf("Show detailed workflow status.\n\n");
        printf("Arguments:\n");
        printf("  workflow_id  - Workflow ID (from 'arc list')\n\n");
        printf("Shows:\n");
        printf("  - Workflow ID and script\n");
        printf("  - Current state\n");
        printf("  - Process ID\n");
        printf("  - Start/end times\n");
        printf("  - Exit code (if completed)\n\n");
        printf("Examples:\n");
        printf("  arc status wf_12345_67890\n");
        printf("  arc attach wf_12345_67890  # View logs\n");
    }
    else if (strcmp(command, "states") == 0) {
        printf("arc states\n\n");
        printf("Show detailed status of all active workflows.\n\n");
        printf("Shows comprehensive information for every workflow:\n");
        printf("  - Workflow ID (with * marking current workflow)\n");
        printf("  - Template and instance names\n");
        printf("  - Branch and environment\n");
        printf("  - Status (running, paused, completed, failed, abandoned)\n");
        printf("  - Process status (RUNNING or STOPPED)\n");
        printf("  - Process ID\n");
        printf("  - Log file location\n\n");
        printf("This is more detailed than 'arc list', showing:\n");
        printf("  - Process state (live check via kill -0)\n");
        printf("  - Template/instance breakdown\n");
        printf("  - Log file paths for debugging\n\n");
        printf("Examples:\n");
        printf("  arc states                    # Show all workflow states\n");
        printf("  arc list                      # Simpler workflow list\n");
        printf("  arc status <workflow_id>      # Single workflow details\n");
    }
    else if (strcmp(command, "pause") == 0) {
        printf("arc pause <workflow_id>\n\n");
        printf("Pause a running workflow.\n\n");
        printf("Sends SIGSTOP to the workflow process, suspending execution.\n");
        printf("Use 'arc resume' to continue.\n\n");
        printf("Examples:\n");
        printf("  arc pause wf_12345_67890\n");
        printf("  arc resume wf_12345_67890\n");
    }
    else if (strcmp(command, "resume") == 0) {
        printf("arc resume <workflow_id>\n\n");
        printf("Resume a paused workflow.\n\n");
        printf("Sends SIGCONT to the workflow process, continuing execution.\n\n");
        printf("Examples:\n");
        printf("  arc resume wf_12345_67890\n");
    }
    else if (strcmp(command, "abandon") == 0) {
        printf("arc abandon <workflow_id>\n\n");
        printf("Stop and remove a workflow.\n\n");
        printf("Sends SIGTERM to the workflow process and marks it as abandoned.\n\n");
        printf("Arguments:\n");
        printf("  workflow_id  - Workflow ID (from 'arc list')\n\n");
        printf("Examples:\n");
        printf("  arc abandon wf_12345_67890\n");
    }
    else if (strcmp(command, "attach") == 0) {
        printf("arc attach <workflow_id>\n\n");
        printf("Attach to workflow logs (tail -f).\n\n");
        printf("Shows real-time log output from the workflow.\n");
        printf("Press Ctrl-C to detach (workflow keeps running).\n\n");
        printf("Arguments:\n");
        printf("  workflow_id  - Workflow ID (from 'arc list')\n\n");
        printf("Examples:\n");
        printf("  arc attach wf_12345_67890\n");
    }
    else if (strcmp(command, "docs") == 0) {
        printf("arc docs <template>\n\n");
        printf("Show documentation for a template.\n\n");
        printf("Displays the README.md from the template directory.\n\n");
        printf("Arguments:\n");
        printf("  template  - Template name\n\n");
        printf("Examples:\n");
        printf("  arc docs create_template\n");
        printf("  arc templates  # List available templates\n");
    }
    else if (strcmp(command, "test") == 0) {
        printf("arc test <template>\n\n");
        printf("Run tests for a workflow template.\n\n");
        printf("Executes test scripts from the template's tests/ directory.\n\n");
        printf("Arguments:\n");
        printf("  template  - Template name\n\n");
        printf("Examples:\n");
        printf("  arc test create_template\n");
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
