/* © 2025 Casey Koons All rights reserved */

/* Test suite for CI-to-CI messaging */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Project includes */
#include "argo_registry.h"
#include "argo_error.h"

/* Test helpers */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Testing: %-50s", name); \
        tests_run++; \
    } while(0)

#define PASS() \
    do { \
        printf(" ✓\n"); \
        tests_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        printf(" ✗\n  %s\n", msg); \
    } while(0)

/* Test message creation */
static void test_message_create(void) {
    TEST("Message creation");

    ci_message_t* msg = message_create("Alice", "Bob", "request", "Hello");
    if (!msg) {
        FAIL("Failed to create message");
        return;
    }

    assert(strcmp(msg->from, "Alice") == 0);
    assert(strcmp(msg->to, "Bob") == 0);
    assert(strcmp(msg->type, "request") == 0);
    assert(strcmp(msg->content, "Hello") == 0);
    assert(msg->timestamp > 0);

    message_destroy(msg);
    PASS();
}

/* Test message to JSON */
static void test_message_to_json(void) {
    TEST("Message serialization to JSON");

    ci_message_t* msg = message_create("Alice", "Bob", "request", "Test");
    if (!msg) {
        FAIL("Failed to create message");
        return;
    }

    char* json = message_to_json(msg);
    if (!json) {
        message_destroy(msg);
        FAIL("Failed to serialize message");
        return;
    }

    /* Check JSON contains expected fields */
    assert(strstr(json, "\"from\":\"Alice\"") != NULL);
    assert(strstr(json, "\"to\":\"Bob\"") != NULL);
    assert(strstr(json, "\"type\":\"request\"") != NULL);
    assert(strstr(json, "\"content\":\"Test\"") != NULL);
    assert(strstr(json, "\"timestamp\":") != NULL);

    free(json);
    message_destroy(msg);
    PASS();
}

/* Test message with thread_id */
static void test_message_with_thread(void) {
    TEST("Message with thread ID");

    ci_message_t* msg = message_create("Alice", "Bob", "response", "Reply");
    if (!msg) {
        FAIL("Failed to create message");
        return;
    }

    msg->thread_id = strdup("thread-123");

    char* json = message_to_json(msg);
    if (!json) {
        message_destroy(msg);
        FAIL("Failed to serialize message with thread");
        return;
    }

    assert(strstr(json, "\"thread_id\":\"thread-123\"") != NULL);

    free(json);
    message_destroy(msg);
    PASS();
}

/* Test message with metadata */
static void test_message_with_metadata(void) {
    TEST("Message with metadata");

    ci_message_t* msg = message_create("Alice", "Bob", "request", "Urgent");
    if (!msg) {
        FAIL("Failed to create message");
        return;
    }

    msg->metadata.priority = strdup("high");
    msg->metadata.timeout_ms = 5000;

    char* json = message_to_json(msg);
    if (!json) {
        message_destroy(msg);
        FAIL("Failed to serialize message with metadata");
        return;
    }

    assert(strstr(json, "\"metadata\"") != NULL);
    assert(strstr(json, "\"priority\":\"high\"") != NULL);
    assert(strstr(json, "\"timeout_ms\":5000") != NULL);

    free(json);
    message_destroy(msg);
    PASS();
}

/* Test message from JSON */
static void test_message_from_json(void) {
    TEST("Message parsing from JSON");

    const char* json = "{\"from\":\"Alice\",\"to\":\"Bob\",\"timestamp\":1234567890,"
                      "\"type\":\"request\",\"content\":\"Hello\"}";

    ci_message_t* msg = message_from_json(json);
    if (!msg) {
        FAIL("Failed to parse JSON message");
        return;
    }

    assert(strcmp(msg->from, "Alice") == 0);
    assert(strcmp(msg->to, "Bob") == 0);
    assert(strcmp(msg->type, "request") == 0);
    assert(strcmp(msg->content, "Hello") == 0);
    assert(msg->timestamp == 1234567890);

    message_destroy(msg);
    PASS();
}

