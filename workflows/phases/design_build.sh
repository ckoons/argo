#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# design_build.sh - Decomposition and builder setup phase
#
# Validates design components and prepares for parallel execution
# Creates execution.builders[] and git worktrees
# Transitions from design -> execution

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source library modules
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"

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

    echo "=== design_build phase ==="

    # Read design from state
    local design=$(jq -r '.design' .argo-project/state.json 2>/dev/null)
    if [[ -z "$design" ]] || [[ "$design" == "null" ]]; then
        echo "ERROR: No design in state" >&2
        update_state "error_recovery.last_error" "Missing design"
        return 1
    fi

    # Read components
    local components=$(echo "$design" | jq -r '.components')
    if [[ -z "$components" ]] || [[ "$components" == "null" ]]; then
        echo "ERROR: No components in design" >&2
        update_state "error_recovery.last_error" "Missing components"
        return 1
    fi

    local component_count=$(echo "$components" | jq 'length')
    echo "Components to build: $component_count"

    # Create builders array
    local builders_json="[]"
    local timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

    # For each component, create a builder entry
    for ((i=0; i<component_count; i++)); do
        local component=$(echo "$components" | jq -r ".[$i]")
        local builder_id=$(echo "$component" | jq -r '.id')
        local description=$(echo "$component" | jq -r '.description')

        echo "Setting up builder: $builder_id"

        # Create builder entry
        local builder=$(jq -n \
            --arg id "$builder_id" \
            --arg desc "$description" \
            --arg status "pending" \
            --arg created "$timestamp" \
            '{
                id: $id,
                description: $desc,
                status: $status,
                worktree: null,
                branch: ("argo-builder-" + $id),
                pid: null,
                progress: 0,
                started: null,
                completed: null,
                created: $created
            }')

        builders_json=$(echo "$builders_json" | jq --argjson builder "$builder" '. += [$builder]')

        # Create git worktree (if git repo exists)
        if git rev-parse --git-dir >/dev/null 2>&1; then
            local branch="argo-builder-${builder_id}"
            local worktree_path=".argo-project/builders/${builder_id}"

            # Create worktree
            echo "Creating git worktree: $worktree_path"
            mkdir -p ".argo-project/builders"

            git worktree add "$worktree_path" -b "$branch" 2>/dev/null || {
                # Branch might exist, try without -b
                git worktree add "$worktree_path" "$branch" 2>/dev/null || {
                    echo "WARNING: Could not create worktree for $builder_id"
                }
            }

            # Update builder with worktree path if successful
            if [[ -d "$worktree_path" ]]; then
                local abs_worktree=$(cd "$worktree_path" && pwd)
                builders_json=$(echo "$builders_json" | jq \
                    --arg id "$builder_id" \
                    --arg wt "$abs_worktree" \
                    'map(if .id == $id then .worktree = $wt else . end)')
            fi
        else
            echo "WARNING: Not a git repository, skipping worktree creation"
        fi
    done

    # Save builders to state
    echo "Saving builders to state..."
    local temp_file=".argo-project/state.json.tmp"

    jq --argjson builders "$builders_json" \
       '.execution = {
           builders: $builders,
           status: "pending",
           started: null,
           completed: null
       }' .argo-project/state.json > "$temp_file"

    if [[ $? -eq 0 ]]; then
        mv "$temp_file" .argo-project/state.json
    else
        echo "ERROR: Failed to update state with builders" >&2
        rm -f "$temp_file"
        return 1
    fi

    # Log phase transition
    log_state_transition "design" "execution"

    # Transition to execution phase
    update_state "phase" "execution"
    update_state "action_needed" "spawn_builders"

    # Keep action_owner as "code" (orchestrator will spawn builders)
    # Don't update action_owner here

    echo "design_build phase complete. Ready for execution..."
    echo ""

    return 0
}

# Run main function
main "$@"
exit $?
