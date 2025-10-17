/* © 2025 Casey Koons All rights reserved */
/* Daemon CI Query API - POST /api/ci/query */

#ifndef ARGO_DAEMON_CI_API_H
#define ARGO_DAEMON_CI_API_H

#include "argo_http_server.h"

/* POST /api/ci/query - Query AI provider */
int api_ci_query(http_request_t* req, http_response_t* resp);

#endif /* ARGO_DAEMON_CI_API_H */
