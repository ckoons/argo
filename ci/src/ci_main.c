/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ci_commands.h"
#include "argo_output.h"

int main(int argc, char** argv) {
    /* Check for explicit help command */
    if (argc >= 2 && strcmp(argv[1], "help") == 0) {
        return ci_cmd_help(argc - 2, argv + 2);
    }

    /* Check for explicit query command (backwards compatibility) */
    if (argc >= 2 && strcmp(argv[1], "query") == 0) {
        return ci_cmd_query(argc - 2, argv + 2);
    }

    /* GUIDELINE_APPROVED - Comment explaining CI usage patterns */
    /* Default: treat all args as query (direct CI interaction) */
    /* This handles:
       - ci "question"           (direct query)
       - ci                      (checks stdin, shows error if none)
       - echo "data" | ci        (pipe to CI)
       - echo "data" | ci "prompt" (pipe with prompt)
    */
    /* GUIDELINE_APPROVED_END */
    return ci_cmd_query(argc - 1, argv + 1);
}
