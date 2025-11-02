#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# project_registry.sh - Global project registry management
#
# Provides functions for registering and tracking active projects in ~/.argo/projects.json.
# Each project gets a unique ID (name-timestamp) for coordination across workflow runs.

# Default registry location
ARGO_PROJECTS_REGISTRY="${ARGO_PROJECTS_REGISTRY:-$HOME/.argo/projects.json}"

#
# init_registry - Initialize empty registry if it doesn't exist
#
# Returns:
#   0 on success, 1 on failure
#
init_registry() {
    local registry_dir=$(dirname "$ARGO_PROJECTS_REGISTRY")

    # Create directory if needed
    mkdir -p "$registry_dir" 2>/dev/null
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to create registry directory: $registry_dir" >&2
        return 1
    fi

    # Create empty registry if it doesn't exist
    if [[ ! -f "$ARGO_PROJECTS_REGISTRY" ]]; then
        echo '{"projects": {}}' > "$ARGO_PROJECTS_REGISTRY"
        if [[ $? -ne 0 ]]; then
            echo "ERROR: Failed to create registry file" >&2
            return 1
        fi
    fi

    return 0
}

#
# register_project - Register a new project in the global registry
#
# Arguments:
#   name - Project name
#   path - Absolute path to project directory
#
# Returns:
#   Project ID to stdout (e.g., "calculator-1736960400")
#   Exit code 0 on success, 1 on failure
#
register_project() {
    local name="$1"
    local path="$2"

    # Validate arguments
    if [[ -z "$name" ]]; then
        echo "ERROR: project name required" >&2
        return 1
    fi

    if [[ -z "$path" ]]; then
        echo "ERROR: project path required" >&2
        return 1
    fi

    # Initialize registry
    init_registry
    if [[ $? -ne 0 ]]; then
        return 1
    fi

    # Generate unique project ID (name-timestamp)
    local timestamp=$(date +%s)
    local project_id="${name}-${timestamp}"

    # Get current timestamp in ISO format
    local now=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

    # Create project entry
    local temp_file="${ARGO_PROJECTS_REGISTRY}.tmp"
    jq --arg id "$project_id" \
       --arg name "$name" \
       --arg path "$path" \
       --arg created "$now" \
       --arg updated "$now" \
       '.projects[$id] = {
           "name": $name,
           "path": $path,
           "status": "idle",
           "created": $created,
           "last_active": $updated
       }' "$ARGO_PROJECTS_REGISTRY" > "$temp_file" 2>/dev/null

    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to add project to registry" >&2
        rm -f "$temp_file"
        return 1
    fi

    # Atomic update
    mv "$temp_file" "$ARGO_PROJECTS_REGISTRY"

    # Output project ID
    echo "$project_id"
    return 0
}

#
# deregister_project - Remove a project from the global registry
#
# Arguments:
#   project_id - Project ID (from register_project)
#
# Returns:
#   0 on success, 1 if project not found
#
deregister_project() {
    local project_id="$1"

    # Validate arguments
    if [[ -z "$project_id" ]]; then
        echo "ERROR: project_id required" >&2
        return 1
    fi

    # Check if registry exists
    if [[ ! -f "$ARGO_PROJECTS_REGISTRY" ]]; then
        echo "ERROR: Registry not found" >&2
        return 1
    fi

    # Check if project exists
    local exists=$(jq -r ".projects.\"$project_id\" != null" "$ARGO_PROJECTS_REGISTRY")
    if [[ "$exists" != "true" ]]; then
        echo "ERROR: Project not found: $project_id" >&2
        return 1
    fi

    # Remove project
    local temp_file="${ARGO_PROJECTS_REGISTRY}.tmp"
    jq --arg id "$project_id" \
       'del(.projects[$id])' \
       "$ARGO_PROJECTS_REGISTRY" > "$temp_file" 2>/dev/null

    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to remove project from registry" >&2
        rm -f "$temp_file"
        return 1
    fi

    # Atomic update
    mv "$temp_file" "$ARGO_PROJECTS_REGISTRY"
    return 0
}

