#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# state_file.sh - Atomic state file operations
#
# Provides functions for reading and updating state.json atomically.
# All state modifications go through these functions to ensure consistency.

# Default state file path
STATE_FILE="${STATE_FILE:-.argo-project/state.json}"
STATE_LOCK="${STATE_LOCK:-.argo-project/state.lock}"

#
# init_state_file - Create initial state.json with default structure
#
# Arguments:
#   project_name - Name of project
#   project_path - Path to project directory (default: .)
#
# Returns:
#   0 on success, 1 on failure
#
init_state_file() {
    local project_name="$1"
    local project_path="${2:-.}"

    # Validate arguments
    if [[ -z "$project_name" ]]; then
        echo "ERROR: project_name required" >&2
        return 1
    fi

    # Create .argo-project directory
    mkdir -p "$project_path/.argo-project"

    # Create initial state.json
    local timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

    cat > "$project_path/.argo-project/state.json" <<EOF
{
  "project_name": "$project_name",
  "phase": "init",
  "action_owner": "ci",
  "action_needed": "gather_requirements",
  "created": "$timestamp",
  "last_update": "$timestamp",
  "last_checkpoint": null,
  "limits": {
    "max_parallel_builders": 10,
    "max_builder_retries": 5,
    "max_workflow_hours": 48,
    "checkpoint_retention_days": 30
  },
  "design": null,
  "execution": null,
  "merge": null,
  "error_recovery": {
    "last_error": null,
    "retry_count": 0,
    "recovery_action": null
  },
  "user_query": null,
  "checkpoints": {}
}
EOF

    # Verify creation
    if [[ ! -f "$project_path/.argo-project/state.json" ]]; then
        echo "ERROR: Failed to create state.json" >&2
        return 1
    fi

    return 0
}

#
# update_state - Update a field in state.json atomically
#
# Arguments:
#   key - JSON path to field (e.g., "phase" or "execution.status")
#   value - Value to set
#
# Returns:
#   0 on success, 1 on failure
#
update_state() {
    local key="$1"
    local value="$2"

    # Validate arguments
    if [[ -z "$key" ]]; then
        echo "ERROR: key required" >&2
        return 1
    fi

    # Check state file exists
    if [[ ! -f "$STATE_FILE" ]]; then
        echo "ERROR: state file not found: $STATE_FILE" >&2
        return 1
    fi

    # Update timestamp
    local timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

    # Atomic update: write to temp file, then rename
    jq --arg k "$key" \
       --arg v "$value" \
       --arg ts "$timestamp" \
       'setpath($k | split("."); $v) | .last_update = $ts' \
       "$STATE_FILE" > "${STATE_FILE}.tmp" 2>/dev/null

    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to update state (jq error)" >&2
        rm -f "${STATE_FILE}.tmp"
        return 1
    fi

    # Atomic rename
    mv "${STATE_FILE}.tmp" "$STATE_FILE"

    return 0
}

#
# read_state - Read a value from state.json
#
# Arguments:
#   key - JSON path to field
#
# Returns:
#   Value to stdout, exit code 0 on success, 1 if not found
#
read_state() {
    local key="$1"

    # Validate arguments
    if [[ -z "$key" ]]; then
        echo "ERROR: key required" >&2
        return 1
    fi

    # Check state file exists
    if [[ ! -f "$STATE_FILE" ]]; then
        echo "ERROR: state file not found: $STATE_FILE" >&2
        return 1
    fi

    # Read value
    local value=$(jq -r ".${key}" "$STATE_FILE" 2>/dev/null)

    if [[ $? -ne 0 ]]; then
        return 1
    fi

    # Return value (empty if null)
    if [[ "$value" == "null" ]]; then
        echo ""
    else
        echo "$value"
    fi

    return 0
}

#
# validate_state_file - Validate state.json is well-formed
#
# Arguments:
#   state_file_path - Path to state file (default: STATE_FILE)
#
# Returns:
#   0 if valid, 1 if invalid
#
validate_state_file() {
    local state_file="${1:-$STATE_FILE}"

    # Check file exists
    if [[ ! -f "$state_file" ]]; then
        echo "ERROR: state file not found: $state_file" >&2
        return 1
    fi

    # Validate JSON syntax
    jq empty "$state_file" 2>/dev/null
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Invalid JSON in state file" >&2
        return 1
    fi

    # Check required fields
    local required_fields=("project_name" "phase" "action_owner" "action_needed")

    for field in "${required_fields[@]}"; do
        local value=$(jq -r ".$field" "$state_file" 2>/dev/null)
        if [[ -z "$value" ]] || [[ "$value" == "null" ]]; then
            echo "ERROR: Required field missing: $field" >&2
            return 1
        fi
    done

    return 0
}

#
# with_state_lock - Execute command with exclusive lock on state file
#
# Arguments:
#   command - Command to execute
#   args... - Command arguments
#
# Returns:
#   Command's exit code
#
with_state_lock() {
    local command="$1"
    shift
    local args=("$@")

    # Ensure lock file directory exists
    local lock_dir=$(dirname "$STATE_LOCK")
    mkdir -p "$lock_dir" 2>/dev/null

    # Create lock file if it doesn't exist
    touch "$STATE_LOCK" 2>/dev/null

    # Build full command string for bash execution
    local full_cmd="$command"
    if [[ $# -gt 0 ]]; then
        full_cmd="$command $*"
    fi

    # Export all functions so they're available in subshell
    export -f read_state update_state init_state_file validate_state_file 2>/dev/null

    # Export state file variables
    export STATE_FILE
    export STATE_LOCK

    # Use perl's flock (works on macOS and Linux)
    # Execute via bash to support functions
    perl -e '
        use Fcntl qw(:flock);
        open(my $fh, ">", $ARGV[0]) or die "Cannot open lock file: $!";
        flock($fh, LOCK_EX) or die "Cannot lock: $!";

        # Execute command via bash
        system("bash", "-c", $ARGV[1]);
        my $exit_code = $? >> 8;

        # Lock automatically released when script exits
        exit($exit_code);
    ' "$STATE_LOCK" "$full_cmd"

    return $?
}

# Export functions
export -f init_state_file
export -f update_state
export -f read_state
export -f validate_state_file
export -f with_state_lock
