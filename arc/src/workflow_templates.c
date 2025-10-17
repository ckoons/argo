/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "arc_commands.h"
#include "arc_constants.h"
#include "arc_context.h"
#include "argo_error.h"
#include "argo_output.h"
#include "argo_limits.h"

/* Check if a directory entry is a workflow template (directory-based only) */
static int is_workflow_template(const char* base_path, const char* name) {
    char workflow_script[ARGO_PATH_MAX];
    struct stat st;

    /* Skip hidden files */
    if (name[0] == '.') {
        return 0;
    }

    /* Check if it's a directory with workflow.sh */
    snprintf(workflow_script, sizeof(workflow_script), "%s/%s/workflow.sh", base_path, name);
    if (stat(workflow_script, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
        return 1;  /* Directory-based template */
    }

    return 0;
}

/* Read description from README.md or metadata.yaml */
static void get_template_description(const char* base_path, const char* name,
                                     char* desc, size_t desc_size) {
    char readme_path[ARGO_PATH_MAX];
    char line[ARGO_BUFFER_MEDIUM];
    FILE* fp = NULL;

    /* Initialize with default */
    strncpy(desc, "No description", desc_size - 1);
    desc[desc_size - 1] = '\0';

    /* Try README.md first */
    snprintf(readme_path, sizeof(readme_path), "%s/%s/README.md", base_path, name);
    fp = fopen(readme_path, "r");
    if (fp) {
        /* Skip title and blank lines, read first description line */
        while (fgets(line, sizeof(line), fp)) {
            /* Trim whitespace */
            char* start = line;
            while (*start == ' ' || *start == '\t' || *start == '#' || *start == '\n') {
                start++;
            }
            if (strlen(start) > ARC_MIN_DESC_LEN) {  /* Minimum meaningful description */
                /* Remove trailing newline */
                char* end = start + strlen(start) - 1;
                while (end > start && (*end == '\n' || *end == '\r')) {
                    *end = '\0';
                    end--;
                }
                strncpy(desc, start, desc_size - 1);
                desc[desc_size - 1] = '\0';
                break;
            }
        }
        fclose(fp);
        return;
    }

    /* Try metadata.yaml */
    snprintf(readme_path, sizeof(readme_path), "%s/%s/metadata.yaml", base_path, name);
    fp = fopen(readme_path, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "description:")) {
                char* desc_start = strchr(line, ':');
                if (desc_start) {
                    desc_start++;
                    while (*desc_start == ' ' || *desc_start == '\t') {
                        desc_start++;
                    }
                    /* Remove trailing newline */
                    char* end = desc_start + strlen(desc_start) - 1;
                    while (end > desc_start && (*end == '\n' || *end == '\r')) {
                        *end = '\0';
                        end--;
                    }
                    strncpy(desc, desc_start, desc_size - 1);
                    desc[desc_size - 1] = '\0';
                    break;
                }
            }
        }
        fclose(fp);
    }
}

/* List templates from a directory */
static int list_templates_from_dir(const char* path, const char* source_label) {
    DIR* dir = opendir(path);
    if (!dir) {
        return 0;  /* Directory doesn't exist - not an error */
    }

    int count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        /* Skip hidden files and . .. */
        if (entry->d_name[0] == '.') {
            continue;
        }

        /* Check if it's a workflow template */
        if (is_workflow_template(path, entry->d_name)) {
            char description[ARGO_BUFFER_MEDIUM];

            /* Get description */
            get_template_description(path, entry->d_name, description, sizeof(description));

            /* Display template (directory name is the template name) */
            LOG_USER_STATUS("  %-30s %-50s [%s]\n",
                   entry->d_name, description, source_label);

            count++;
        }
    }

    closedir(dir);
    return count;
}

/* arc workflow templates command handler */
int arc_workflow_templates(int argc, char** argv) {
    (void)argc;
    (void)argv;

    LOG_USER_STATUS("\nAVAILABLE WORKFLOW TEMPLATES:\n");
    LOG_USER_STATUS("%-32s %-50s %s\n", "NAME", "DESCRIPTION", "SOURCE");
    LOG_USER_STATUS("----------------------------------------------------------------------------------------------------\n");

    int total_count = 0;

    /* List system templates (shipped with Argo) */
    char system_path[ARGO_PATH_MAX];
    snprintf(system_path, sizeof(system_path), "workflows/templates");
    int system_count = list_templates_from_dir(system_path, "system");
    total_count += system_count;

    /* List user templates (in ~/.argo/workflows/templates) */
    const char* home = getenv("HOME");
    if (home) {
        char user_path[ARGO_PATH_MAX];
        snprintf(user_path, sizeof(user_path), "%s/.argo/workflows/templates", home);
        int user_count = list_templates_from_dir(user_path, "user");
        total_count += user_count;
    }

    if (total_count == 0) {
        LOG_USER_STATUS("\nNo templates found.\n");
        LOG_USER_INFO("  Create a template with: arc workflow create\n");
    } else {
        LOG_USER_STATUS("\nTotal: %d templates\n", total_count);
        LOG_USER_INFO("\nUse 'arc workflow start <template>' to run a workflow\n");
        LOG_USER_INFO("Use 'arc workflow docs <template>' to see documentation\n");
    }

    LOG_USER_STATUS("\n");
    return ARC_EXIT_SUCCESS;
}
