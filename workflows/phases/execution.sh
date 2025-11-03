#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# execution.sh - Parallel builder execution phase
#
# Spawns CI sessions for each builder in git worktrees
# Monitors progress and waits for completion
# Transitions from execution -> merge_build

# Get script directory
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source library modules
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"

# CI tool command (allow override for testing)
readonly CI_TOOL="${CI_COMMAND:-ci}"

# Poll interval for monitoring builders
MONITOR_INTERVAL=5

#
# spawn_builder - Launch CI session for a builder
#
spawn_builder() {
    local builder_id="$1"
    local worktree="$2"
    local description="$3"

    log_builder "$builder_id" "starting" "Spawning builder process"

    # Build prompt for CI
    local prompt="You are building the '$builder_id' component.

Description: $description

Your task:
1. Implement the component according to the design
2. Write tests
3. Ensure all tests pass
4. Update state when complete

Work in the directory: $worktree
Commit your changes when done."

    # Spawn CI in background (simplified for Sprint 2)
    # In full implementation, this would use proper CI session management
    if command -v "$CI_TOOL" >/dev/null 2>&1; then
        (
            cd "$worktree" 2>/dev/null || exit 1
            "$CI_TOOL" "$prompt" >/dev/null 2>&1
            echo $? > ".build_result"
        ) &
        local pid=$!

        echo "$pid"
    else
        echo "ERROR: CI tool not found: $CI_TOOL" >&2
        return 1
    fi
}

#
# check_builder_status - Check if builder is still running
#
check_builder_status() {
    local pid="$1"

    if [[ -z "$pid" ]] || [[ "$pid" == "null" ]]; then
        return 1  # No PID means not running
    fi

    if ps -p "$pid" >/dev/null 2>&1; then
        return 0  # Still running
    else
        return 1  # Completed or failed
    fi
}

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

    cd "$project_path" || {
        echo "ERROR: Failed to change to project directory" >&2
        return 1
    }

    echo "=== execution phase ==="

    # Read builders from state
    local execution=$(jq -r '.execution' .argo-project/state.json 2>/dev/null)
    if [[ -z "$execution" ]] || [[ "$execution" == "null" ]]; then
        echo "ERROR: No execution in state" >&2
        return 1
    fi

    local builders=$(echo "$execution" | jq -r '.builders')
    local builder_count=$(echo "$builders" | jq 'length')

    if [[ $builder_count -eq 0 ]]; then
        echo "No builders to execute, transitioning to merge..."
        update_state "phase" "merge_build"
        return 0
    fi

    echo "Spawning $builder_count builders..."

    # Spawn all builders
    local pids=()
    for ((i=0; i<builder_count; i++)); do
        local builder=$(echo "$builders" | jq -r ".[$i]")
        local builder_id=$(echo "$builder" | jq -r '.id')
        local worktree=$(echo "$builder" | jq -r '.worktree')
        local description=$(echo "$builder" | jq -r '.description')

        echo "Spawning builder: $builder_id"

        # Update status to running
        local temp_file=".argo-project/state.json.tmp"
        jq --arg id "$builder_id" \
           --arg status "running" \
           --arg started "$(date -u +"%Y-%m-%dT%H:%M:%SZ")" \
           '(.execution.builders[] | select(.id == $id)) |= (.status = $status | .started = $started)' \
           .argo-project/state.json > "$temp_file" && mv "$temp_file" .argo-project/state.json

        # Spawn builder
        local pid=$(spawn_builder "$builder_id" "$worktree" "$description")
        if [[ -n "$pid" ]]; then
            pids+=("$pid")

            # Save PID to state
            jq --arg id "$builder_id" \
               --arg pid "$pid" \
               '(.execution.builders[] | select(.id == $id)) |= (.pid = $pid)' \
               .argo-project/state.json > "$temp_file" && mv "$temp_file" .argo-project/state.json

            log_builder "$builder_id" "spawned" "PID: $pid"
        else
            log_builder "$builder_id" "failed" "Could not spawn builder"
        fi
    done

    echo "All builders spawned. Monitoring progress..."

    # Monitor builders (simplified - just wait for PIDs)
    local all_complete=false
    while [[ $all_complete == false ]]; do
        sleep $MONITOR_INTERVAL

        all_complete=true
        for ((i=0; i<builder_count; i++)); do
            local builder=$(jq -r ".execution.builders[$i]" .argo-project/state.json)
            local builder_id=$(echo "$builder" | jq -r '.id')
            local pid=$(echo "$builder" | jq -r '.pid')
            local status=$(echo "$builder" | jq -r '.status')

            if [[ "$status" == "running" ]]; then
                # Check if still running
                if ! check_builder_status "$pid"; then
                    # Builder completed
                    echo "Builder $builder_id completed"
                    log_builder "$builder_id" "completed" "Process finished"

                    jq --arg id "$builder_id" \
                       --arg completed "$(date -u +"%Y-%m-%dT%H:%M:%SZ")" \
                       '(.execution.builders[] | select(.id == $id)) |= (.status = "completed" | .completed = $completed | .progress = 100)' \
                       .argo-project/state.json > "$temp_file" && mv "$temp_file" .argo-project/state.json
                else
                    all_complete=false
                fi
            fi
        done
    done

    echo "All builders complete!"

    # Log phase transition
    log_state_transition "execution" "merge_build"

    # Transition to merge phase
    update_state "phase" "merge_build"
    update_state "execution.status" "completed"
    update_state "action_needed" "merge_branches"

    echo "execution phase complete. Ready for merge..."
    echo ""

    return 0
}

# Run main function
main "$@"
exit $?
