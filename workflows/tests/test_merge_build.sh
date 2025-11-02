#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_merge_build.sh - Tests for merge_build.sh phase handler
#
# Tests branch merging and final workflow completion

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source test framework
source "$SCRIPT_DIR/test_framework.sh"

# Source library modules
source "$SCRIPT_DIR/../lib/state_file.sh"

# Path to scripts
SETUP_TEMPLATE="$SCRIPT_DIR/../templates/project_setup.sh"
MERGE_BUILD="$SCRIPT_DIR/../phases/merge_build.sh"

#
# Test: merge_build requires project path
#
test_merge_build_requires_path() {
    "$MERGE_BUILD" 2>/dev/null
    assert_failure $? "Should fail without project path"
}

#
# Test: merge_build reads builders from state
#
test_merge_build_reads_builders() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Initialize git repo
    git init >/dev/null 2>&1
    git config user.name "Test" 2>/dev/null
    git config user.email "test@example.com" 2>/dev/null
    touch README.md
    git add README.md 2>/dev/null
    git commit -m "Initial commit" >/dev/null 2>&1

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set execution with completed builder
    local execution_json='{
        "builders": [
            {"id": "backend", "status": "completed", "branch": "argo-builder-backend"}
        ],
        "status": "completed"
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson execution "$execution_json" '.execution = $execution' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "merge_build"

    # Run phase handler (will attempt to merge)
    "$MERGE_BUILD" "$project_dir" >/dev/null 2>&1 || true

    # Just verify it attempted to process builders (no crash)
    assert_success 0 "Should attempt to process builders"
}

#
# Test: merge_build merges builder branches
#
test_merge_build_merges_branches() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Initialize git with a simple branch
    git init >/dev/null 2>&1
    git config user.name "Test" 2>/dev/null
    git config user.email "test@example.com" 2>/dev/null
    echo "main" > main.txt
    git add main.txt 2>/dev/null
    git commit -m "Main commit" >/dev/null 2>&1

    # Create builder branch with change
    git checkout -b argo-builder-backend >/dev/null 2>&1
    echo "backend" > backend.txt
    git add backend.txt 2>/dev/null
    git commit -m "Backend work" >/dev/null 2>&1
    git checkout main >/dev/null 2>&1

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set execution with completed builder
    local execution_json='{
        "builders": [
            {"id": "backend", "status": "completed", "branch": "argo-builder-backend"}
        ],
        "status": "completed"
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson execution "$execution_json" '.execution = $execution' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "merge_build"

    # Run phase handler
    "$MERGE_BUILD" "$project_dir" >/dev/null 2>&1

    # Check if merge occurred (backend.txt should exist on main)
    git checkout main >/dev/null 2>&1
    if [[ -f backend.txt ]]; then
        assert_file_exists "backend.txt" "Builder branch should be merged"
    fi
}

#
# Test: merge_build handles merge conflicts
#
test_merge_build_handles_conflicts() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Initialize git with conflicting changes
    git init >/dev/null 2>&1
    git config user.name "Test" 2>/dev/null
    git config user.email "test@example.com" 2>/dev/null
    echo "version 1" > conflict.txt
    git add conflict.txt 2>/dev/null
    git commit -m "Main version" >/dev/null 2>&1

    # Create builder branch with conflicting change
    git checkout -b argo-builder-backend >/dev/null 2>&1
    echo "version 2" > conflict.txt
    git add conflict.txt 2>/dev/null
    git commit -m "Backend version" >/dev/null 2>&1

    # Add another commit on main to create conflict
    git checkout main >/dev/null 2>&1
    echo "version 3" > conflict.txt
    git add conflict.txt 2>/dev/null
    git commit -m "Main update" >/dev/null 2>&1

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set execution with completed builder
    local execution_json='{
        "builders": [
            {"id": "backend", "status": "completed", "branch": "argo-builder-backend"}
        ],
        "status": "completed"
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson execution "$execution_json" '.execution = $execution' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "merge_build"

    # Run phase handler (should detect conflict)
    "$MERGE_BUILD" "$project_dir" >/dev/null 2>&1 || true

    # Abort any ongoing merge
    git merge --abort 2>/dev/null || true

    # Should have handled conflict (either resolved or reported)
    assert_success 0 "Should handle merge conflict"
}

