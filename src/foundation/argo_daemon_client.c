/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_daemon_client.h"
#include "argo_config.h"
#include "argo_env_utils.h"
#include "argo_limits.h"

/* Static buffer for URL */
static char daemon_url_buffer[ARGO_BUFFER_MEDIUM];
static bool url_initialized = false;

/* Get daemon host */
const char* argo_get_daemon_host(void) {
    /* 1. Check config file */
    const char* host = argo_config_get("daemon_host");
    if (host && host[0]) {
        return host;
    }

    /* 2. Check environment */
    host = argo_getenv(ARGO_DAEMON_HOST_ENV);
    if (host && host[0]) {
        return host;
    }

    /* 3. Use default */
    return ARGO_DAEMON_DEFAULT_HOST;
}

/* Get daemon port */
int argo_get_daemon_port(void) {
    /* 1. Check config file */
    const char* port_str = argo_config_get("daemon_port");
    if (port_str && port_str[0]) {
        char* endptr = NULL;
        long port = strtol(port_str, &endptr, 10);
        if (endptr != port_str && port > 0 && port < 65536) {
            return (int)port;
        }
    }

    /* 2. Check environment */
    port_str = argo_getenv(ARGO_DAEMON_PORT_ENV);
    if (port_str && port_str[0]) {
        char* endptr = NULL;
        long port = strtol(port_str, &endptr, 10);
        if (endptr != port_str && port > 0 && port < 65536) {
            return (int)port;
        }
    }

    /* 3. Use default */
    return ARGO_DAEMON_DEFAULT_PORT;
}

/* Get complete daemon URL */
const char* argo_get_daemon_url(void) {
    if (!url_initialized) {
        const char* host = argo_get_daemon_host();
        int port = argo_get_daemon_port();

        snprintf(daemon_url_buffer, sizeof(daemon_url_buffer),
                "http://%s:%d", host, port);

        url_initialized = true;
    }

    return daemon_url_buffer;
}
