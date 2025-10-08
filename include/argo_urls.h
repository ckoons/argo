/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_URLS_H
#define ARGO_URLS_H

/* Daemon HTTP configuration */
#define DEFAULT_DAEMON_HOST "localhost"
#define DEFAULT_DAEMON_PORT 9876

/* API endpoint paths */
#define API_PATH_HEALTH "/api/health"
#define API_PATH_VERSION "/api/version"
#define API_PATH_WORKFLOW_START "/api/workflow/start"
#define API_PATH_WORKFLOW_LIST "/api/workflow/list"
#define API_PATH_WORKFLOW_STATUS "/api/workflow/status"
#define API_PATH_WORKFLOW_PROGRESS "/api/workflow/progress"
#define API_PATH_WORKFLOW_ABANDON "/api/workflow/abandon"
#define API_PATH_WORKFLOW_PAUSE "/api/workflow/pause"
#define API_PATH_WORKFLOW_RESUME "/api/workflow/resume"

#endif /* ARGO_URLS_H */
