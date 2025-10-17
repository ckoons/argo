/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "arc_commands.h"
#include "argo_output.h"

/* Find README for template */
static int find_readme(const char* template_name, char* readme_path, size_t readme_path_size) {
    const char* home = getenv("HOME");
    if (!home) {
        LOG_USER_ERROR("HOME environment variable not set\n");
        return ARC_EXIT_ERROR;
    }

    /* Try directory-based template */
    snprintf(readme_path, readme_path_size,
            "%s/.argo/workflows/templates/%s/README.md", home, template_name);

    struct stat st;
    if (stat(readme_path, &st) == 0 && S_ISREG(st.st_mode)) {
        return ARC_EXIT_SUCCESS;
    }

    /* No README found */
    LOG_USER_ERROR("No documentation found for template: %s\n", template_name);
    LOG_USER_INFO("  Expected: %s\n", readme_path);
    return ARC_EXIT_ERROR;
}

/* Display README content */
static void display_readme(const char* readme_path) {
    FILE* fp = fopen(readme_path, "r");
    if (!fp) {
        LOG_USER_ERROR("Failed to open README: %s\n", readme_path);
        return;
    }

    char line[ARC_LINE_BUFFER];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }

    fclose(fp);
}

/* Display metadata summary */
static void display_metadata(const char* template_name) {
    const char* home = getenv("HOME");
    if (!home) return;

    char metadata_path[ARC_PATH_BUFFER];
    snprintf(metadata_path, sizeof(metadata_path),
            "%s/.argo/workflows/templates/%s/metadata.yaml", home, template_name);

    struct stat st;
    if (stat(metadata_path, &st) != 0) {
        /* No metadata file */
        return;
    }

    printf("\n");
    LOG_USER_INFO("Template Metadata:\n");
    printf("---\n");

    FILE* fp = fopen(metadata_path, "r");
    if (!fp) return;

    char line[ARC_LINE_BUFFER];
    while (fgets(line, sizeof(line), fp)) {
        /* Skip comments and empty lines */
        char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '#' || *trimmed == '\0' || *trimmed == '\n') continue;

        printf("%s", line);
    }

    fclose(fp);
    printf("---\n");
}

/* arc workflow docs command */
int arc_workflow_docs(int argc, char** argv) {
    if (argc < 1) {
        LOG_USER_ERROR("No template specified\n");
        LOG_USER_INFO("Usage: arc workflow docs <template_name>\n");
        LOG_USER_INFO("  template_name - Name of workflow template\n");
        return ARC_EXIT_ERROR;
    }

    const char* template_name = argv[0];

    /* Find README */
    char readme_path[ARC_PATH_BUFFER];
    if (find_readme(template_name, readme_path, sizeof(readme_path)) != ARC_EXIT_SUCCESS) {
        return ARC_EXIT_ERROR;
    }

    /* Display documentation */
    LOG_USER_INFO("Documentation for workflow template: %s\n\n", template_name);
    display_readme(readme_path);

    /* Display metadata if available */
    display_metadata(template_name);

    return ARC_EXIT_SUCCESS;
}
