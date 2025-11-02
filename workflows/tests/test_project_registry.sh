#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_project_registry.sh - Tests for project_registry.sh module
#
# Tests project registration, deregistration, and lookup functions

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source test framework
source "$SCRIPT_DIR/test_framework.sh"

# Source module under test
source "$SCRIPT_DIR/../lib/project_registry.sh"

#
# Test: register_project creates new entry
#
test_register_project() {
    local project_name="test_project"
    local project_path="$TEST_TMP_DIR/test_project"

    mkdir -p "$project_path"

    # Override registry location for testing
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    # Register project
    local project_id=$(register_project "$project_name" "$project_path")
    local result=$?

    # Assert success
    assert_success $result "register_project should succeed"
    assert_not_empty "$project_id" "Project ID should be generated"

    # Assert registry file created
    assert_file_exists "$ARGO_PROJECTS_REGISTRY" "Registry file should be created"

    # Assert project exists in registry
    local found=$(jq -r ".projects.\"$project_id\".name" "$ARGO_PROJECTS_REGISTRY")
    assert_equals "$project_name" "$found" "Project should be in registry"
}

#
# Test: register_project generates unique IDs
#
test_register_unique_ids() {
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    mkdir -p "$TEST_TMP_DIR/project1"
    mkdir -p "$TEST_TMP_DIR/project2"

    # Register two projects with same name
    local id1=$(register_project "myproject" "$TEST_TMP_DIR/project1")
    sleep 1  # Ensure different timestamp
    local id2=$(register_project "myproject" "$TEST_TMP_DIR/project2")

    # IDs should be different
    if [[ "$id1" == "$id2" ]]; then
        test_fail "Project IDs should be unique (got: $id1, $id2)"
        return 1
    fi

    # Both should exist
    local count=$(jq '.projects | length' "$ARGO_PROJECTS_REGISTRY")
    assert_equals "2" "$count" "Both projects should exist"
}

#
# Test: deregister_project removes entry
#
test_deregister_project() {
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    mkdir -p "$TEST_TMP_DIR/project"
    local project_id=$(register_project "test" "$TEST_TMP_DIR/project")

    # Deregister
    deregister_project "$project_id"
    assert_success $? "deregister_project should succeed"

    # Assert project removed
    local found=$(jq -r ".projects.\"$project_id\"" "$ARGO_PROJECTS_REGISTRY")
    assert_equals "null" "$found" "Project should be removed from registry"
}

#
# Test: deregister_project fails for unknown ID
#
test_deregister_unknown() {
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    # Initialize empty registry
    mkdir -p "$(dirname "$ARGO_PROJECTS_REGISTRY")"
    echo '{"projects": {}}' > "$ARGO_PROJECTS_REGISTRY"

    # Try to deregister non-existent project
    deregister_project "nonexistent-123"
    assert_failure $? "deregister_project should fail for unknown ID"
}

#
# Test: list_projects shows all projects
#
test_list_projects() {
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    mkdir -p "$TEST_TMP_DIR/project1"
    mkdir -p "$TEST_TMP_DIR/project2"

    register_project "project1" "$TEST_TMP_DIR/project1" > /dev/null
    register_project "project2" "$TEST_TMP_DIR/project2" > /dev/null

    # List projects
    local output=$(list_projects)

    # Should contain both projects
    assert_matches "$output" "project1" "Output should contain project1"
    assert_matches "$output" "project2" "Output should contain project2"

    # Should be tab-separated format
    local line_count=$(echo "$output" | wc -l | tr -d ' ')
    assert_equals "2" "$line_count" "Should have 2 lines"
}

#
# Test: list_projects returns empty for no projects
#
test_list_projects_empty() {
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    # Initialize empty registry
    mkdir -p "$(dirname "$ARGO_PROJECTS_REGISTRY")"
    echo '{"projects": {}}' > "$ARGO_PROJECTS_REGISTRY"

    # List projects
    local output=$(list_projects)

    # Should be empty
    assert_empty "$output" "Output should be empty for no projects"
}

#
# Test: get_project_info returns JSON
#
test_get_project_info() {
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    mkdir -p "$TEST_TMP_DIR/project"
    local project_id=$(register_project "myproject" "$TEST_TMP_DIR/project")

    # Get project info
    local info=$(get_project_info "$project_id")
    assert_success $? "get_project_info should succeed"

    # Validate JSON structure
    local name=$(echo "$info" | jq -r '.name')
    assert_equals "myproject" "$name" "Info should contain project name"

    local path=$(echo "$info" | jq -r '.path')
    assert_equals "$TEST_TMP_DIR/project" "$path" "Info should contain project path"
}

