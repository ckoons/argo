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
    else if (strcmp(argv[1], "switch") == 0) {
        return arc_cmd_switch(argc - 2, argv + 2);
    }
    else if (strcmp(argv[1], "workflow") == 0) {
        return arc_cmd_workflow(argc - 2, argv + 2);
    }
    else {
        LOG_USER_ERROR("Unknown command: %s\n", argv[1]);
        LOG_USER_INFO("Use 'arc help' to see available commands.\n");
        return ARC_EXIT_ERROR;
    }
}
