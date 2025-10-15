/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include "arc_commands.h"
#include "argo_output.h"

/* Find tests directory for template */
static int find_tests_dir(const char* template_name, char* tests_dir, size_t tests_dir_size) {
    char template_path[512];
    const char* home = getenv("HOME");
    if (!home) {
        LOG_USER_ERROR("HOME environment variable not set\n");
        return ARC_EXIT_ERROR;
    }

    /* Try directory-based template first */
    snprintf(template_path, sizeof(template_path),
            "%s/.argo/workflows/templates/%s/tests", home, template_name);

    struct stat st;
    if (stat(template_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        strncpy(tests_dir, template_path, tests_dir_size - 1);
        tests_dir[tests_dir_size - 1] = '\0';
        return ARC_EXIT_SUCCESS;
    }

    /* No tests directory found */
    LOG_USER_ERROR("No tests directory found for template: %s\n", template_name);
    LOG_USER_INFO("  Expected: %s\n", template_path);
    return ARC_EXIT_ERROR;
}

/* Run a single test script */
static int run_test_script(const char* test_path, const char* test_name, const char* tests_dir) {
    LOG_USER_INFO("Running test: %s\n", test_name);

    pid_t pid = fork();
    if (pid < 0) {
        LOG_USER_ERROR("Failed to fork test process\n");
        return ARC_EXIT_ERROR;
    }

    if (pid == 0) {
        /* Child process - change to tests directory and run test */
        if (chdir(tests_dir) != 0) {
            LOG_USER_ERROR("Failed to change to tests directory: %s\n", tests_dir);
            exit(1);
        }
        execl("/bin/bash", "bash", test_path, NULL);
        /* If exec fails */
        LOG_USER_ERROR("Failed to execute test: %s\n", test_path);
        exit(1);
    }

    /* Parent process - wait for test */
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        LOG_USER_ERROR("Failed to wait for test process\n");
        return ARC_EXIT_ERROR;
    }

    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code == 0) {
            LOG_USER_SUCCESS("  ✓ %s passed\n", test_name);
            return ARC_EXIT_SUCCESS;
        } else {
            LOG_USER_ERROR("  ✗ %s failed (exit code %d)\n", test_name, exit_code);
            return ARC_EXIT_ERROR;
        }
    } else {
        LOG_USER_ERROR("  ✗ %s terminated abnormally\n", test_name);
        return ARC_EXIT_ERROR;
    }
}

/* Run all tests in directory */
static int run_all_tests(const char* tests_dir) {
    DIR* dir = opendir(tests_dir);
    if (!dir) {
        LOG_USER_ERROR("Failed to open tests directory: %s\n", tests_dir);
        return ARC_EXIT_ERROR;
    }

    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip hidden files and non-test files */
        if (entry->d_name[0] == '.') continue;
        if (strncmp(entry->d_name, "test_", 5) != 0) continue;
        if (!strstr(entry->d_name, ".sh")) continue;

        char test_path[512];
        snprintf(test_path, sizeof(test_path), "%s/%s", tests_dir, entry->d_name);

        /* Verify it's a regular file */
        struct stat st;
        if (stat(test_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        total_tests++;
        if (run_test_script(test_path, entry->d_name, tests_dir) == ARC_EXIT_SUCCESS) {
            passed_tests++;
        } else {
            failed_tests++;
        }
    }

    closedir(dir);

    /* Print summary */
    printf("\n");
    LOG_USER_INFO("Test Results:\n");
    LOG_USER_INFO("  Total:  %d tests\n", total_tests);
    LOG_USER_SUCCESS("  Passed: %d tests\n", passed_tests);
    if (failed_tests > 0) {
        LOG_USER_ERROR("  Failed: %d tests\n", failed_tests);
    }

    if (total_tests == 0) {
        LOG_USER_ERROR("No tests found in %s\n", tests_dir);
        return ARC_EXIT_ERROR;
    }

    return (failed_tests == 0) ? ARC_EXIT_SUCCESS : ARC_EXIT_ERROR;
}

/* arc workflow test command */
int arc_workflow_test(int argc, char** argv) {
    if (argc < 1) {
        LOG_USER_ERROR("No template specified\n");
        LOG_USER_INFO("Usage: arc workflow test <template_name> [test_name]\n");
        LOG_USER_INFO("  template_name - Name of workflow template to test\n");
        LOG_USER_INFO("  test_name     - Optional specific test to run\n");
        return ARC_EXIT_ERROR;
    }

    const char* template_name = argv[0];

    /* Find tests directory */
    char tests_dir[512];
    if (find_tests_dir(template_name, tests_dir, sizeof(tests_dir)) != ARC_EXIT_SUCCESS) {
        return ARC_EXIT_ERROR;
    }

    LOG_USER_INFO("Testing workflow template: %s\n", template_name);
    LOG_USER_INFO("Tests directory: %s\n\n", tests_dir);

    /* Run specific test or all tests */
    if (argc >= 2) {
        const char* test_name = argv[1];
        char test_path[512];

        /* Add .sh extension if not present */
        if (strstr(test_name, ".sh")) {
            snprintf(test_path, sizeof(test_path), "%s/%s", tests_dir, test_name);
        } else {
            snprintf(test_path, sizeof(test_path), "%s/%s.sh", tests_dir, test_name);
        }

        /* Verify test exists */
        struct stat st;
        if (stat(test_path, &st) != 0) {
            LOG_USER_ERROR("Test not found: %s\n", test_name);
            return ARC_EXIT_ERROR;
        }

        return run_test_script(test_path, test_name, tests_dir);
    } else {
        return run_all_tests(tests_dir);
    }
}