#
# Test: get_project_info fails for unknown ID
#
test_get_project_info_unknown() {
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    # Initialize empty registry
    mkdir -p "$(dirname "$ARGO_PROJECTS_REGISTRY")"
    echo '{"projects": {}}' > "$ARGO_PROJECTS_REGISTRY"

    # Try to get non-existent project
    get_project_info "nonexistent-123" 2>/dev/null
    assert_failure $? "get_project_info should fail for unknown ID"
}

#
# Test: update_project_status changes status
#
test_update_project_status() {
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    mkdir -p "$TEST_TMP_DIR/project"
    local project_id=$(register_project "test" "$TEST_TMP_DIR/project")

    # Update status
    update_project_status "$project_id" "building"
    assert_success $? "update_project_status should succeed"

    # Verify status changed
    local status=$(jq -r ".projects.\"$project_id\".status" "$ARGO_PROJECTS_REGISTRY")
    assert_equals "building" "$status" "Status should be updated"
}

#
# Test: update_project_status updates timestamp
#
test_update_project_status_timestamp() {
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    mkdir -p "$TEST_TMP_DIR/project"
    local project_id=$(register_project "test" "$TEST_TMP_DIR/project")

    # Get initial timestamp
    local ts1=$(jq -r ".projects.\"$project_id\".last_active" "$ARGO_PROJECTS_REGISTRY")

    sleep 1

    # Update status
    update_project_status "$project_id" "building"

    # Get new timestamp
    local ts2=$(jq -r ".projects.\"$project_id\".last_active" "$ARGO_PROJECTS_REGISTRY")

    # Timestamps should differ
    if [[ "$ts1" == "$ts2" ]]; then
        test_fail "last_active timestamp should be updated"
        return 1
    fi
}

#
# Test: find_project_by_path locates project
#
test_find_project_by_path() {
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    mkdir -p "$TEST_TMP_DIR/myproject"
    local expected_id=$(register_project "test" "$TEST_TMP_DIR/myproject")

    # Find by path
    local found_id=$(find_project_by_path "$TEST_TMP_DIR/myproject")
    assert_success $? "find_project_by_path should succeed"

    assert_equals "$expected_id" "$found_id" "Should find correct project ID"
}

#
# Test: find_project_by_path fails for unknown path
#
test_find_project_by_path_unknown() {
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    # Initialize empty registry
    mkdir -p "$(dirname "$ARGO_PROJECTS_REGISTRY")"
    echo '{"projects": {}}' > "$ARGO_PROJECTS_REGISTRY"

    # Try to find non-existent path
    find_project_by_path "/nonexistent/path" 2>/dev/null
    assert_failure $? "find_project_by_path should fail for unknown path"
}

#
# Test: registry survives multiple operations
#
test_registry_persistence() {
    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"

    mkdir -p "$TEST_TMP_DIR/p1"
    mkdir -p "$TEST_TMP_DIR/p2"
    mkdir -p "$TEST_TMP_DIR/p3"

    # Register three projects
    local id1=$(register_project "p1" "$TEST_TMP_DIR/p1")
    local id2=$(register_project "p2" "$TEST_TMP_DIR/p2")
    local id3=$(register_project "p3" "$TEST_TMP_DIR/p3")

    # Deregister middle one
    deregister_project "$id2"

    # Update status of first
    update_project_status "$id1" "building"

    # Verify registry is consistent
    local count=$(jq '.projects | length' "$ARGO_PROJECTS_REGISTRY")
    assert_equals "2" "$count" "Should have 2 projects remaining"

    # Verify correct ones remain
    local status=$(jq -r ".projects.\"$id1\".status" "$ARGO_PROJECTS_REGISTRY")
    assert_equals "building" "$status" "First project should be updated"

    local exists=$(jq -r ".projects.\"$id3\" != null" "$ARGO_PROJECTS_REGISTRY")
    assert_equals "true" "$exists" "Third project should exist"
}

#
# Run all tests
#
echo "Testing project_registry.sh"
echo "============================"

run_test test_register_project
run_test test_register_unique_ids
run_test test_deregister_project
run_test test_deregister_unknown
run_test test_list_projects
run_test test_list_projects_empty
run_test test_get_project_info
run_test test_get_project_info_unknown
run_test test_update_project_status
run_test test_update_project_status_timestamp
run_test test_find_project_by_path
run_test test_find_project_by_path_unknown
run_test test_registry_persistence

# Print summary
test_summary
