#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# merge_build.sh - Branch merging and workflow completion phase
#
# Merges builder branches back to main, handles conflicts
# Cleans up worktrees and transitions to storage
# Transitions from merge_build -> storage

# Get script directory
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source library modules
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"

# CI tool command (allow override for testing)
readonly CI_TOOL="${CI_COMMAND:-ci}"

#
# merge_builder_branch - Merge a builder branch into main
#
merge_builder_branch() {
    local builder_id="$1"
    local branch="$2"

    log_builder "$builder_id" "merging" "Merging branch $branch"

    # Check if branch exists
    if ! git rev-parse --verify "$branch" >/dev/null 2>&1; then
        log_builder "$builder_id" "warning" "Branch $branch not found"
        return 1
    fi

    # Attempt merge
    if git merge --no-ff -m "Merge builder: $builder_id" "$branch" >/dev/null 2>&1; then
        log_builder "$builder_id" "merged" "Successfully merged"
        return 0
    else
        # Merge conflict detected
        log_builder "$builder_id" "conflict" "Merge conflict detected"

        # Abort the merge
        git merge --abort 2>/dev/null

        return 2  # Conflict indicator
    fi
}

#
# cleanup_builder_worktree - Remove builder worktree and branch
#
cleanup_builder_worktree() {
    local builder_id="$1"
    local worktree="$2"
    local branch="$3"

    log_builder "$builder_id" "cleanup" "Removing worktree and branch"

    # Remove worktree if it exists
    if [[ -d "$worktree" ]]; then
        git worktree remove "$worktree" --force 2>/dev/null || {
            # If worktree remove fails, just delete the directory
            rm -rf "$worktree" 2>/dev/null
        }
    fi

    # Delete branch (if not already deleted)
    if git rev-parse --verify "$branch" >/dev/null 2>&1; then
        git branch -D "$branch" >/dev/null 2>&1
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

    echo "=== merge_build phase ==="

    # Verify we're in a git repository
    if ! git rev-parse --git-dir >/dev/null 2>&1; then
        echo "ERROR: Not a git repository" >&2
        return 1
    fi

    # Read builders from state
    local execution=$(jq -r '.execution' .argo-project/state.json 2>/dev/null)
    if [[ -z "$execution" ]] || [[ "$execution" == "null" ]]; then
        echo "ERROR: No execution in state" >&2
        return 1
    fi

    local builders=$(echo "$execution" | jq -r '.builders')
    local builder_count=$(echo "$builders" | jq 'length')

    if [[ $builder_count -eq 0 ]]; then
        echo "No builders to merge, transitioning to storage..."
        update_state "phase" "storage"
        update_state "action_owner" "user"
        return 0
    fi

    echo "Merging $builder_count builder branches..."

    # Ensure we're on main branch
    local current_branch=$(git rev-parse --abbrev-ref HEAD)
    if [[ "$current_branch" != "main" ]] && [[ "$current_branch" != "master" ]]; then
        echo "Switching to main branch..."
        git checkout main 2>/dev/null || git checkout master 2>/dev/null || {
            echo "ERROR: Could not checkout main/master branch" >&2
            return 1
        }
    fi

    # Track conflicts
    local has_conflicts=false
    local conflict_builders=()

    # Merge each builder branch
    for ((i=0; i<builder_count; i++)); do
        local builder=$(echo "$builders" | jq -r ".[$i]")
        local builder_id=$(echo "$builder" | jq -r '.id')
        local branch=$(echo "$builder" | jq -r '.branch')
        local worktree=$(echo "$builder" | jq -r '.worktree')
        local status=$(echo "$builder" | jq -r '.status')

        # Only merge completed builders
        if [[ "$status" != "completed" ]]; then
            log_builder "$builder_id" "skipped" "Not completed (status: $status)"
            continue
        fi

        echo "Merging builder: $builder_id"

        # Attempt merge
        merge_builder_branch "$builder_id" "$branch"
        local merge_result=$?

        if [[ $merge_result -eq 0 ]]; then
            # Success - clean up worktree
            cleanup_builder_worktree "$builder_id" "$worktree" "$branch"
        elif [[ $merge_result -eq 2 ]]; then
            # Conflict detected
            has_conflicts=true
            conflict_builders+=("$builder_id")
            log_builder "$builder_id" "conflict" "Merge conflict requires resolution"
        else
            # Branch not found or other error
            log_builder "$builder_id" "error" "Merge failed"
        fi
    done

    # Handle conflicts
    if [[ $has_conflicts == true ]]; then
        echo ""
        echo "Merge conflicts detected in builders: ${conflict_builders[*]}"
        echo "Conflicts require manual resolution or CI assistance."
        echo ""

        # Set action_needed for conflict resolution
        update_state "action_needed" "resolve_conflicts"
        update_state "action_owner" "ci"

        # Save conflict list to state
        local conflicts_json=$(printf '%s\n' "${conflict_builders[@]}" | jq -R . | jq -s .)
        local temp_file=".argo-project/state.json.tmp"
        jq --argjson conflicts "$conflicts_json" '.merge_conflicts = $conflicts' \
           .argo-project/state.json > "$temp_file" && mv "$temp_file" .argo-project/state.json

        echo "Workflow paused for conflict resolution."
        return 0
    fi

    echo "All builders merged successfully!"

    # Log phase transition
    log_state_transition "merge_build" "storage"

    # Transition to storage phase (workflow complete)
    update_state "phase" "storage"
    update_state "action_owner" "user"
    update_state "action_needed" "none"

    echo "merge_build phase complete. Workflow finished!"
    echo ""

    return 0
}

# Run main function
main "$@"
exit $?
