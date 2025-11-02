#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# orchestrator.sh - Main workflow orchestration daemon
#
# Usage: ./orchestrator.sh <project_path>
#
# Polls state.json and dispatches to phase handlers when action_owner="code"
# Runs as background daemon until phase reaches "storage"

# Get script directory (absolute path, save before any cd)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKFLOWS_DIR="$SCRIPT_DIR"  # Save absolute path for phase handlers

# Source library modules
source "$SCRIPT_DIR/lib/state_file.sh"
source "$SCRIPT_DIR/lib/logging_enhanced.sh"

# Poll interval (seconds)
POLL_INTERVAL=5

# Cleanup flag
CLEANUP_REQUESTED=0

#
# cleanup - Cleanup on exit
#
cleanup() {
    CLEANUP_REQUESTED=1

    if [[ -n "$PROJECT_PATH" ]] && [[ -f "$PROJECT_PATH/.argo-project/orchestrator.pid" ]]; then
        rm -f "$PROJECT_PATH/.argo-project/orchestrator.pid"
    fi

    exit 0
}

#
# Signal handlers
#
trap cleanup SIGTERM SIGINT

#
# Main orchestrator loop
#
main() {
    local project_path="$1"

    # Validate argument
    if [[ -z "$project_path" ]]; then
        echo "ERROR: Project path required" >&2
        echo "Usage: $0 <project_path>" >&2
        return 1
    fi

    # Convert to absolute path
    project_path=$(cd "$project_path" 2>/dev/null && pwd)
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Invalid project path: $project_path" >&2
        return 1
    fi

    # Export for cleanup
    export PROJECT_PATH="$project_path"

    # Validate .argo-project exists
    if [[ ! -d "$project_path/.argo-project" ]]; then
        echo "ERROR: .argo-project not found in $project_path" >&2
        return 1
    fi

    # Validate state.json exists
    if [[ ! -f "$project_path/.argo-project/state.json" ]]; then
        echo "ERROR: state.json not found" >&2
        return 1
    fi

    # Change to project directory
    cd "$project_path"

    # Main polling loop
    while [[ $CLEANUP_REQUESTED -eq 0 ]]; do
        # Check current phase (exit if storage)
        local phase=$(read_state "phase")
        if [[ "$phase" == "storage" ]]; then
            echo "Workflow complete (phase=storage)"
            break
        fi

        # Check action owner
        local action_owner=$(read_state "action_owner")

        if [[ "$action_owner" == "code" ]]; then
            # It's our turn - dispatch to phase handler
            # Use absolute path to workflows directory
            local phase_handler="$WORKFLOWS_DIR/phases/${phase}.sh"

            if [[ -f "$phase_handler" ]] && [[ -x "$phase_handler" ]]; then
                echo "[$(date -u +"%Y-%m-%dT%H:%M:%SZ")] Executing phase: $phase"

                # Execute phase handler
                "$phase_handler" "$project_path"
                local result=$?

                if [[ $result -ne 0 ]]; then
                    echo "ERROR: Phase handler failed: $phase (exit code: $result)" >&2
                    # Don't exit - let error be handled by state
                fi
            else
                echo "ERROR: Phase handler not found or not executable: $phase_handler" >&2
                sleep $POLL_INTERVAL
                continue
            fi
        else
            # CI is working - wait
            : # no-op, just poll
        fi

        # Sleep between polls (in shorter intervals to be more responsive to signals)
        for ((i=0; i<POLL_INTERVAL; i++)); do
            sleep 1
            [[ $CLEANUP_REQUESTED -eq 1 ]] && break
        done
    done

    cleanup
    return 0
}

# Run main function
main "$@"
exit $?
