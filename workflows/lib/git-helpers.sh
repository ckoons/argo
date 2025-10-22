#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# git-helpers.sh - Git operations for Argo workflows
#
# Provides wrapper functions around git and gh (GitHub CLI) for
# branch management, merging, conflict detection, and GitHub operations.

# ============================================================================
# Branch Management
# ============================================================================

# Create a new branch and optionally push to remote
# Args: branch_name, [base_branch], [push_to_remote]
# Returns: 0 on success, 1 on failure
git_create_branch() {
    local branch_name="$1"
    local base_branch="${2:-HEAD}"
    local push_remote="${3:-no}"

    if [[ -z "$branch_name" ]]; then
        echo "Error: Branch name required" >&2
        return 1
    fi

    # Check if branch already exists
    if git rev-parse --verify "$branch_name" >/dev/null 2>&1; then
        echo "Branch already exists: $branch_name" >&2
        return 1
    fi

    # Create branch
    if ! git checkout -b "$branch_name" "$base_branch"; then
        echo "Error: Failed to create branch $branch_name" >&2
        return 1
    fi

    # Push to remote if requested
    if [[ "$push_remote" == "yes" ]]; then
        if ! git push -u origin "$branch_name"; then
            echo "Error: Failed to push branch $branch_name" >&2
            return 1
        fi
    fi

    return 0
}

# Delete a branch locally and optionally from remote
# Args: branch_name, [delete_remote]
# Returns: 0 on success, 1 on failure
git_delete_branch() {
    local branch_name="$1"
    local delete_remote="${2:-no}"

    if [[ -z "$branch_name" ]]; then
        echo "Error: Branch name required" >&2
        return 1
    fi

    # Don't allow deleting current branch
    local current_branch=$(git rev-parse --abbrev-ref HEAD)
    if [[ "$current_branch" == "$branch_name" ]]; then
        echo "Error: Cannot delete current branch. Checkout another branch first." >&2
        return 1
    fi

    # Delete local branch
    if git rev-parse --verify "$branch_name" >/dev/null 2>&1; then
        if ! git branch -D "$branch_name"; then
            echo "Error: Failed to delete local branch $branch_name" >&2
            return 1
        fi
    fi

    # Delete remote branch if requested
    if [[ "$delete_remote" == "yes" ]]; then
        if git ls-remote --heads origin "$branch_name" | grep -q "$branch_name"; then
            if ! git push origin --delete "$branch_name" 2>/dev/null; then
                echo "Warning: Failed to delete remote branch $branch_name" >&2
            fi
        fi
    fi

    return 0
}

# Check if branch exists (local or remote)
# Args: branch_name, [check_remote]
# Returns: 0 if exists, 1 if not
git_branch_exists() {
    local branch_name="$1"
    local check_remote="${2:-no}"

    # Check local
    if git rev-parse --verify "$branch_name" >/dev/null 2>&1; then
        return 0
    fi

    # Check remote if requested
    if [[ "$check_remote" == "yes" ]]; then
        if git ls-remote --heads origin "$branch_name" | grep -q "$branch_name"; then
            return 0
        fi
    fi

    return 1
}

# Switch to a branch
# Args: branch_name
# Returns: 0 on success, 1 on failure
git_checkout_branch() {
    local branch_name="$1"

    if [[ -z "$branch_name" ]]; then
        echo "Error: Branch name required" >&2
        return 1
    fi

    if ! git checkout "$branch_name"; then
        echo "Error: Failed to checkout branch $branch_name" >&2
        return 1
    fi

    return 0
}

# Push branch to remote
# Args: branch_name, [force]
# Returns: 0 on success, 1 on failure
git_push_branch() {
    local branch_name="$1"
    local force="${2:-no}"

    if [[ -z "$branch_name" ]]; then
        echo "Error: Branch name required" >&2
        return 1
    fi

    # Check if on correct branch
    local current_branch=$(git rev-parse --abbrev-ref HEAD)
    if [[ "$current_branch" != "$branch_name" ]]; then
        echo "Error: Not on branch $branch_name (currently on $current_branch)" >&2
        return 1
    fi

    # Push
    if [[ "$force" == "yes" ]]; then
        if ! git push --force-with-lease origin "$branch_name"; then
            echo "Error: Failed to force push branch $branch_name" >&2
            return 1
        fi
    else
        if ! git push -u origin "$branch_name"; then
            echo "Error: Failed to push branch $branch_name" >&2
            return 1
        fi
    fi

    return 0
}

# ============================================================================
# Merge Operations
# ============================================================================

# Merge a branch with conflict detection
# Args: source_branch, [commit_message]
# Returns: 0 if clean merge, 1 if conflicts, 2 on error
git_merge_branch() {
    local source_branch="$1"
    local commit_message="${2:-Merge branch '$source_branch'}"

    if [[ -z "$source_branch" ]]; then
        echo "Error: Source branch required" >&2
        return 2
    fi

    # Verify source branch exists
    if ! git rev-parse --verify "$source_branch" >/dev/null 2>&1; then
        echo "Error: Source branch does not exist: $source_branch" >&2
        return 2
    fi

    # Attempt merge
    if git merge "$source_branch" --no-ff -m "$commit_message" 2>/dev/null; then
        return 0  # Clean merge
    fi

    # Check if there are conflicts
    if git_has_conflicts; then
        return 1  # Conflicts detected
    fi

    # Other error
    echo "Error: Merge failed for unknown reason" >&2
    return 2
}