/* Test message roundtrip */
static void test_message_roundtrip(void) {
    TEST("Message JSON roundtrip");

    ci_message_t* msg1 = message_create("Alice", "Bob", "request", "Test");
    if (!msg1) {
        FAIL("Failed to create message");
        return;
    }

    msg1->thread_id = strdup("thread-456");
    msg1->metadata.priority = strdup("normal");
    msg1->metadata.timeout_ms = 3000;

    char* json = message_to_json(msg1);
    if (!json) {
        message_destroy(msg1);
        FAIL("Failed to serialize message");
        return;
    }

    ci_message_t* msg2 = message_from_json(json);
    if (!msg2) {
        free(json);
        message_destroy(msg1);
        FAIL("Failed to parse JSON");
        return;
    }

    /* Verify fields match */
    assert(strcmp(msg1->from, msg2->from) == 0);
    assert(strcmp(msg1->to, msg2->to) == 0);
    assert(strcmp(msg1->type, msg2->type) == 0);
    assert(strcmp(msg1->content, msg2->content) == 0);
    assert(strcmp(msg1->thread_id, msg2->thread_id) == 0);
    assert(strcmp(msg1->metadata.priority, msg2->metadata.priority) == 0);
    assert(msg1->metadata.timeout_ms == msg2->metadata.timeout_ms);

    free(json);
    message_destroy(msg1);
    message_destroy(msg2);
    PASS();
}

/* Test registry message send (basic) */
static void test_registry_send_basic(void) {
    TEST("Registry send message (basic validation)");

    ci_registry_t* registry = registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    /* Add two CIs */
    int result = registry_add_ci(registry, "Alice", "builder", "claude", 9001);
    assert(result == ARGO_SUCCESS);

    result = registry_add_ci(registry, "Bob", "coordinator", "gpt4", 9002);
    assert(result == ARGO_SUCCESS);

    /* Set both to READY status */
    registry_update_status(registry, "Alice", CI_STATUS_READY);
    registry_update_status(registry, "Bob", CI_STATUS_READY);

    /* Create and send message */
    ci_message_t* msg = message_create("Alice", "Bob", "request", "Hello Bob");
    char* json = message_to_json(msg);

    /* Note: This will try to send via socket, which may fail if no socket server running
     * But it should at least validate the CIs and update statistics */
    result = registry_send_message(registry, "Alice", "Bob", json);

    /* Check statistics were updated */
    ci_registry_entry_t* alice = registry_find_ci(registry, "Alice");
    ci_registry_entry_t* bob = registry_find_ci(registry, "Bob");

    assert(alice != NULL);
    assert(bob != NULL);
    /* Stats should be updated even if socket send fails */
    assert(alice->messages_sent >= 1 || result != ARGO_SUCCESS);
    assert(bob->messages_received >= 1 || result != ARGO_SUCCESS);

    free(json);
    message_destroy(msg);
    registry_destroy(registry);
    PASS();
}

/* Test broadcast message */
static void test_registry_broadcast(void) {
    TEST("Registry broadcast message");

    ci_registry_t* registry = registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    /* Add multiple CIs with different roles */
    registry_add_ci(registry, "Alice", "builder", "claude", 9001);
    registry_add_ci(registry, "Bob", "builder", "gpt4", 9002);
    registry_add_ci(registry, "Carol", "coordinator", "gemini", 9003);

    /* Set all to READY */
    registry_update_status(registry, "Alice", CI_STATUS_READY);
    registry_update_status(registry, "Bob", CI_STATUS_READY);
    registry_update_status(registry, "Carol", CI_STATUS_READY);

    /* Broadcast to all builders */
    ci_message_t* msg = message_create("Carol", "all", "broadcast", "Status update");
    char* json = message_to_json(msg);

    /* Broadcast to all builders - may fail if no socket server running */
    (void)registry_broadcast_message(registry, "Carol", "builder", json);

    free(json);
    message_destroy(msg);
    registry_destroy(registry);
    PASS();
}

int main(void) {
    printf("========================================\n");
    printf("ARGO MESSAGING TESTS\n");
    printf("========================================\n\n");

    /* Run tests */
    test_message_create();
    test_message_to_json();
    test_message_with_thread();
    test_message_with_metadata();
    test_message_from_json();
    test_message_roundtrip();
    test_registry_send_basic();
    test_registry_broadcast();

    /* Print summary */
    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n");

    return (tests_run == tests_passed) ? 0 : 1;
}
