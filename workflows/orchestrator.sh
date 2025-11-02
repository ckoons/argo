#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# orchestrator.sh - Workflow orchestration daemon
#
# Polls state file and dispatches phase handlers when action_owner="code"
# Runs as background daemon process

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source library modules
source "$SCRIPT_DIR/lib/state_file.sh"
source "$SCRIPT_DIR/lib/logging_enhanced.sh"

# Save workflows directory before any cd operations
WORKFLOWS_DIR="$SCRIPT_DIR"

# Polling interval in seconds
POLL_INTERVAL=5

# Cleanup flag for signal handling
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

    cd "$project_path"

    # Create PID file
    local orchestrator_pid_file=".argo-project/orchestrator.pid"

    # Check if orchestrator already running
    if [[ -f "$orchestrator_pid_file" ]]; then
        local existing_pid=$(cat "$orchestrator_pid_file")
        if ps -p "$existing_pid" >/dev/null 2>&1; then
            echo "WARNING: Orchestrator already running (PID $existing_pid)" >&2
            return 1
        fi
        # Stale PID file, remove it
        rm -f "$orchestrator_pid_file"
    fi

    # Write our PID
    echo "$$" > "$orchestrator_pid_file"

    # Main orchestration loop
    while [[ $CLEANUP_REQUESTED -eq 0 ]]; do
        # Read current phase
        local phase=$(read_state "phase")

        # Exit if workflow is in storage phase (complete)
        if [[ "$phase" == "storage" ]]; then
            break
        fi

        # Check if code should act
        local action_owner=$(read_state "action_owner")

        if [[ "$action_owner" == "code" ]]; then
            # Dispatch based on action_needed (or phase name for direct mapping)
            local action_needed=$(read_state "action_needed")
            local handler_name="${action_needed:-$phase}"

            local phase_handler="$WORKFLOWS_DIR/phases/${handler_name}.sh"

            if [[ -f "$phase_handler" ]] && [[ -x "$phase_handler" ]]; then
                # Execute phase handler
                "$phase_handler" "$project_path"
            fi
        fi

        # Sleep with responsive signal checking
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
