#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# status.sh - Show current workflow status
#
# Usage: ./status.sh [project_path]
#
# Displays current phase, action owner, and progress

# Get script directory
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source library modules
source "$SCRIPT_DIR/lib/state_file.sh"

#
# Main function
#
main() {
    local project_path="${1:-.}"

    # Convert to absolute path
    if [[ "$project_path" != "." ]]; then
        project_path=$(cd "$project_path" 2>/dev/null && pwd)
        if [[ $? -ne 0 ]]; then
            echo "ERROR: Invalid project path" >&2
            return 1
        fi
    else
        project_path=$(pwd)
    fi

    cd "$project_path" || {
        echo "ERROR: Failed to change to project directory" >&2
        return 1
    }

    # Verify .argo-project exists
    if [[ ! -d .argo-project ]]; then
        echo "ERROR: .argo-project directory not found" >&2
        echo "Run project_setup.sh first" >&2
        return 1
    fi

    # Read state
    local project_name=$(read_state "project_name")
    local phase=$(read_state "phase")
    local action_owner=$(read_state "action_owner")
    local action_needed=$(read_state "action_needed")
    local last_update=$(read_state "last_update")
    local user_query=$(read_state "user_query")

    # Display status
    echo "=========================================="
    echo "Argo Workflow Status"
    echo "=========================================="
    echo ""
    echo "Project: $project_name"
    echo "Path: $project_path"
    echo ""
    echo "Current Phase: $phase"
    echo "Action Owner: $action_owner"
    echo "Action Needed: $action_needed"
    echo "Last Update: $last_update"
    echo ""
    if [[ -n "$user_query" ]] && [[ "$user_query" != "null" ]]; then
        echo "User Query: $user_query"
        echo ""
    fi

    # Check if orchestrator is running
    if [[ -f .argo-project/orchestrator.pid ]]; then
        local orch_pid=$(cat .argo-project/orchestrator.pid)
        if ps -p "$orch_pid" >/dev/null 2>&1; then
            echo "Orchestrator: Running (PID $orch_pid)"
        else
            echo "Orchestrator: Not running (stale PID file)"
        fi
    else
        echo "Orchestrator: Not running"
    fi

    echo ""
    echo "=========================================="

    return 0
}

# Run main function
main "$@"
exit $?
