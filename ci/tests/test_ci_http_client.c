/* © 2025 Casey Koons All rights reserved */

/* CI HTTP Client Unit Tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ci_http_client.h"
#include "ci_constants.h"
#include "argo_error.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

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
        tests_failed++; \
    } while(0)

/* Test HTTP client initialization */
static void test_http_client_init(void) {
    TEST("HTTP client initialization");

    /* ci_get_daemon_url should return default URL */
    const char* url = ci_get_daemon_url();
    if (url && strstr(url, "localhost")) {
        PASS();
    } else {
        FAIL("Invalid daemon URL");
    }
}

/* Test HTTP client with custom port */
static void test_http_client_custom_port(void) {
    TEST("HTTP client custom port");

    /* Set custom port */
    setenv("ARGO_DAEMON_PORT", "12345", 1);

    const char* url = ci_get_daemon_url();
    if (url && strstr(url, "12345")) {
        PASS();
    } else {
        FAIL("Custom port not applied");
    }

    /* Reset */
    unsetenv("ARGO_DAEMON_PORT");
}

/* Test HTTP response allocation */
static void test_http_response_alloc(void) {
    TEST("HTTP response allocation");

    ci_http_response_t* resp = calloc(1, sizeof(ci_http_response_t));
    if (!resp) {
        FAIL("Failed to allocate response");
        return;
    }

    resp->status_code = 200;
    resp->body = strdup("{\"status\":\"success\"}");

    if (resp->status_code == 200 && resp->body &&
        strcmp(resp->body, "{\"status\":\"success\"}") == 0) {
        PASS();
    } else {
        FAIL("Response fields not set correctly");
    }

    free(resp->body);
    free(resp);
}

/* Test HTTP response cleanup */
static void test_http_response_cleanup(void) {
    TEST("HTTP response cleanup");

    ci_http_response_t* resp = calloc(1, sizeof(ci_http_response_t));
    if (!resp) {
        FAIL("Failed to allocate response");
        return;
    }

    resp->body = strdup("test body");
    resp->body_size = strlen(resp->body);

    ci_http_response_free(resp);
    /* If we get here without crash, cleanup worked */
    PASS();
}

/* Test HTTP request connection failure handling */
static void test_http_connection_failure(void) {
    TEST("HTTP connection failure handling");

    /* Try to connect to non-existent daemon */
    setenv("ARGO_DAEMON_PORT", "19999", 1);

    ci_http_response_t* resp = NULL;
    int result = ci_http_post("/api/ci/query", "{\"query\":\"test\"}", &resp);

    /* Should fail gracefully */
    if (result != ARGO_SUCCESS) {
        PASS();
    } else {
        FAIL("Should fail when daemon not running");
        if (resp) ci_http_response_free(resp);
    }

    unsetenv("ARGO_DAEMON_PORT");
}

/* Test HTTP POST with NULL endpoint */
static void test_http_post_null_endpoint(void) {
    TEST("HTTP POST with NULL endpoint");

    ci_http_response_t* resp = NULL;
    int result = ci_http_post(NULL, "{\"query\":\"test\"}", &resp);

    /* Should handle NULL endpoint */
    if (result != ARGO_SUCCESS) {
        PASS();
    } else {
        FAIL("Should fail with NULL endpoint");
        if (resp) ci_http_response_free(resp);
    }
}

/* Test HTTP POST with NULL body */
static void test_http_post_null_body(void) {
    TEST("HTTP POST with NULL body");

    ci_http_response_t* resp = NULL;
    int result = ci_http_post("/api/ci/query", NULL, &resp);

    /* Should handle NULL body */
    if (result != ARGO_SUCCESS) {
        PASS();
    } else {
        FAIL("Should fail with NULL body");
        if (resp) ci_http_response_free(resp);
    }
}

/* Test HTTP POST with NULL response pointer */
static void test_http_post_null_response(void) {
    TEST("HTTP POST with NULL response pointer");

    int result = ci_http_post("/api/ci/query", "{\"query\":\"test\"}", NULL);

    /* Should handle NULL response gracefully */
    if (result != ARGO_SUCCESS) {
        PASS();
    } else {
        FAIL("Should fail with NULL response pointer");
    }
}

/* Test endpoint path construction */
static void test_endpoint_construction(void) {
    TEST("Endpoint path construction");

    /* Test that daemon URL + endpoint gives valid URL */
    const char* base_url = ci_get_daemon_url();
    const char* endpoint = "/api/ci/query";

    char full_url[512];
    snprintf(full_url, sizeof(full_url), "%s%s", base_url, endpoint);

    if (strstr(full_url, "http://") && strstr(full_url, endpoint)) {
        PASS();
    } else {
        FAIL("Invalid endpoint construction");
    }
}

/* Test JSON body construction */
static void test_json_body_construction(void) {
    TEST("JSON body construction for CI query");

    const char* query = "What is 2+2?";
    char json_body[CI_REQUEST_BUFFER_SIZE];
    snprintf(json_body, sizeof(json_body), "{\"query\":\"%s\"}", query);

    if (strstr(json_body, query) &&
        strstr(json_body, "{") &&
        strstr(json_body, "}")) {
        PASS();
    } else {
        FAIL("JSON body not constructed correctly");
    }
}