#
# list_projects - List all registered projects
#
# Returns:
#   Tab-separated lines: project_id\tproject_name\tstatus\tlast_active
#   Exit code 0
#
list_projects() {
    # Check if registry exists
    if [[ ! -f "$ARGO_PROJECTS_REGISTRY" ]]; then
        return 0  # Empty list is success
    fi

    # Extract project info as tab-separated values
    jq -r '.projects | to_entries[] |
           "\(.key)\t\(.value.name)\t\(.value.status)\t\(.value.last_active)"' \
       "$ARGO_PROJECTS_REGISTRY" 2>/dev/null

    return 0
}

#
# get_project_info - Get detailed information about a project
#
# Arguments:
#   project_id - Project ID
#
# Returns:
#   JSON object to stdout
#   Exit code 0 on success, 1 if not found
#
get_project_info() {
    local project_id="$1"

    # Validate arguments
    if [[ -z "$project_id" ]]; then
        echo "ERROR: project_id required" >&2
        return 1
    fi

    # Check if registry exists
    if [[ ! -f "$ARGO_PROJECTS_REGISTRY" ]]; then
        echo "ERROR: Registry not found" >&2
        return 1
    fi

    # Get project info
    local info=$(jq -r ".projects.\"$project_id\"" "$ARGO_PROJECTS_REGISTRY" 2>/dev/null)

    if [[ "$info" == "null" ]] || [[ -z "$info" ]]; then
        echo "ERROR: Project not found: $project_id" >&2
        return 1
    fi

    echo "$info"
    return 0
}

#
# update_project_status - Update the status of a project
#
# Arguments:
#   project_id - Project ID
#   status - New status (e.g., "building", "idle", "failed")
#
# Returns:
#   0 on success, 1 if project not found
#
update_project_status() {
    local project_id="$1"
    local status="$2"

    # Validate arguments
    if [[ -z "$project_id" ]]; then
        echo "ERROR: project_id required" >&2
        return 1
    fi

    if [[ -z "$status" ]]; then
        echo "ERROR: status required" >&2
        return 1
    fi

    # Check if registry exists
    if [[ ! -f "$ARGO_PROJECTS_REGISTRY" ]]; then
        echo "ERROR: Registry not found" >&2
        return 1
    fi

    # Check if project exists
    local exists=$(jq -r ".projects.\"$project_id\" != null" "$ARGO_PROJECTS_REGISTRY")
    if [[ "$exists" != "true" ]]; then
        echo "ERROR: Project not found: $project_id" >&2
        return 1
    fi

    # Update status and timestamp
    local now=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
    local temp_file="${ARGO_PROJECTS_REGISTRY}.tmp"

    jq --arg id "$project_id" \
       --arg status "$status" \
       --arg updated "$now" \
       '.projects[$id].status = $status |
        .projects[$id].last_active = $updated' \
       "$ARGO_PROJECTS_REGISTRY" > "$temp_file" 2>/dev/null

    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to update project status" >&2
        rm -f "$temp_file"
        return 1
    fi

    # Atomic update
    mv "$temp_file" "$ARGO_PROJECTS_REGISTRY"
    return 0
}

#
# find_project_by_path - Find a project ID by its path
#
# Arguments:
#   project_path - Project directory path
#
# Returns:
#   Project ID to stdout if found
#   Exit code 0 if found, 1 if not found
#
find_project_by_path() {
    local project_path="$1"

    # Validate arguments
    if [[ -z "$project_path" ]]; then
        echo "ERROR: project_path required" >&2
        return 1
    fi

    # Check if registry exists
    if [[ ! -f "$ARGO_PROJECTS_REGISTRY" ]]; then
        echo "ERROR: Registry not found" >&2
        return 1
    fi

    # Find project by path
    local project_id=$(jq -r --arg path "$project_path" \
                           '.projects | to_entries[] |
                            select(.value.path == $path) |
                            .key' \
                           "$ARGO_PROJECTS_REGISTRY" 2>/dev/null | head -n1)

    if [[ -z "$project_id" ]]; then
        echo "ERROR: No project found for path: $project_path" >&2
        return 1
    fi

    echo "$project_id"
    return 0
}

# Export functions
export -f init_registry
export -f register_project
export -f deregister_project
export -f list_projects
export -f get_project_info
export -f update_project_status
export -f find_project_by_path
