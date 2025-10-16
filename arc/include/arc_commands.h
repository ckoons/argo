/* © 2025 Casey Koons All rights reserved */

#ifndef ARC_COMMANDS_H
#define ARC_COMMANDS_H

/* Exit codes */
#define ARC_EXIT_SUCCESS 0
#define ARC_EXIT_ERROR 1

/* Environment utilities */
const char* arc_get_effective_environment(int argc, char** argv);
const char* arc_get_environment_for_creation(int argc, char** argv);

/* Command handlers */
int arc_cmd_help(int argc, char** argv);
int arc_cmd_switch(int argc, char** argv);
int arc_cmd_workflow(int argc, char** argv);

/* Workflow subcommands */
int arc_workflow_start(int argc, char** argv);
int arc_workflow_list(int argc, char** argv);
int arc_workflow_templates(int argc, char** argv);
int arc_workflow_status(int argc, char** argv);
int arc_workflow_states(int argc, char** argv);
int arc_workflow_attach(int argc, char** argv);
int arc_workflow_attach_auto(const char* workflow_id);  /* Auto-attach from start */
int arc_workflow_pause(int argc, char** argv);
int arc_workflow_resume(int argc, char** argv);
int arc_workflow_abandon(int argc, char** argv);
int arc_workflow_test(int argc, char** argv);
int arc_workflow_docs(int argc, char** argv);

#endif /* ARC_COMMANDS_H */
