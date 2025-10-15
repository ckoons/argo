/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc_commands.h"
#include "argo_output.h"

int main(int argc, char** argv) {
    /* No arguments or just 'arc' - show general help */
    if (argc < 2) {
        return arc_cmd_help(0, NULL);
    }

    /* Dispatch to command handlers */
    if (strcmp(argv[1], "help") == 0) {
        return arc_cmd_help(argc - 2, argv + 2);
    }
    else if (strcmp(argv[1], "workflow") == 0) {
        return arc_cmd_workflow(argc - 2, argv + 2);
    }
    else {
        /* Try as workflow subcommand (e.g., 'arc status' -> 'arc workflow status') */
        /* Known workflow subcommands: start, list, status, states, attach, pause, resume, abandon, test, docs */
        const char* cmd = argv[1];
        if (strcmp(cmd, "start") == 0 || strcmp(cmd, "list") == 0 ||
            strcmp(cmd, "status") == 0 || strcmp(cmd, "states") == 0 ||
            strcmp(cmd, "attach") == 0 || strcmp(cmd, "pause") == 0 ||
            strcmp(cmd, "resume") == 0 || strcmp(cmd, "abandon") == 0 ||
            strcmp(cmd, "test") == 0 || strcmp(cmd, "docs") == 0) {
            /* Dispatch to workflow handler */
            return arc_cmd_workflow(argc - 1, argv + 1);
        }

        LOG_USER_ERROR("Unknown command: %s\n", argv[1]);
        LOG_USER_INFO("Use 'arc help' to see available commands.\n");
        return ARC_EXIT_ERROR;
    }
}
