/* © 2025 Casey Koons All rights reserved */

#ifndef CI_COMMANDS_H
#define CI_COMMANDS_H

/* Exit codes */
#define CI_EXIT_SUCCESS 0
#define CI_EXIT_ERROR 1

/* Command handlers */
int ci_cmd_help(int argc, char** argv);
int ci_cmd_query(int argc, char** argv);

#endif /* CI_COMMANDS_H */
