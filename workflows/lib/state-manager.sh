#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# state-manager.sh - Session state management for Argo workflows
#
# Provides functions for creating, loading, and managing workflow session state.
# Sessions track project context, builder states, and workflow progress.

# Base directory for all sessions
SESSIONS_DIR="${HOME}/.argo/sessions"
ARCHIVES_DIR="${HOME}/.argo/archives"

# Ensure directories exist
mkdir -p "$SESSIONS_DIR"
mkdir -p "$ARCHIVES_DIR"

# ============================================================================
# Session Management
# ============================================================================

# Create a new session with unique ID
# Returns: session_id
create_session() {
    local project_name="${1:-unnamed}"

    # Generate session ID: proj-YYYYMMDD-HHMMSS
    local session_id="proj-$(date +%Y%m%d-%H%M%S)"
    local session_dir="$SESSIONS_DIR/$session_id"

    # Create session directory structure
    mkdir -p "$session_dir/builders"

    # Create empty context
    cat > "$session_dir/context.json" <<EOF
{
  "session_id": "$session_id",
  "project_name": "$project_name",
  "created": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "current_phase": "initialized"
}
EOF

    echo "$session_id"
}

# Load session ID from current workflow context or create new
# Returns: session_id
get_or_create_session() {
    local project_name="$1"

    # Check if SESSION_ID already set (from parent workflow)
    if [[ -n "${SESSION_ID:-}" ]] && [[ -d "$SESSIONS_DIR/$SESSION_ID" ]]; then
        echo "$SESSION_ID"
        return 0
    fi

    # Create new session
    create_session "$project_name"
}

# Check if session exists
# Args: session_id
# Returns: 0 if exists, 1 if not
session_exists() {
    local session_id="$1"
    [[ -d "$SESSIONS_DIR/$session_id" ]]
}

# Get session directory path
# Args: session_id
# Returns: full path to session directory
get_session_dir() {
    local session_id="$1"
    echo "$SESSIONS_DIR/$session_id"
}

# ============================================================================
# Context Management
# ============================================================================

# Save project context to session
# Args: session_id, field_name, field_value
save_context_field() {
    local session_id="$1"
    local field="$2"
    local value="$3"
    local context_file="$SESSIONS_DIR/$session_id/context.json"

    if [[ ! -f "$context_file" ]]; then
        echo "Error: Context file not found for session $session_id" >&2
        return 1
    fi

    # Update field using jq
    local temp_file=$(mktemp)
    jq --arg field "$field" --arg value "$value" \
       '.[$field] = $value' \
       "$context_file" > "$temp_file"
    mv "$temp_file" "$context_file"
}

# Save complete context (overwrites existing)
# Args: session_id, json_string
save_context() {
    local session_id="$1"
    local json_data="$2"
    local context_file="$SESSIONS_DIR/$session_id/context.json"

    echo "$json_data" > "$context_file"
}

# Load entire context as JSON
# Args: session_id
# Returns: JSON string
load_context() {
    local session_id="$1"
    local context_file="$SESSIONS_DIR/$session_id/context.json"

    if [[ ! -f "$context_file" ]]; then
        echo "Error: Context file not found for session $session_id" >&2
        return 1
    fi

    cat "$context_file"
}

# Get single context field value
# Args: session_id, field_name
# Returns: field value
get_context_field() {
    local session_id="$1"
    local field="$2"
    local context_file="$SESSIONS_DIR/$session_id/context.json"

    if [[ ! -f "$context_file" ]]; then
        echo "Error: Context file not found for session $session_id" >&2
        return 1
    fi

    jq -r ".$field" "$context_file"
}

# Update current phase in context
# Args: session_id, phase_name
update_phase() {
    local session_id="$1"
    local phase="$2"
    save_context_field "$session_id" "current_phase" "$phase"
}

# ============================================================================
# Builder State Management
# ============================================================================

# Initialize builder state file
# Args: session_id, builder_id, feature_branch, task_description
init_builder_state() {
    local session_id="$1"
    local builder_id="$2"
    local feature_branch="$3"
    local task_description="$4"
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"

    cat > "$builder_file" <<EOF
{
  "builder_id": "$builder_id",
  "feature_branch": "$feature_branch",
  "task_description": "$task_description",
  "status": "initialized",
  "build_dir": "",
  "tests_passed": false,
  "tests_passed_count": 0,
  "tests_failed_count": 0,
  "total_tests": 0,
  "failed_tests": [],
  "commits": [],
  "needs_assistance": false,
  "created": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "completion_time": null
}
EOF
}

# Update builder status
# Args: session_id, builder_id, status
update_builder_status() {
    local session_id="$1"
    local builder_id="$2"
    local status="$3"
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"

    if [[ ! -f "$builder_file" ]]; then
        echo "Error: Builder state file not found: $builder_id" >&2
        return 1
    fi

    local temp_file=$(mktemp)
    jq --arg status "$status" \
       '.status = $status' \
       "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"

    # If completed or failed, set completion time
    if [[ "$status" == "completed" ]] || [[ "$status" == "failed" ]]; then
        local temp_file2=$(mktemp)
        jq --arg time "$(date -u +"%Y-%m-%dT%H:%M:%SZ")" \
           '.completion_time = $time' \
           "$builder_file" > "$temp_file2"
        mv "$temp_file2" "$builder_file"
    fi
}

# Save complete builder state
# Args: session_id, builder_id, json_string
save_builder_state() {
    local session_id="$1"
    local builder_id="$2"
    local json_data="$3"
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"

    echo "$json_data" > "$builder_file"
}