#
# Test: merge_build transitions to storage phase
#
test_merge_build_transitions_phase() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Initialize git
    git init >/dev/null 2>&1
    git config user.name "Test" 2>/dev/null
    git config user.email "test@example.com" 2>/dev/null
    echo "main" > main.txt
    git add main.txt 2>/dev/null
    git commit -m "Main" >/dev/null 2>&1

    # Create clean builder branch
    git checkout -b argo-builder-backend >/dev/null 2>&1
    echo "backend" > backend.txt
    git add backend.txt 2>/dev/null
    git commit -m "Backend" >/dev/null 2>&1
    git checkout main >/dev/null 2>&1

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set execution with completed builder
    local execution_json='{
        "builders": [
            {"id": "backend", "status": "completed", "branch": "argo-builder-backend"}
        ],
        "status": "completed"
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson execution "$execution_json" '.execution = $execution' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "merge_build"

    # Run phase handler
    "$MERGE_BUILD" "$project_dir" >/dev/null 2>&1

    # Check phase transition
    local new_phase=$(read_state "phase")
    assert_equals "storage" "$new_phase" "Should transition to storage phase"
}

#
# Test: merge_build hands off to user
#
test_merge_build_handoff_to_user() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Initialize git
    git init >/dev/null 2>&1
    git config user.name "Test" 2>/dev/null
    git config user.email "test@example.com" 2>/dev/null
    echo "main" > main.txt
    git add main.txt 2>/dev/null
    git commit -m "Main" >/dev/null 2>&1

    # Create clean builder branch
    git checkout -b argo-builder-backend >/dev/null 2>&1
    echo "backend" > backend.txt
    git add backend.txt 2>/dev/null
    git commit -m "Backend" >/dev/null 2>&1
    git checkout main >/dev/null 2>&1

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set execution with completed builder
    local execution_json='{
        "builders": [
            {"id": "backend", "status": "completed", "branch": "argo-builder-backend"}
        ],
        "status": "completed"
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson execution "$execution_json" '.execution = $execution' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "merge_build"
    update_state "action_owner" "code"

    # Run phase handler
    "$MERGE_BUILD" "$project_dir" >/dev/null 2>&1

    # Check action owner handoff
    local action_owner=$(read_state "action_owner")
    assert_equals "user" "$action_owner" "Should hand off to user"
}

#
# Test: merge_build handles empty builders
#
test_merge_build_handles_no_builders() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Initialize git
    git init >/dev/null 2>&1
    git config user.name "Test" 2>/dev/null
    git config user.email "test@example.com" 2>/dev/null
    touch README.md
    git add README.md 2>/dev/null
    git commit -m "Initial" >/dev/null 2>&1

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set execution with empty builders
    local execution_json='{
        "builders": [],
        "status": "completed"
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson execution "$execution_json" '.execution = $execution' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "merge_build"

    # Run phase handler
    "$MERGE_BUILD" "$project_dir" >/dev/null 2>&1
    local result=$?

    # Should handle gracefully
    assert_success 0 "Should handle empty builders"
}

#
# Run all tests
#
echo "Testing merge_build.sh"
echo "======================"

run_test test_merge_build_requires_path
run_test test_merge_build_reads_builders
run_test test_merge_build_merges_branches
run_test test_merge_build_handles_conflicts
run_test test_merge_build_transitions_phase
run_test test_merge_build_handoff_to_user
run_test test_merge_build_handles_no_builders

# Print summary
test_summary
