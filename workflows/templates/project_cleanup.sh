#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# project_cleanup.sh - Cleanup build artifacts and workspace
#
# Usage: ./project_cleanup.sh [project_path] [cleanup_type]
#
# Cleanup types:
#   - partial: Delete builders/, temp files, temp branches (keep logs/checkpoints)
#   - full: Delete builders/, logs/ (keep checkpoints)
#   - nuclear: Delete entire .argo-project/

#
# Main cleanup function
#
main() {
    local project_path="${1:-.}"  # Default to current directory
    local cleanup_type="${2:-partial}"  # Default to partial

    # Convert to absolute path
    if [[ "$project_path" != "." ]]; then
        project_path=$(cd "$project_path" 2>/dev/null && pwd)
        if [[ $? -ne 0 ]]; then
            echo "ERROR: Invalid project path: $project_path" >&2
            return 1
        fi
    else
        project_path=$(pwd)
    fi

    cd "$project_path"

    # Check if .argo-project exists
    if [[ ! -d .argo-project ]]; then
        echo "No .argo-project directory found in $project_path"
        return 0  # Not an error - just nothing to clean
    fi

    echo "Cleaning up Argo project: $project_path"
    echo "Cleanup type: $cleanup_type"

    case "$cleanup_type" in
        partial)
            cleanup_partial
            ;;
        full)
            cleanup_full
            ;;
        nuclear)
            cleanup_nuclear
            ;;
        *)
            echo "ERROR: Invalid cleanup type: $cleanup_type" >&2
            echo "Valid types: partial, full, nuclear" >&2
            return 1
            ;;
    esac

    return $?
}

#
# cleanup_partial - Delete builders and temp files, keep logs and checkpoints
#
cleanup_partial() {
    echo "Partial cleanup:"
    echo "  - Removing builders/"
    echo "  - Removing temp files"
    echo "  - Keeping logs/ and checkpoints/"

    # Remove builders directory
    if [[ -d .argo-project/builders ]]; then
        rm -rf .argo-project/builders
        if [[ $? -ne 0 ]]; then
            echo "ERROR: Failed to remove builders/" >&2
            return 1
        fi
    fi

    # Remove temp files (*.tmp, *.lock)
    find .argo-project -name "*.tmp" -delete 2>/dev/null
    find .argo-project -name "*.lock" -delete 2>/dev/null

    # Remove git worktrees if any exist
    if [[ -f .argo-project/worktrees.txt ]]; then
        while IFS= read -r worktree; do
            if [[ -n "$worktree" ]]; then
                git worktree remove "$worktree" 2>/dev/null || true
            fi
        done < .argo-project/worktrees.txt
        rm -f .argo-project/worktrees.txt
    fi

    # Remove temp branches (argo-builder-*)
    git branch | grep "argo-builder-" | xargs -r git branch -D 2>/dev/null || true

    echo "Partial cleanup complete"
    return 0
}

#
# cleanup_full - Delete builders and logs, keep checkpoints
#
cleanup_full() {
    echo "Full cleanup:"
    echo "  - Removing builders/"
    echo "  - Removing logs/"
    echo "  - Keeping checkpoints/"

    # First do partial cleanup (removes builders, temp files, worktrees)
    cleanup_partial
    if [[ $? -ne 0 ]]; then
        return 1
    fi

    # Additionally remove logs
    echo "  - Removing logs/"
    if [[ -d .argo-project/logs ]]; then
        rm -rf .argo-project/logs
        if [[ $? -ne 0 ]]; then
            echo "ERROR: Failed to remove logs/" >&2
            return 1
        fi
    fi

    echo "Full cleanup complete"
    return 0
}

#
# cleanup_nuclear - Delete entire .argo-project directory
#
cleanup_nuclear() {
    echo "Nuclear cleanup:"
    echo "  - Removing entire .argo-project/"
    echo ""
    echo "WARNING: This will delete ALL project data including:"
    echo "  - State files"
    echo "  - Configuration"
    echo "  - Logs"
    echo "  - Checkpoints"
    echo "  - Build artifacts"

    # Remove git worktrees before nuking
    if [[ -f .argo-project/worktrees.txt ]]; then
        while IFS= read -r worktree; do
            if [[ -n "$worktree" ]]; then
                git worktree remove "$worktree" 2>/dev/null || true
            fi
        done < .argo-project/worktrees.txt
    fi

    # Remove temp branches
    git branch | grep "argo-builder-" | xargs -r git branch -D 2>/dev/null || true

    # Remove entire directory
    rm -rf .argo-project
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to remove .argo-project/" >&2
        return 1
    fi

    echo "Nuclear cleanup complete"
    echo "Project can be re-initialized with project_setup.sh"
    return 0
}

# Run main function
main "$@"
exit $?
