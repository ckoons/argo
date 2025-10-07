/* © 2025 Casey Koons All rights reserved */

/* Session management test - verify session lifecycle and state management */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/argo_session.h"
#include "../include/argo_error.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        printf("Testing: %s ... ", name); \
        tests_run++; \
    } while(0)

#define PASS() \
    do { \
        printf("✓\n"); \
        tests_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        printf("✗ (%s)\n", msg); \
        tests_failed++; \
    } while(0)

/* Test session creation */
static void test_session_creation(void) {
    TEST("Session creation");

    argo_session_t* session = session_create("test-session", "TestProject", "main");
    if (!session) {
        FAIL("Failed to create session");
        return;
    }

    if (strcmp(session->id, "test-session") != 0) {
        FAIL("Session ID mismatch");
        session_destroy(session);
        return;
    }

    if (strcmp(session->project_name, "TestProject") != 0) {
        FAIL("Project name mismatch");
        session_destroy(session);
        return;
    }

    if (session->status != SESSION_STATUS_CREATED) {
        FAIL("Initial status should be CREATED");
        session_destroy(session);
        return;
    }

    if (!session->orchestrator || !session->registry || !session->memory) {
        FAIL("Session components not initialized");
        session_destroy(session);
        return;
    }

    session_destroy(session);
    PASS();
}

/* Test session lifecycle transitions */
static void test_session_lifecycle(void) {
    TEST("Session lifecycle transitions");

    argo_session_t* session = session_create("lifecycle-test", "TestProject", "main");
    if (!session) {
        FAIL("Failed to create session");
        return;
    }

    /* Start session */
    int result = session_start(session);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to start session");
        session_destroy(session);
        return;
    }

    if (session->status != SESSION_STATUS_ACTIVE) {
        FAIL("Status should be ACTIVE after start");
        session_destroy(session);
        return;
    }

    /* Pause session */
    result = session_pause(session);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to pause session");
        session_destroy(session);
        return;
    }

    if (session->status != SESSION_STATUS_PAUSED) {
        FAIL("Status should be PAUSED after pause");
        session_destroy(session);
        return;
    }

    /* Resume session */
    result = session_resume(session);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to resume session");
        session_destroy(session);
        return;
    }

    if (session->status != SESSION_STATUS_ACTIVE) {
        FAIL("Status should be ACTIVE after resume");
        session_destroy(session);
        return;
    }

    /* End session */
    result = session_end(session);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to end session");
        session_destroy(session);
        return;
    }

    if (session->status != SESSION_STATUS_ENDED) {
        FAIL("Status should be ENDED after end");
        session_destroy(session);
        return;
    }

    session_destroy(session);
    PASS();
}

/* Test sunset/sunrise protocol */
static void test_sunset_sunrise(void) {
    TEST("Sunset/sunrise protocol");

    argo_session_t* session = session_create("sunset-test", "TestProject", "main");
    if (!session) {
        FAIL("Failed to create session");
        return;
    }

    session_start(session);

    /* Sunset */
    int result = session_sunset(session, "Completed feature X implementation");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to sunset session");
        session_destroy(session);
        return;
    }

    if (session->status != SESSION_STATUS_SUNSET) {
        FAIL("Status should be SUNSET");
        session_destroy(session);
        return;
    }

    if (!session->memory->sunset_notes) {
        FAIL("Sunset notes not stored");
        session_destroy(session);
        return;
    }

    /* Sunrise */
    result = session_sunrise(session, "Continue with feature X testing");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to sunrise session");
        session_destroy(session);
        return;
    }

    if (session->status != SESSION_STATUS_ACTIVE) {
        FAIL("Status should be ACTIVE after sunrise");
        session_destroy(session);
        return;
    }

    if (!session->memory->sunrise_brief) {
        FAIL("Sunrise brief not stored");
        session_destroy(session);
        return;
    }

    session_destroy(session);
    PASS();
}