# Load builder state
# Args: session_id, builder_id
# Returns: JSON string
load_builder_state() {
    local session_id="$1"
    local builder_id="$2"
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"

    if [[ ! -f "$builder_file" ]]; then
        echo "Error: Builder state file not found: $builder_id" >&2
        return 1
    fi

    cat "$builder_file"
}

# Get builder status
# Args: session_id, builder_id
# Returns: status string
get_builder_status() {
    local session_id="$1"
    local builder_id="$2"
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"

    if [[ ! -f "$builder_file" ]]; then
        echo "not_found"
        return 1
    fi

    jq -r '.status' "$builder_file"
}

# Check if builder needs assistance
# Args: session_id, builder_id
# Returns: 0 if needs help, 1 if not
builder_needs_assistance() {
    local session_id="$1"
    local builder_id="$2"
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"

    if [[ ! -f "$builder_file" ]]; then
        return 1
    fi

    local needs_help=$(jq -r '.needs_assistance' "$builder_file")
    [[ "$needs_help" == "true" ]]
}

# List all builders for a session
# Args: session_id
# Returns: space-separated list of builder IDs
list_builders() {
    local session_id="$1"
    local builders_dir="$SESSIONS_DIR/$session_id/builders"

    if [[ ! -d "$builders_dir" ]]; then
        return 0
    fi

    # List all .json files and extract builder IDs
    shopt -s nullglob
    for file in "$builders_dir"/*.json; do
        if [[ -f "$file" ]]; then
            basename "$file" .json
        fi
    done
    shopt -u nullglob
}

# Get count of builders in each status
# Args: session_id
# Returns: JSON with status counts
get_builder_statistics() {
    local session_id="$1"
    local builders_dir="$SESSIONS_DIR/$session_id/builders"

    local total=0
    local initialized=0
    local building=0
    local testing=0
    local fixing=0
    local completed=0
    local failed=0
    local needs_assistance=0

    shopt -s nullglob
    for file in "$builders_dir"/*.json; do
        if [[ -f "$file" ]]; then
            total=$((total + 1))
            status=$(jq -r '.status' "$file")

            case "$status" in
                initialized) initialized=$((initialized + 1)) ;;
                building) building=$((building + 1)) ;;
                testing) testing=$((testing + 1)) ;;
                fixing) fixing=$((fixing + 1)) ;;
                completed) completed=$((completed + 1)) ;;
                failed) failed=$((failed + 1)) ;;
            esac

            if [[ "$(jq -r '.needs_assistance' "$file")" == "true" ]]; then
                needs_assistance=$((needs_assistance + 1))
            fi
        fi
    done
    shopt -u nullglob

    cat <<EOF
{
  "total": $total,
  "initialized": $initialized,
  "building": $building,
  "testing": $testing,
  "fixing": $fixing,
  "completed": $completed,
  "failed": $failed,
  "needs_assistance": $needs_assistance
}
EOF
}

# ============================================================================
# Session Lifecycle
# ============================================================================

# Archive a completed session
# Args: session_id
archive_session() {
    local session_id="$1"
    local session_dir="$SESSIONS_DIR/$session_id"
    local archive_dir="$ARCHIVES_DIR/$session_id"

    if [[ ! -d "$session_dir" ]]; then
        echo "Error: Session not found: $session_id" >&2
        return 1
    fi

    # Move to archives
    mv "$session_dir" "$archive_dir"

    echo "Session archived: $archive_dir"
}

# Delete a session (cleanup)
# Args: session_id
delete_session() {
    local session_id="$1"
    local session_dir="$SESSIONS_DIR/$session_id"

    if [[ ! -d "$session_dir" ]]; then
        echo "Error: Session not found: $session_id" >&2
        return 1
    fi

    rm -rf "$session_dir"
    echo "Session deleted: $session_id"
}

# List all active sessions
list_sessions() {
    shopt -s nullglob
    for session_dir in "$SESSIONS_DIR"/proj-*/; do
        if [[ -d "$session_dir" ]]; then
            basename "$session_dir"
        fi
    done
    shopt -u nullglob
}

# Get most recent session
get_latest_session() {
    ( cd "$SESSIONS_DIR" 2>/dev/null && ls -td proj-*/ 2>/dev/null | head -1 | sed 's:/$::' )
}

# ============================================================================
# Helper Functions
# ============================================================================

# Pretty print context (for debugging)
# Args: session_id
show_context() {
    local session_id="$1"
    local context_file="$SESSIONS_DIR/$session_id/context.json"

    if [[ ! -f "$context_file" ]]; then
        echo "Error: Context file not found for session $session_id" >&2
        return 1
    fi

    echo "Session: $session_id"
    echo "----------------------------------------"
    jq '.' "$context_file"
}

# Pretty print builder state (for debugging)
# Args: session_id, builder_id
show_builder() {
    local session_id="$1"
    local builder_id="$2"
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"

    if [[ ! -f "$builder_file" ]]; then
        echo "Error: Builder state file not found: $builder_id" >&2
        return 1
    fi

    echo "Builder: $builder_id"
    echo "----------------------------------------"
    jq '.' "$builder_file"
}

# Export functions for use in other scripts
export -f create_session
export -f get_or_create_session
export -f session_exists
export -f get_session_dir
export -f save_context_field
export -f save_context
export -f load_context
export -f get_context_field
export -f update_phase
export -f init_builder_state
export -f update_builder_status
export -f save_builder_state
export -f load_builder_state
export -f get_builder_status
export -f builder_needs_assistance
export -f list_builders
export -f get_builder_statistics
export -f archive_session
export -f delete_session
export -f list_sessions
export -f get_latest_session
export -f show_context
export -f show_builder
