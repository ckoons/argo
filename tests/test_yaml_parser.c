/* © 2025 Casey Koons All rights reserved */
/* Test YAML parser functionality */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "argo_yaml.h"
#include "argo_error.h"

/* Test simple key-value parsing */
static void test_simple_yaml(void) {
    const char* yaml =
        "name: test_workflow\n"
        "description: A test workflow\n"
        "version: 1.0.0\n"
        "author: Casey\n";

    char name[256] = {0};
    char desc[256] = {0};
    char version[256] = {0};
    char author[256] = {0};

    int result = yaml_get_value(yaml, "name", name, sizeof(name));
    assert(result == ARGO_SUCCESS);
    assert(strcmp(name, "test_workflow") == 0);

    result = yaml_get_value(yaml, "description", desc, sizeof(desc));
    assert(result == ARGO_SUCCESS);
    assert(strcmp(desc, "A test workflow") == 0);

    result = yaml_get_value(yaml, "version", version, sizeof(version));
    assert(result == ARGO_SUCCESS);
    assert(strcmp(version, "1.0.0") == 0);

    result = yaml_get_value(yaml, "author", author, sizeof(author));
    assert(result == ARGO_SUCCESS);
    assert(strcmp(author, "Casey") == 0);

    printf("✓ Simple YAML parsing works\n");
}

/* Test comments and whitespace */
static void test_yaml_comments(void) {
    const char* yaml =
        "# This is a comment\n"
        "name: test  # inline comment\n"
        "  description: spaced value  \n"
        "\n"
        "version: 1.0\n";

    char name[256] = {0};
    char desc[256] = {0};
    char version[256] = {0};

    int result = yaml_get_value(yaml, "name", name, sizeof(name));
    assert(result == ARGO_SUCCESS);
    assert(strcmp(name, "test") == 0);

    result = yaml_get_value(yaml, "description", desc, sizeof(desc));
    assert(result == ARGO_SUCCESS);
    assert(strcmp(desc, "spaced value") == 0);

    result = yaml_get_value(yaml, "version", version, sizeof(version));
    assert(result == ARGO_SUCCESS);
    assert(strcmp(version, "1.0") == 0);

    printf("✓ YAML comments and whitespace handled correctly\n");
}

/* Test quoted values */
static void test_yaml_quotes(void) {
    const char* yaml =
        "name: \"quoted value\"\n"
        "description: 'single quoted'\n"
        "path: /some/path\n";

    char name[256] = {0};
    char desc[256] = {0};
    char path[256] = {0};

    int result = yaml_get_value(yaml, "name", name, sizeof(name));
    assert(result == ARGO_SUCCESS);
    assert(strcmp(name, "quoted value") == 0);

    result = yaml_get_value(yaml, "description", desc, sizeof(desc));
    assert(result == ARGO_SUCCESS);
    assert(strcmp(desc, "single quoted") == 0);

    result = yaml_get_value(yaml, "path", path, sizeof(path));
    assert(result == ARGO_SUCCESS);
    assert(strcmp(path, "/some/path") == 0);

    printf("✓ YAML quoted values handled correctly\n");
}

/* Test actual create_workflow metadata */
static void test_create_workflow_metadata(void) {
    const char* path = "workflows/system/create_workflow/metadata.yaml";

    char name[256] = {0};
    char desc[256] = {0};
    char version[256] = {0};

    /* Read the file and parse values */
    FILE* f = fopen(path, "r");
    if (!f) {
        printf("Warning: Could not open %s (may not exist yet)\n", path);
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        printf("Warning: Memory allocation failed\n");
        return;
    }

    size_t bytes_read = fread(content, 1, size, f);
    content[bytes_read] = '\0';
    fclose(f);

    int result = yaml_get_value(content, "name", name, sizeof(name));
    assert(result == ARGO_SUCCESS);
    assert(strcmp(name, "create_workflow") == 0);

    result = yaml_get_value(content, "description", desc, sizeof(desc));
    assert(result == ARGO_SUCCESS);
    assert(strstr(desc, "Meta-workflow") != NULL);

    result = yaml_get_value(content, "version", version, sizeof(version));
    assert(result == ARGO_SUCCESS);
    assert(strcmp(version, "1.0.0") == 0);

    free(content);
    printf("✓ create_workflow metadata parsed correctly\n");
}

int main(void) {
    printf("Testing YAML parser...\n\n");

    test_simple_yaml();
    test_yaml_comments();
    test_yaml_quotes();
    test_create_workflow_metadata();

    printf("\nAll YAML parser tests passed!\n");
    return 0;
}