# Check if there are merge conflicts
# Returns: 0 if conflicts exist, 1 if no conflicts
git_has_conflicts() {
    # Check for unmerged files
    if git diff --name-only --diff-filter=U | grep -q .; then
        return 0
    fi
    return 1
}

# Get list of conflicted files
# Returns: List of files with conflicts (one per line)
git_list_conflicts() {
    git diff --name-only --diff-filter=U
}

# Abort current merge
# Returns: 0 on success, 1 on failure
git_abort_merge() {
    if ! git merge --abort 2>/dev/null; then
        echo "Error: Failed to abort merge (or no merge in progress)" >&2
        return 1
    fi
    return 0
}

# Check if merge is in progress
# Returns: 0 if merge in progress, 1 if not
git_merge_in_progress() {
    [[ -f ".git/MERGE_HEAD" ]]
}

# ============================================================================
# Diff and Change Analysis
# ============================================================================

# Get number of commits between branches
# Args: base_branch, compare_branch
# Returns: commit count (as stdout)
git_get_commit_count() {
    local base_branch="$1"
    local compare_branch="$2"

    if [[ -z "$base_branch" ]] || [[ -z "$compare_branch" ]]; then
        echo "0"
        return 1
    fi

    git rev-list --count "$base_branch..$compare_branch" 2>/dev/null || echo "0"
}

# Get number of lines changed between branches
# Args: base_branch, compare_branch
# Returns: line count (as stdout)
git_get_lines_changed() {
    local base_branch="$1"
    local compare_branch="$2"

    if [[ -z "$base_branch" ]] || [[ -z "$compare_branch" ]]; then
        echo "0"
        return 1
    fi

    git diff "$base_branch..$compare_branch" | wc -l | tr -d ' '
}

# Get list of files changed between branches
# Args: base_branch, compare_branch
# Returns: List of changed files (one per line)
git_list_changed_files() {
    local base_branch="$1"
    local compare_branch="$2"

    if [[ -z "$base_branch" ]] || [[ -z "$compare_branch" ]]; then
        return 1
    fi

    git diff --name-only "$base_branch..$compare_branch"
}

# Get diff between branches
# Args: base_branch, compare_branch, [file]
# Returns: Diff output
git_get_diff() {
    local base_branch="$1"
    local compare_branch="$2"
    local file="${3:-}"

    if [[ -z "$base_branch" ]] || [[ -z "$compare_branch" ]]; then
        echo "Error: Both branches required" >&2
        return 1
    fi

    if [[ -n "$file" ]]; then
        git diff "$base_branch..$compare_branch" -- "$file"
    else
        git diff "$base_branch..$compare_branch"
    fi
}

# ============================================================================
# Conflict Analysis
# ============================================================================

# Extract conflict markers from a file
# Args: file_path
# Returns: 0 if conflicts found, 1 if none
git_has_conflict_markers() {
    local file="$1"

    if [[ ! -f "$file" ]]; then
        return 1
    fi

    grep -q "^<<<<<<< " "$file" && \
    grep -q "^=======" "$file" && \
    grep -q "^>>>>>>> " "$file"
}

# Count conflict markers in repository
# Returns: Number of conflicted files
git_count_conflicts() {
    git diff --name-only --diff-filter=U | wc -l | tr -d ' '
}

# Get conflict details for a file
# Args: file_path
# Returns: JSON with conflict information
git_analyze_conflict() {
    local file="$1"

    if [[ ! -f "$file" ]]; then
        echo "{\"error\": \"File not found\"}"
        return 1
    fi

    if ! git_has_conflict_markers "$file"; then
        echo "{\"has_conflicts\": false}"
        return 0
    fi

    # Extract conflict sections
    local conflict_count=$(grep -c "^<<<<<<< " "$file")

    cat <<EOF
{
  "file": "$file",
  "has_conflicts": true,
  "conflict_count": $conflict_count
}
EOF
}

# ============================================================================
# GitHub Operations (via gh CLI)
# ============================================================================

# Check if gh CLI is available
# Returns: 0 if available, 1 if not
gh_is_available() {
    command -v gh >/dev/null 2>&1
}

# Clone a repository
# Args: repo_url, [directory]
# Returns: 0 on success, 1 on failure
gh_clone_repo() {
    local repo_url="$1"
    local directory="${2:-}"

    if ! gh_is_available; then
        echo "Error: gh CLI not installed" >&2
        return 1
    fi

    if [[ -n "$directory" ]]; then
        gh repo clone "$repo_url" "$directory"
    else
        gh repo clone "$repo_url"
    fi
}

