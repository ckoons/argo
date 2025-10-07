/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>

/* Project includes */
#include "arc_http_client.h"
#include "arc_error.h"
#include "argo_error.h"

/* Write callback for curl */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    arc_http_response_t* resp = (arc_http_response_t*)userp;

    char* ptr = realloc(resp->body, resp->body_size + realsize + 1);
    if (!ptr) {
        return 0;
    }

    resp->body = ptr;
    memcpy(&(resp->body[resp->body_size]), contents, realsize);
    resp->body_size += realsize;
    resp->body[resp->body_size] = '\0';

    return realsize;
}

/* Get daemon base URL */
const char* arc_get_daemon_url(void) {
    static char url_buffer[256];
    static int initialized = 0;

    if (!initialized) {
        int port = ARC_DAEMON_DEFAULT_PORT;
        const char* port_env = getenv(ARC_DAEMON_PORT_ENV);
        if (port_env) {
            port = atoi(port_env);
        }
        snprintf(url_buffer, sizeof(url_buffer), "http://%s:%d", 
                 ARC_DAEMON_DEFAULT_HOST, port);
        initialized = 1;
    }

    return url_buffer;
}

/* HTTP GET request */
int arc_http_get(const char* endpoint, arc_http_response_t** response) {
    if (!endpoint || !response) {
        return E_INPUT_NULL;
    }

    /* Ensure daemon is running */
    int result = arc_ensure_daemon_running();
    if (result != ARGO_SUCCESS) {
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return E_SYSTEM_MEMORY;
    }

    arc_http_response_t* resp = calloc(1, sizeof(arc_http_response_t));
    if (!resp) {
        curl_easy_cleanup(curl);
        return E_SYSTEM_MEMORY;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s%s", arc_get_daemon_url(), endpoint);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        arc_http_response_free(resp);
        curl_easy_cleanup(curl);
        return E_SYSTEM_NETWORK;
    }

    long status_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    resp->status_code = (int)status_code;

    curl_easy_cleanup(curl);
    *response = resp;
    return ARGO_SUCCESS;
}

/* HTTP POST request */
int arc_http_post(const char* endpoint, const char* json_body, arc_http_response_t** response) {
    if (!endpoint || !json_body || !response) {
        return E_INPUT_NULL;
    }

    /* Ensure daemon is running */
    int result = arc_ensure_daemon_running();
    if (result != ARGO_SUCCESS) {
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return E_SYSTEM_MEMORY;
    }

    arc_http_response_t* resp = calloc(1, sizeof(arc_http_response_t));
    if (!resp) {
        curl_easy_cleanup(curl);
        return E_SYSTEM_MEMORY;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s%s", arc_get_daemon_url(), endpoint);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        arc_http_response_free(resp);
        curl_easy_cleanup(curl);
        return E_SYSTEM_NETWORK;
    }

    long status_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    resp->status_code = (int)status_code;

    curl_easy_cleanup(curl);
    *response = resp;
    return ARGO_SUCCESS;
}

/* HTTP DELETE request */
int arc_http_delete(const char* endpoint, arc_http_response_t** response) {
    if (!endpoint || !response) {
        return E_INPUT_NULL;
    }

    /* Ensure daemon is running */
    int result = arc_ensure_daemon_running();
    if (result != ARGO_SUCCESS) {
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return E_SYSTEM_MEMORY;
    }

    arc_http_response_t* resp = calloc(1, sizeof(arc_http_response_t));
    if (!resp) {
        curl_easy_cleanup(curl);
        return E_SYSTEM_MEMORY;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s%s", arc_get_daemon_url(), endpoint);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        arc_http_response_free(resp);
        curl_easy_cleanup(curl);
        return E_SYSTEM_NETWORK;
    }

    long status_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    resp->status_code = (int)status_code;

    curl_easy_cleanup(curl);
    *response = resp;
    return ARGO_SUCCESS;
}

/* Free response */
void arc_http_response_free(arc_http_response_t* response) {
    if (!response) return;

    free(response->body);
    free(response);
}

/* Check if daemon is running by trying /api/health */
static int is_daemon_running(void) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return 0;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/api/health", arc_get_daemon_url());

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);  /* HEAD request */
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1L);  /* Quick timeout */

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK);
}

/* Start daemon in background */
static int start_daemon(void) {
    pid_t pid = fork();
    if (pid < 0) {
        return E_SYSTEM_PROCESS;
    }

    if (pid == 0) {
        /* Child process - start daemon */
        /* Redirect stdout/stderr to /dev/null */
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        /* Get port from environment or use default */
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", ARC_DAEMON_DEFAULT_PORT);

        const char* port_env = getenv(ARC_DAEMON_PORT_ENV);
        if (port_env) {
            snprintf(port_str, sizeof(port_str), "%s", port_env);
        }

        /* Execute daemon */
        execlp("argo-daemon", "argo-daemon", "--port", port_str, NULL);

        /* If exec fails, exit child */
        exit(1);
    }

    /* Parent process - wait a moment for daemon to start */
    sleep(2);

    /* Verify daemon started */
    if (!is_daemon_running()) {
        return E_SYSTEM_PROCESS;
    }

    return ARGO_SUCCESS;
}

/* Ensure daemon is running, start if needed */
int arc_ensure_daemon_running(void) {
    if (is_daemon_running()) {
        return ARGO_SUCCESS;
    }

    /* Daemon not running - try to start it */
    return start_daemon();
}
