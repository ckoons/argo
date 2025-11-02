#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_project_setup.sh - Tests for project_setup.sh template
#
# Tests project initialization and directory structure creation

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source test framework
source "$SCRIPT_DIR/test_framework.sh"

# Path to template
SETUP_TEMPLATE="$SCRIPT_DIR/../templates/project_setup.sh"

#
# Test: project_setup creates directory structure
#
test_setup_creates_structure() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Run setup
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null
    assert_success $? "project_setup should succeed"

    # Assert directory structure created
    assert_dir_exists ".argo-project" ".argo-project should exist"
    assert_dir_exists ".argo-project/logs" "logs directory should exist"
    assert_dir_exists ".argo-project/checkpoints" "checkpoints directory should exist"
}

#
# Test: project_setup creates state.json
#
test_setup_creates_state() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "my_project" "$project_dir" >/dev/null

    # Assert state.json created
    assert_file_exists ".argo-project/state.json" "state.json should exist"

    # Verify project name in state
    local name=$(jq -r '.project_name' .argo-project/state.json)
    assert_equals "my_project" "$name" "Project name should be in state.json"
}

#
# Test: project_setup creates config.json
#
test_setup_creates_config() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Assert config.json created
    assert_file_exists ".argo-project/config.json" "config.json should exist"

    # Verify it's valid JSON
    jq empty .argo-project/config.json 2>/dev/null
    assert_success $? "config.json should be valid JSON"
}

#
# Test: project_setup adds to .gitignore
#
test_setup_adds_gitignore() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Assert .gitignore exists
    assert_file_exists ".gitignore" ".gitignore should exist"

    # Assert contains .argo-project
    local content=$(cat .gitignore)
    assert_matches "$content" ".argo-project" ".gitignore should contain .argo-project"
}

#
# Test: project_setup doesn't duplicate .gitignore entry
#
test_setup_gitignore_idempotent() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Create .gitignore with .argo-project already
    echo ".argo-project/" > .gitignore

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Count occurrences of .argo-project
    local count=$(grep -c ".argo-project" .gitignore)
    assert_equals "1" "$count" "Should not duplicate .gitignore entry"
}

#
# Test: project_setup registers project
#
test_setup_registers_project() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "my_project" "$project_dir" >/dev/null

    # Assert registry file created
    assert_file_exists "$ARGO_PROJECTS_REGISTRY" "Registry should be created"

    # Verify project registered
    local count=$(jq '.projects | length' "$ARGO_PROJECTS_REGISTRY")
    assert_equals "1" "$count" "Project should be registered"
}

#
# Test: project_setup with default path (current directory)
#
test_setup_default_path() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    # Don't provide path argument - should use current directory
    "$SETUP_TEMPLATE" "test_project" >/dev/null
    assert_success $? "Should work with default path"

    # Verify structure created in current directory
    assert_dir_exists ".argo-project" "Should create in current directory"
}

#
# Test: project_setup requires project name
#
test_setup_requires_name() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Run without project name
    "$SETUP_TEMPLATE" 2>/dev/null
    assert_failure $? "Should fail without project name"
}

#
# Test: project_setup is idempotent
#
test_setup_idempotent() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    # Run setup twice
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null
    assert_success $? "Should be idempotent"

    # Verify state.json still valid
    jq empty .argo-project/state.json 2>/dev/null
    assert_success $? "state.json should remain valid"
}

#
# Test: project_setup creates logs directory
#
test_setup_logs_directory() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Verify logs directory exists
    assert_dir_exists ".argo-project/logs" "logs directory should exist"
}

#
# Test: project_setup creates checkpoints directory
#
test_setup_checkpoints_directory() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Verify checkpoints directory exists
    assert_dir_exists ".argo-project/checkpoints" "checkpoints directory should exist"
}

#
# Run all tests
#
echo "Testing project_setup.sh"
echo "========================"

run_test test_setup_creates_structure
run_test test_setup_creates_state
run_test test_setup_creates_config
run_test test_setup_adds_gitignore
run_test test_setup_gitignore_idempotent
run_test test_setup_registers_project
run_test test_setup_default_path
run_test test_setup_requires_name
run_test test_setup_idempotent
run_test test_setup_logs_directory
run_test test_setup_checkpoints_directory

# Print summary
test_summary
