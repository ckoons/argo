#!/usr/bin/env python3
# © 2025 Casey Koons All rights reserved
# Script to migrate API endpoints from temporary registry to shared registry

import re
import sys

def migrate_endpoint(content):
    """Migrate all endpoints to use shared registry with mutex"""

    # Pattern 1: Replace registry creation with shared registry access
    pattern1 = r'(/\* Load registry \*/\s+)?workflow_registry_t\* registry = workflow_registry_create\("[^"]+"\);'
    replacement1 = '''/* Use shared registry with mutex protection */
    pthread_mutex_lock(&g_api_daemon->workflow_registry_lock);

    workflow_registry_t* registry = g_api_daemon->workflow_registry;'''

    content = re.sub(pattern1, replacement1, content)

    # Pattern 2: Replace first error check after creation
    pattern2 = r'if \(!registry\) \{(\s+)http_response_set_error\(resp, HTTP_STATUS_SERVER_ERROR, "Failed to create registry"\);(\s+)return E_SYSTEM_MEMORY;(\s+)\}'
    replacement2 = r'if (!registry) {\1pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);\1http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Registry not initialized");\1return E_INVALID_STATE;\3}'

    content = re.sub(pattern2, replacement2, content)

    # Pattern 3: Replace workflow_registry_load and error handling
    pattern3 = r'int result = workflow_registry_load\(registry\);(\s+)if \(result != ARGO_SUCCESS && result != E_SYSTEM_FILE\) \{(\s+)workflow_registry_destroy\(registry\);(\s+)http_response_set_error\([^)]+\);(\s+)return result;(\s+)\}(\s+)(/\* Cleanup dead workflows after loading \*/)?(\s+)(workflow_registry_cleanup_dead_workflows\(registry\);)?'
    replacement3 = ''

    content = re.sub(pattern3, replacement3, content)

    # Pattern 4: Replace all workflow_registry_destroy(registry) with pthread_mutex_unlock
    content = re.sub(r'workflow_registry_destroy\(registry\);',
                    'pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);',
                    content)

    return content

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <file>")
        sys.exit(1)

    filepath = sys.argv[1]

    with open(filepath, 'r') as f:
        content = f.read()

    migrated = migrate_endpoint(content)

    with open(filepath, 'w') as f:
        f.write(migrated)

    print(f"Migrated {filepath}")
