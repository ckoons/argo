/* © 2025 Casey Koons All rights reserved */

#ifndef ARC_COMMANDS_H
#define ARC_COMMANDS_H

/* Exit codes */
#define ARC_EXIT_SUCCESS 0
#define ARC_EXIT_ERROR 1

/* Command handlers */
int arc_cmd_help(int argc, char** argv);
int arc_cmd_switch(int argc, char** argv);
int arc_cmd_workflow(int argc, char** argv);

/* Workflow subcommands */
int arc_workflow_start(int argc, char** argv);
int arc_workflow_list(int argc, char** argv);
int arc_workflow_status(int argc, char** argv);
int arc_workflow_pause(int argc, char** argv);
int arc_workflow_resume(int argc, char** argv);
int arc_workflow_abandon(int argc, char** argv);

#endif /* ARC_COMMANDS_H */
