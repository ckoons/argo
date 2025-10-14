/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <string.h>
#include "arc_commands.h"
#include "argo_output.h"

/* arc workflow command dispatcher */
int arc_cmd_workflow(int argc, char** argv) {
    if (argc < 1) {
        LOG_USER_ERROR("workflow subcommand required\n");
        LOG_USER_INFO("Usage: arc workflow <subcommand>\n");
        LOG_USER_INFO("Use 'arc help workflow' for details.\n");
        return ARC_EXIT_ERROR;
    }

    const char* subcommand = argv[0];

    /* Dispatch to subcommand handlers */
    if (strcmp(subcommand, "start") == 0) {
        return arc_workflow_start(argc - 1, argv + 1);
    }
    else if (strcmp(subcommand, "list") == 0) {
        return arc_workflow_list(argc - 1, argv + 1);
    }
    else if (strcmp(subcommand, "status") == 0) {
        return arc_workflow_status(argc - 1, argv + 1);
    }
    else if (strcmp(subcommand, "states") == 0) {
        return arc_workflow_states(argc - 1, argv + 1);
    }
    else if (strcmp(subcommand, "pause") == 0) {
        return arc_workflow_pause(argc - 1, argv + 1);
    }
    else if (strcmp(subcommand, "resume") == 0) {
        return arc_workflow_resume(argc - 1, argv + 1);
    }
    else if (strcmp(subcommand, "abandon") == 0) {
        return arc_workflow_abandon(argc - 1, argv + 1);
    }
    else {
        LOG_USER_ERROR("Unknown workflow subcommand: %s\n", subcommand);
        LOG_USER_INFO("Use 'arc help workflow' to see available subcommands.\n");
        return ARC_EXIT_ERROR;
    }
}