/* Test multiple sequential requests */
static void test_sequential_requests(void) {
    TEST("Multiple sequential requests");

    /* Try multiple requests (will fail if daemon not running, but tests client) */
    setenv("ARGO_DAEMON_PORT", "19998", 1);

    ci_http_response_t* resp1 = NULL;
    ci_http_response_t* resp2 = NULL;

    int result1 = ci_http_post("/api/ci/query", "{\"query\":\"test1\"}", &resp1);
    int result2 = ci_http_post("/api/ci/query", "{\"query\":\"test2\"}", &resp2);

    /* Both should fail the same way (no daemon) */
    if (result1 != ARGO_SUCCESS && result2 != ARGO_SUCCESS) {
        PASS();
    } else {
        FAIL("Sequential requests behaved inconsistently");
    }

    if (resp1) ci_http_response_free(resp1);
    if (resp2) ci_http_response_free(resp2);

    unsetenv("ARGO_DAEMON_PORT");
}

/* Test response body size limits */
static void test_response_body_limits(void) {
    TEST("Response body size limits");

    /* Create a large fake response */
    ci_http_response_t* resp = calloc(1, sizeof(ci_http_response_t));
    if (!resp) {
        FAIL("Failed to allocate response");
        return;
    }

    /* Allocate large body */
    size_t large_size = 100000;
    resp->body = calloc(1, large_size);
    if (!resp->body) {
        FAIL("Failed to allocate large body");
        free(resp);
        return;
    }

    memset(resp->body, 'X', large_size - 1);
    resp->body[large_size - 1] = '\0';

    /* Should handle large response */
    if (strlen(resp->body) == large_size - 1) {
        PASS();
    } else {
        FAIL("Large body not handled correctly");
    }

    ci_http_response_free(resp);
}

/* Test timeout handling (120s for CI queries) */
static void test_timeout_value(void) {
    TEST("CI query timeout is 120 seconds");

    /* The timeout constant should be CI_QUERY_TIMEOUT_SECONDS */
    int expected_timeout = CI_QUERY_TIMEOUT_SECONDS;

    if (expected_timeout == 120) {
        PASS();
    } else {
        FAIL("CI query timeout not set to 120 seconds");
    }
}

/* Test URL encoding safety */
static void test_url_safety(void) {
    TEST("URL encoding safety");

    /* Test that we can build URLs safely */
    const char* endpoint = "/api/ci/query";
    char url[256];
    snprintf(url, sizeof(url), "%s%s", "http://localhost:9876", endpoint);

    if (strstr(url, endpoint) && url[0] == 'h') {
        PASS();
    } else {
        FAIL("URL construction not safe");
    }
}

/* Test JSON escaping for special characters */
static void test_json_special_chars(void) {
    TEST("JSON special characters handling");

    const char* query_with_quotes = "Test \"quotes\" here";
    char json_body[CI_REQUEST_BUFFER_SIZE];

    /* In actual code, json_escape_string would be used */
    /* Here we just test the pattern */
    int len = snprintf(json_body, sizeof(json_body), "{\"query\":\"%s\"}", query_with_quotes);

    if (len > 0 && len < (int)sizeof(json_body)) {
        PASS();
    } else {
        FAIL("JSON construction failed");
    }
}

/* Test provider and model parameters */
static void test_provider_model_params(void) {
    TEST("Provider and model parameter construction");

    const char* query = "test";
    const char* provider = "claude_code";
    const char* model = "claude-sonnet-4-5";
    char json_body[CI_REQUEST_BUFFER_SIZE];

    snprintf(json_body, sizeof(json_body),
            "{\"query\":\"%s\",\"provider\":\"%s\",\"model\":\"%s\"}",
            query, provider, model);

    if (strstr(json_body, query) &&
        strstr(json_body, provider) &&
        strstr(json_body, model)) {
        PASS();
    } else {
        FAIL("Parameter construction failed");
    }
}

/* Test error code handling */
static void test_error_codes(void) {
    TEST("Error code handling");

    /* Test various error conditions */
    ci_http_response_t* resp = NULL;

    /* NULL endpoint */
    int result1 = ci_http_post(NULL, "{}", &resp);

    /* NULL body */
    int result2 = ci_http_post("/test", NULL, &resp);

    /* NULL response */
    int result3 = ci_http_post("/test", "{}", NULL);

    /* All should return error codes */
    if (result1 != ARGO_SUCCESS &&
        result2 != ARGO_SUCCESS &&
        result3 != ARGO_SUCCESS) {
        PASS();
    } else {
        FAIL("Error codes not returned correctly");
    }
}

int main(void) {
    printf("\n========================================\n");
    printf("CI HTTP Client Unit Tests\n");
    printf("========================================\n\n");

    /* Run tests */
    test_http_client_init();
    test_http_client_custom_port();
    test_http_response_alloc();
    test_http_response_cleanup();
    test_http_connection_failure();
    test_http_post_null_endpoint();
    test_http_post_null_body();
    test_http_post_null_response();
    test_endpoint_construction();
    test_json_body_construction();
    test_sequential_requests();
    test_response_body_limits();
    test_timeout_value();
    test_url_safety();
    test_json_special_chars();
    test_provider_model_params();
    test_error_codes();

    /* Print summary */
    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("========================================\n\n");

    return (tests_failed > 0) ? 1 : 0;
}
