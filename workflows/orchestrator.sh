#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# orchestrator.sh - Workflow orchestration daemon
#
# Polls state file and dispatches phase handlers when action_owner="code"
# Runs as background daemon process
#
# Architecture:
#   - Polling-based (checks state every POLL_INTERVAL seconds)
#   - Dispatches to phase handlers in phases/ directory
#   - Uses action_needed field to determine which handler to call
#   - Exits gracefully on SIGTERM/SIGINT or when phase="storage"

# Get script directory (must be absolute for correct handler paths)
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source library modules (state management and logging)
source "$SCRIPT_DIR/lib/state_file.sh"
source "$SCRIPT_DIR/lib/logging_enhanced.sh"

# Configuration (readonly to prevent accidental modification)
readonly WORKFLOWS_DIR="$SCRIPT_DIR"
readonly POLL_INTERVAL=5

# Signal handling state (modified by trap handler)
CLEANUP_REQUESTED=0

#
# Signal handler for graceful shutdown
#
cleanup() {
    CLEANUP_REQUESTED=1
}

trap cleanup SIGTERM SIGINT

#
# Main function
#
main() {
    local project_path="$1"

    # Validate argument
    if [[ -z "$project_path" ]]; then
        echo "ERROR: Project path required" >&2
        return 1
    fi

    # Validate project directory
    if [[ ! -d "$project_path/.argo-project" ]]; then
        echo "ERROR: .argo-project not found in $project_path" >&2
        return 1
    fi

    # Change to project directory (all state operations are relative to this)
    cd "$project_path" || {
        echo "ERROR: Failed to change to project directory" >&2
        return 1
    }

    # PID file management (prevents multiple orchestrators for same project)
    local orchestrator_pid_file=".argo-project/orchestrator.pid"

    # Check if orchestrator already running
    if [[ -f "$orchestrator_pid_file" ]]; then
        local existing_pid
        existing_pid=$(cat "$orchestrator_pid_file" 2>/dev/null)
        if [[ -n "$existing_pid" ]] && ps -p "$existing_pid" >/dev/null 2>&1; then
            echo "WARNING: Orchestrator already running (PID $existing_pid)" >&2
            return 1
        fi
        # Stale PID file, remove it
        rm -f "$orchestrator_pid_file"
    fi

    # Write our PID for process tracking
    echo "$$" > "$orchestrator_pid_file"

    # Main orchestration loop (state-file relay pattern)
    # Polls state file, dispatches when action_owner="code", waits when "ci" or "user"
    while [[ $CLEANUP_REQUESTED -eq 0 ]]; do
        # Read current phase from state
        local phase
        phase=$(read_state "phase")

        # Exit condition: workflow has completed (reached storage phase)
        if [[ "$phase" == "storage" ]]; then
            break
        fi

        # State-file relay: check who owns the action
        local action_owner
        action_owner=$(read_state "action_owner")

        if [[ "$action_owner" == "code" ]]; then
            # Code's turn: dispatch to appropriate phase handler

            # Determine which handler to call (action_needed takes precedence over phase)
            local action_needed
            action_needed=$(read_state "action_needed")
            local handler_name="${action_needed:-$phase}"

            local phase_handler="$WORKFLOWS_DIR/phases/${handler_name}.sh"

            # Execute handler if it exists and is executable
            if [[ -f "$phase_handler" ]] && [[ -x "$phase_handler" ]]; then
                "$phase_handler" "$project_path"
                # Handler updates state and may change action_owner (relay pattern)
            fi
        fi
        # If action_owner is "ci" or "user", we wait for them to update state

        # Sleep between polls (responsive to signals via 1-second increments)
        for ((i=0; i<POLL_INTERVAL; i++)); do
            sleep 1
            [[ $CLEANUP_REQUESTED -eq 1 ]] && break
        done
    done

    # Cleanup
    rm -f "$orchestrator_pid_file"

    return 0
}

# Run main function
main "$@"
exit $?