# Fork a repository
# Args: repo_url, [clone_after_fork]
# Returns: 0 on success, 1 on failure
gh_fork_repo() {
    local repo_url="$1"
    local clone_after="${2:-yes}"

    if ! gh_is_available; then
        echo "Error: gh CLI not installed" >&2
        return 1
    fi

    if [[ "$clone_after" == "yes" ]]; then
        gh repo fork "$repo_url" --clone
    else
        gh repo fork "$repo_url"
    fi
}

# Create a pull request
# Args: title, body, [base_branch]
# Returns: 0 on success, 1 on failure
gh_create_pr() {
    local title="$1"
    local body="$2"
    local base_branch="${3:-main}"

    if ! gh_is_available; then
        echo "Error: gh CLI not installed" >&2
        return 1
    fi

    gh pr create --title "$title" --body "$body" --base "$base_branch"
}

# Check if current directory is a git repository
# Returns: 0 if git repo, 1 if not
git_is_repo() {
    git rev-parse --git-dir >/dev/null 2>&1
}

# Initialize a new git repository
# Args: [directory]
# Returns: 0 on success, 1 on failure
git_init_repo() {
    local directory="${1:-.}"

    if [[ ! -d "$directory" ]]; then
        echo "Error: Directory does not exist: $directory" >&2
        return 1
    fi

    cd "$directory" || return 1

    if git_is_repo; then
        echo "Directory is already a git repository" >&2
        return 1
    fi

    git init
}

# ============================================================================
# Commit Operations
# ============================================================================

# Create a commit with all changes
# Args: commit_message
# Returns: 0 on success, 1 on failure
git_commit_all() {
    local message="$1"

    if [[ -z "$message" ]]; then
        echo "Error: Commit message required" >&2
        return 1
    fi

    git add -A
    git commit -m "$message"
}

# Check if there are uncommitted changes
# Returns: 0 if changes exist, 1 if clean
git_has_changes() {
    ! git diff-index --quiet HEAD --
}

# Get current branch name
# Returns: Branch name (as stdout)
git_current_branch() {
    git rev-parse --abbrev-ref HEAD
}

# Get current commit hash
# Returns: Commit hash (as stdout)
git_current_commit() {
    git rev-parse HEAD
}

# ============================================================================
# Utility Functions
# ============================================================================

# Pull latest changes from remote
# Args: [branch_name]
# Returns: 0 on success, 1 on failure
git_pull_branch() {
    local branch_name="${1:-$(git_current_branch)}"

    git pull origin "$branch_name"
}

# Fetch all branches from remote
# Returns: 0 on success, 1 on failure
git_fetch_all() {
    git fetch --all --prune
}

# Check if local branch is behind remote
# Args: branch_name
# Returns: 0 if behind, 1 if up-to-date or ahead
git_is_behind_remote() {
    local branch_name="$1"

    git fetch origin "$branch_name" 2>/dev/null

    local local_commit=$(git rev-parse "$branch_name" 2>/dev/null)
    local remote_commit=$(git rev-parse "origin/$branch_name" 2>/dev/null)

    if [[ "$local_commit" != "$remote_commit" ]]; then
        # Check if local is ancestor of remote (i.e., behind)
        if git merge-base --is-ancestor "$branch_name" "origin/$branch_name" 2>/dev/null; then
            return 0  # Behind
        fi
    fi

    return 1  # Up-to-date or ahead
}

# Get repository root directory
# Returns: Repository root path (as stdout)
git_repo_root() {
    git rev-parse --show-toplevel
}

# Check if repository has remote
# Returns: 0 if remote exists, 1 if not
git_has_remote() {
    git remote | grep -q "origin"
}

# Add remote origin
# Args: remote_url
# Returns: 0 on success, 1 on failure
git_add_remote() {
    local remote_url="$1"

    if [[ -z "$remote_url" ]]; then
        echo "Error: Remote URL required" >&2
        return 1
    fi

    if git_has_remote; then
        echo "Remote 'origin' already exists" >&2
        return 1
    fi

    git remote add origin "$remote_url"
}

# Export functions for use in other scripts
export -f git_create_branch
export -f git_delete_branch
export -f git_branch_exists
export -f git_checkout_branch
export -f git_push_branch
export -f git_merge_branch
export -f git_has_conflicts
export -f git_list_conflicts
export -f git_abort_merge
export -f git_merge_in_progress
export -f git_get_commit_count
export -f git_get_lines_changed
export -f git_list_changed_files
export -f git_get_diff
export -f git_has_conflict_markers
export -f git_count_conflicts
export -f git_analyze_conflict
export -f gh_is_available
export -f gh_clone_repo
export -f gh_fork_repo
export -f gh_create_pr
export -f git_is_repo
export -f git_init_repo
export -f git_commit_all
export -f git_has_changes
export -f git_current_branch
export -f git_current_commit
export -f git_pull_branch
export -f git_fetch_all
export -f git_is_behind_remote
export -f git_repo_root
export -f git_has_remote
export -f git_add_remote
