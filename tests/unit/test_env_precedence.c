/* © 2025 Casey Koons All rights reserved */

/* Test environment file loading precedence */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "argo_env_utils.h"
#include "argo_init.h"

int test_count = 0;
int test_passed = 0;

#define TEST(name) do { \
    test_count++; \
    printf("Testing: %-50s", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    test_passed++; \
    printf(" ✓\n"); \
} while(0)

#define FAIL() do { \
    printf(" ✗\n"); \
} while(0)

static void create_test_env_files(void) {
    char home_env[256];
    char home_argorc[256];
    const char* home = getenv("HOME");

    if (home) {
        /* Create ~/.env */
        snprintf(home_env, sizeof(home_env), "%s/.env", home);
        FILE* fp = fopen(home_env, "w");
        if (fp) {
            fprintf(fp, "TEST_VAR_HOME=from_home_env\n");
            fprintf(fp, "TEST_OVERRIDE=home_env\n");
            fclose(fp);
        }

        /* Create ~/.argorc */
        snprintf(home_argorc, sizeof(home_argorc), "%s/.argorc", home);
        fp = fopen(home_argorc, "w");
        if (fp) {
            fprintf(fp, "TEST_VAR_ARGORC=from_argorc\n");
            fprintf(fp, "TEST_OVERRIDE=argorc\n");
            fclose(fp);
        }
    }

    /* .env.argo created by default in current dir */
}

static void cleanup_test_env_files(void) {
    char home_env[256];
    char home_argorc[256];
    const char* home = getenv("HOME");

    if (home) {
        snprintf(home_env, sizeof(home_env), "%s/.env", home);
        snprintf(home_argorc, sizeof(home_argorc), "%s/.argorc", home);
        unlink(home_env);
        unlink(home_argorc);
    }
}

static void test_load_order(void) {
    TEST("Environment files loaded in correct order");

    create_test_env_files();

    argo_init();

    /* .argorc should override ~/.env */
    const char* override = argo_getenv("TEST_OVERRIDE");
    if (override && strcmp(override, "argorc") == 0) {
        PASS();
    } else {
        printf("(got '%s', expected 'argorc') ", override ? override : "NULL");
        FAIL();
    }

    argo_exit();
    cleanup_test_env_files();
}

static void test_argorc_loaded(void) {
    TEST("~/.argorc file is loaded");

    create_test_env_files();

    argo_init();

    const char* argorc_var = argo_getenv("TEST_VAR_ARGORC");
    if (argorc_var && strcmp(argorc_var, "from_argorc") == 0) {
        PASS();
    } else {
        FAIL();
    }

    argo_exit();
    cleanup_test_env_files();
}

static void test_home_env_loaded(void) {
    TEST("~/.env file is loaded");

    create_test_env_files();

    argo_init();

    const char* home_var = argo_getenv("TEST_VAR_HOME");
    if (home_var && strcmp(home_var, "from_home_env") == 0) {
        PASS();
    } else {
        FAIL();
    }

    argo_exit();
    cleanup_test_env_files();
}

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("ARGO ENVIRONMENT PRECEDENCE TESTS\n");
    printf("========================================\n");
    printf("\n");

    test_home_env_loaded();
    test_argorc_loaded();
    test_load_order();

    printf("\n");
    printf("========================================\n");
    printf("Tests run:    %d\n", test_count);
    printf("Tests passed: %d\n", test_passed);
    printf("Tests failed: %d\n", test_count - test_passed);
    printf("========================================\n");

    return (test_count == test_passed) ? 0 : 1;
}