/* Test session persistence */
static void test_session_persistence(void) {
    TEST("Session persistence");

    const char* test_id = "persist-test";

    /* Clean up any existing test session */
    session_delete(test_id);

    argo_session_t* session = session_create(test_id, "TestProject", "develop");
    if (!session) {
        FAIL("Failed to create session");
        return;
    }

    session_start(session);

    /* Save session */
    int result = session_save(session);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to save session");
        session_destroy(session);
        session_delete(test_id);
        return;
    }

    /* Verify file exists */
    if (!session_exists(test_id)) {
        FAIL("Session file not created");
        session_destroy(session);
        session_delete(test_id);
        return;
    }

    session_destroy(session);

    /* Restore session */
    argo_session_t* restored = session_restore(test_id);
    if (!restored) {
        FAIL("Failed to restore session");
        session_delete(test_id);
        return;
    }

    if (strcmp(restored->id, test_id) != 0) {
        FAIL("Restored session ID mismatch");
        session_destroy(restored);
        session_delete(test_id);
        return;
    }

    session_destroy(restored);
    session_delete(test_id);

    PASS();
}

/* Test session status strings */
static void test_session_status_strings(void) {
    TEST("Session status strings");

    if (strcmp(session_status_string(SESSION_STATUS_CREATED), SESSION_STATUS_STR_CREATED) != 0) {
        FAIL("CREATED status string mismatch");
        return;
    }

    if (strcmp(session_status_string(SESSION_STATUS_ACTIVE), SESSION_STATUS_STR_ACTIVE) != 0) {
        FAIL("ACTIVE status string mismatch");
        return;
    }

    if (strcmp(session_status_string(SESSION_STATUS_PAUSED), SESSION_STATUS_STR_PAUSED) != 0) {
        FAIL("PAUSED status string mismatch");
        return;
    }

    if (strcmp(session_status_string(SESSION_STATUS_SUNSET), SESSION_STATUS_STR_SUNSET) != 0) {
        FAIL("SUNSET status string mismatch");
        return;
    }

    if (strcmp(session_status_string(SESSION_STATUS_ENDED), SESSION_STATUS_STR_ENDED) != 0) {
        FAIL("ENDED status string mismatch");
        return;
    }

    PASS();
}

/* Test session uptime calculation */
static void test_session_uptime(void) {
    TEST("Session uptime calculation");

    argo_session_t* session = session_create("uptime-test", "TestProject", "main");
    if (!session) {
        FAIL("Failed to create session");
        return;
    }

    /* Uptime should be 0 before starting */
    int uptime = session_get_uptime(session);
    if (uptime != 0) {
        FAIL("Uptime should be 0 before start");
        session_destroy(session);
        return;
    }

    session_start(session);
    sleep(1);

    uptime = session_get_uptime(session);
    if (uptime < 1) {
        FAIL("Uptime should be at least 1 second");
        session_destroy(session);
        return;
    }

    session_destroy(session);
    PASS();
}

/* Test activity tracking */
static void test_activity_tracking(void) {
    TEST("Activity tracking");

    argo_session_t* session = session_create("activity-test", "TestProject", "main");
    if (!session) {
        FAIL("Failed to create session");
        return;
    }

    session_start(session);

    time_t before = session->last_activity;
    sleep(1);
    session_update_activity(session);
    time_t after = session->last_activity;

    if (after <= before) {
        FAIL("Activity time not updated");
        session_destroy(session);
        return;
    }

    session_destroy(session);
    PASS();
}

/* Test invalid state transitions */
static void test_invalid_transitions(void) {
    TEST("Invalid state transitions");

    argo_session_t* session = session_create("invalid-test", "TestProject", "main");
    if (!session) {
        FAIL("Failed to create session");
        return;
    }

    /* Cannot pause before starting */
    int result = session_pause(session);
    if (result == ARGO_SUCCESS) {
        FAIL("Should not be able to pause before starting");
        session_destroy(session);
        return;
    }

    session_start(session);

    /* Cannot start again */
    result = session_start(session);
    if (result == ARGO_SUCCESS) {
        FAIL("Should not be able to start twice");
        session_destroy(session);
        return;
    }

    session_destroy(session);
    PASS();
}

int main(void) {
    printf("\n========================================\n");
    printf("ARGO SESSION TESTS\n");
    printf("========================================\n\n");

    test_session_creation();
    test_session_lifecycle();
    test_sunset_sunrise();
    test_session_persistence();
    test_session_status_strings();
    test_session_uptime();
    test_activity_tracking();
    test_invalid_transitions();

    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
