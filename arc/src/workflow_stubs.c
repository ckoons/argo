/* © 2025 Casey Koons All rights reserved */

/* Stub implementations for Phase 2 commands */

#include <stdio.h>
#include "arc_commands.h"

int arc_workflow_start(int argc, char** argv) {
    (void)argc;
    (void)argv;
    fprintf(stderr, "Error: 'arc workflow start' not yet implemented\n");
    return ARC_EXIT_ERROR;
}

int arc_workflow_status(int argc, char** argv) {
    (void)argc;
    (void)argv;
    fprintf(stderr, "Error: 'arc workflow status' not yet implemented\n");
    return ARC_EXIT_ERROR;
}

int arc_workflow_pause(int argc, char** argv) {
    (void)argc;
    (void)argv;
    fprintf(stderr, "Error: 'arc workflow pause' not yet implemented\n");
    return ARC_EXIT_ERROR;
}

int arc_workflow_resume(int argc, char** argv) {
    (void)argc;
    (void)argv;
    fprintf(stderr, "Error: 'arc workflow resume' not yet implemented\n");
    return ARC_EXIT_ERROR;
}

int arc_workflow_abandon(int argc, char** argv) {
    (void)argc;
    (void)argv;
    fprintf(stderr, "Error: 'arc workflow abandon' not yet implemented\n");
    return ARC_EXIT_ERROR;
}
