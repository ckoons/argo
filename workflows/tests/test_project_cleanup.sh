#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_project_cleanup.sh - Tests for project_cleanup.sh template
#
# Tests project cleanup with different levels (partial, full, nuclear)

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source test framework
source "$SCRIPT_DIR/test_framework.sh"

# Path to templates
SETUP_TEMPLATE="$SCRIPT_DIR/../templates/project_setup.sh"
CLEANUP_TEMPLATE="$SCRIPT_DIR/../templates/project_cleanup.sh"

#
# Test: partial cleanup keeps logs and checkpoints
#
test_cleanup_partial() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Create some files
    touch .argo-project/builders/test.txt
    touch .argo-project/logs/workflow.log
    touch .argo-project/checkpoints/test.json

    # Run partial cleanup
    "$CLEANUP_TEMPLATE" "$project_dir" "partial" >/dev/null
    assert_success $? "Partial cleanup should succeed"

    # Assert builders deleted, logs/checkpoints kept
    if [[ -f .argo-project/builders/test.txt ]]; then
        test_fail "Builders should be deleted"
        return 1
    fi

    assert_file_exists ".argo-project/logs/workflow.log" "Logs should be kept"
    assert_file_exists ".argo-project/checkpoints/test.json" "Checkpoints should be kept"
}

#
# Test: full cleanup keeps checkpoints
#
test_cleanup_full() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Create files
    touch .argo-project/builders/test.txt
    touch .argo-project/logs/workflow.log
    touch .argo-project/checkpoints/test.json

    # Run full cleanup
    "$CLEANUP_TEMPLATE" "$project_dir" "full" >/dev/null
    assert_success $? "Full cleanup should succeed"

    # Assert builders and logs deleted, checkpoints kept
    if [[ -f .argo-project/builders/test.txt ]]; then
        test_fail "Builders should be deleted"
        return 1
    fi

    if [[ -f .argo-project/logs/workflow.log ]]; then
        test_fail "Logs should be deleted"
        return 1
    fi

    assert_file_exists ".argo-project/checkpoints/test.json" "Checkpoints should be kept"
}

#
# Test: nuclear cleanup removes everything
#
test_cleanup_nuclear() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Create files
    touch .argo-project/builders/test.txt
    touch .argo-project/logs/workflow.log
    touch .argo-project/checkpoints/test.json

    # Run nuclear cleanup
    "$CLEANUP_TEMPLATE" "$project_dir" "nuclear" >/dev/null
    assert_success $? "Nuclear cleanup should succeed"

    # Assert entire .argo-project deleted
    if [[ -d .argo-project ]]; then
        test_fail ".argo-project should be completely removed"
        return 1
    fi
}

#
# Test: cleanup with default path (current directory)
#
test_cleanup_default_path() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Run cleanup without path argument (should use current directory)
    "$CLEANUP_TEMPLATE" "" "partial" >/dev/null
    assert_success $? "Should work with default path"
}

#
# Test: cleanup with default type (partial)
#
test_cleanup_default_type() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    touch .argo-project/logs/workflow.log

    # Run cleanup without type argument (should default to partial)
    "$CLEANUP_TEMPLATE" "$project_dir" >/dev/null
    assert_success $? "Should work with default type"

    # Verify partial cleanup behavior (logs kept)
    assert_file_exists ".argo-project/logs/workflow.log" "Should use partial cleanup by default"
}

#
# Test: cleanup handles missing .argo-project gracefully
#
test_cleanup_missing_directory() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Don't run setup - no .argo-project directory

    # Run cleanup (should not error)
    "$CLEANUP_TEMPLATE" "$project_dir" "partial" >/dev/null
    # Success or graceful failure both acceptable
    local result=$?
    if [[ $result -ne 0 ]]; then
        # It's ok to return non-zero, just shouldn't crash
        return 0
    fi
}

#
# Test: cleanup preserves state.json in partial mode
#
test_cleanup_preserves_state_partial() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Run partial cleanup
    "$CLEANUP_TEMPLATE" "$project_dir" "partial" >/dev/null

    # state.json should still exist
    assert_file_exists ".argo-project/state.json" "state.json should be preserved"
}

#
# Test: cleanup preserves config.json in partial mode
#
test_cleanup_preserves_config_partial() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Run partial cleanup
    "$CLEANUP_TEMPLATE" "$project_dir" "partial" >/dev/null

    # config.json should still exist
    assert_file_exists ".argo-project/config.json" "config.json should be preserved"
}

#
# Test: cleanup can be run multiple times
#
test_cleanup_idempotent() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Run cleanup twice
    "$CLEANUP_TEMPLATE" "$project_dir" "partial" >/dev/null
    "$CLEANUP_TEMPLATE" "$project_dir" "partial" >/dev/null
    assert_success $? "Cleanup should be idempotent"
}

#
# Test: cleanup removes builders directory
#
test_cleanup_removes_builders() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Create builder files
    mkdir -p .argo-project/builders/backend
    touch .argo-project/builders/backend/file.txt

    # Run partial cleanup
    "$CLEANUP_TEMPLATE" "$project_dir" "partial" >/dev/null

    # Builders should be gone
    if [[ -d .argo-project/builders/backend ]]; then
        test_fail "Builders directory should be removed"
        return 1
    fi
}

#
# Run all tests
#
echo "Testing project_cleanup.sh"
echo "=========================="

run_test test_cleanup_partial
run_test test_cleanup_full
run_test test_cleanup_nuclear
run_test test_cleanup_default_path
run_test test_cleanup_default_type
run_test test_cleanup_missing_directory
run_test test_cleanup_preserves_state_partial
run_test test_cleanup_preserves_config_partial
run_test test_cleanup_idempotent
run_test test_cleanup_removes_builders

# Print summary
test_summary
