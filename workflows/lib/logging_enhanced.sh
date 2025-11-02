#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# logging_enhanced.sh - Enhanced logging with semantic tags
#
# Provides workflow logging functions with semantic tags for parsing.
# All logs write to .argo-project/logs/workflow.log and selective functions
# write to additional outputs (stdout, state, builder-specific logs).

# Default log directory
LOG_DIR="${LOG_DIR:-.argo-project/logs}"
WORKFLOW_LOG="$LOG_DIR/workflow.log"

#
# ensure_log_dir - Create log directory if it doesn't exist
#
ensure_log_dir() {
    if [[ ! -d "$LOG_DIR" ]]; then
        mkdir -p "$LOG_DIR" 2>/dev/null
    fi
}

#
# get_timestamp - Get ISO 8601 timestamp
#
get_timestamp() {
    date -u +"%Y-%m-%dT%H:%M:%SZ"
}

#
# write_log - Write message to workflow log
#
# Arguments:
#   tag - Log tag (e.g., PROGRESS, CHECKPOINT)
#   message - Log message
#
write_log() {
    local tag="$1"
    local message="$2"

    ensure_log_dir

    local timestamp=$(get_timestamp)
    echo "[$timestamp] [$tag] $message" >> "$WORKFLOW_LOG"
}

#
# progress - Log progress update (triple output: screen, log, state)
#
# Arguments:
#   builder_id - Builder identifier
#   percent - Progress percentage (0-100)
#   message - Human-readable status message
#
# Returns: 0 always
#
progress() {
    local builder_id="$1"
    local percent="$2"
    local message="$3"

    # Log to file
    write_log "PROGRESS" "[$builder_id] ${percent}% - $message"

    # Output to stdout
    echo "[$builder_id] ${percent}% - $message"

    # TODO: Update state.json (requires jq path manipulation for arrays)
    # For now, state updates are handled by caller if needed

    return 0
}

#
# log_checkpoint - Log checkpoint save event
#
# Arguments:
#   name - Checkpoint name
#   description - Human-readable description
#
# Returns: 0 always
#
log_checkpoint() {
    local name="$1"
    local description="$2"

    write_log "CHECKPOINT" "Saved: $name - $description"
    return 0
}

#
# log_user_query - Log user query event
#
# Arguments:
#   question - Question posed to user
#
# Returns: 0 always
#
log_user_query() {
    local question="$1"

    write_log "USER_QUERY" "$question"
    return 0
}

#
# log_builder - Log builder-specific event
#
# Arguments:
#   builder_id - Builder identifier
#   event - Event type (e.g., "started", "completed", "failed")
#   message - Event details
#
# Returns: 0 always
#
log_builder() {
    local builder_id="$1"
    local event="$2"
    local message="$3"

    # Write to workflow log
    write_log "BUILDER" "[$builder_id] $event - $message"

    # Write to builder-specific log
    ensure_log_dir
    local builder_log="$LOG_DIR/builder-${builder_id}.log"
    local timestamp=$(get_timestamp)
    echo "[$timestamp] $event - $message" >> "$builder_log"

    return 0
}

#
# log_merge - Log merge operation event
#
# Arguments:
#   event - Event type (e.g., "started", "conflict", "completed")
#   message - Event details
#
# Returns: 0 always
#
log_merge() {
    local event="$1"
    local message="$2"

    write_log "MERGE" "$event - $message"
    return 0
}

#
# log_state_transition - Log workflow phase transition
#
# Arguments:
#   from_phase - Previous phase
#   to_phase - New phase
#
# Returns: 0 always
#
log_state_transition() {
    local from_phase="$1"
    local to_phase="$2"

    write_log "STATE" "Phase transition: $from_phase -> $to_phase"
    return 0
}

# Export functions
export -f ensure_log_dir
export -f get_timestamp
export -f write_log
export -f progress
export -f log_checkpoint
export -f log_user_query
export -f log_builder
export -f log_merge
export -f log_state_transition
