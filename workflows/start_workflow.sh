#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# start_workflow.sh - Start Argo workflow execution
#
# Usage: ./start_workflow.sh <user_query> [project_path]
#
# Initializes workflow state and launches orchestrator daemon
# Sets initial phase to "init" and action_owner to "code"
# Orchestrator runs as background daemon process

# Get script directory (needed for orchestrator path)
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source library modules
source "$SCRIPT_DIR/lib/state_file.sh"
source "$SCRIPT_DIR/lib/logging_enhanced.sh"

#
# Main function
#
main() {
    local user_query="$1"
    local project_path="${2:-.}"

    # Validate arguments
    if [[ -z "$user_query" ]]; then
        echo "ERROR: User query required" >&2
        echo "Usage: $0 <user_query> [project_path]" >&2
        echo "" >&2
        echo "Example:" >&2
        echo "  $0 \"Build a CLI calculator with add/subtract/multiply\"" >&2
        return 1
    fi

    # Convert to absolute path and verify (absolute paths required for daemon)
    if [[ "$project_path" != "." ]]; then
        if ! project_path=$(cd "$project_path" 2>/dev/null && pwd); then
            echo "ERROR: Invalid project path: $2" >&2
            return 1
        fi
    else
        project_path=$(pwd)
    fi

    # Change to project directory (needed for state operations)
    cd "$project_path" || {
        echo "ERROR: Failed to change to project directory" >&2
        return 1
    }

    # Verify .argo-project exists
    if [[ ! -d .argo-project ]]; then
        echo "ERROR: .argo-project directory not found" >&2
        echo "Run project_setup.sh first to initialize the project" >&2
        return 1
    fi

    # Verify state.json exists
    if [[ ! -f .argo-project/state.json ]]; then
        echo "ERROR: state.json not found" >&2
        echo "Project may be corrupted. Re-run project_setup.sh" >&2
        return 1
    fi

    echo "Starting Argo workflow..."
    echo "Project: $project_path"
    echo "Query: $user_query"
    echo ""

    # Save user query to state
    update_state "user_query" "$user_query"
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to save user query" >&2
        return 1
    fi

    # Set initial workflow state
    update_state "phase" "init"
    update_state "action_owner" "code"
    update_state "action_needed" "design_program"

    # Log workflow start
    log_state_transition "idle" "init"

    # Check if orchestrator is already running
    local orchestrator_pid_file=".argo-project/orchestrator.pid"
    if [[ -f "$orchestrator_pid_file" ]]; then
        local old_pid=$(cat "$orchestrator_pid_file")
        if ps -p "$old_pid" >/dev/null 2>&1; then
            echo "WARNING: Orchestrator already running (PID $old_pid)" >&2
            echo "Stopping previous instance..."
            kill "$old_pid" 2>/dev/null || true
            sleep 1
        fi
        rm -f "$orchestrator_pid_file"
    fi

    # Launch orchestrator in background
    "$SCRIPT_DIR/orchestrator.sh" "$project_path" >/dev/null 2>&1 &
    local orchestrator_pid=$!

    # Save PID
    echo "$orchestrator_pid" > "$orchestrator_pid_file"

    echo "Workflow started!"
    echo "Orchestrator PID: $orchestrator_pid"
    echo ""
    echo "Monitor progress:"
    echo "  ./workflows/status.sh"
    echo "  tail -f .argo-project/logs/workflow.log"
    echo ""
    echo "Stop workflow:"
    echo "  ./workflows/stop_workflow.sh"

    return 0
}

# Run main function
main "$@"
exit $?
