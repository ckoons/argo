#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_state_manager.sh - Unit tests for state-manager.sh
#
# Tests all state management functions

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/../lib"

# Load test framework
source "$SCRIPT_DIR/test_framework.sh"

# We'll set SESSIONS_DIR/ARCHIVES_DIR in a custom setup function
# This ensures each test gets its own isolated directories

# Custom test setup for state manager tests
state_manager_test_setup() {
    # Call standard test_setup first
    test_setup

    # Override state manager directories for this test
    export SESSIONS_DIR="$TEST_TMP_DIR/sessions"
    export ARCHIVES_DIR="$TEST_TMP_DIR/archives"
    mkdir -p "$SESSIONS_DIR" "$ARCHIVES_DIR"
}

# Override test_setup function
test_setup() {
    # Create temporary directory for this test
    TEST_TMP_DIR=$(mktemp -d /tmp/argo_test.XXXXXX)

    # Export for tests
    export TEST_TMP_DIR
    export TEST_SESSION_DIR="$TEST_TMP_DIR/sessions"
    export ARGO_SESSIONS_DIR="$TEST_SESSION_DIR"

    # Set state manager directories
    export SESSIONS_DIR="$TEST_TMP_DIR/sessions"
    export ARCHIVES_DIR="$TEST_TMP_DIR/archives"
    mkdir -p "$SESSIONS_DIR" "$ARCHIVES_DIR"
}

# Load state manager (after test_setup override)
source "$LIB_DIR/state-manager.sh"

#
# Session Management Tests
#

test_create_session() {
    local session_id=$(create_session "test_project")

    assert_not_empty "$session_id" "Session ID should not be empty" || return 1
    assert_matches "$session_id" "^proj-[0-9]{8}-[0-9]{6}$" "Session ID format" || return 1

    local session_dir=$(get_session_dir "$session_id")
    assert_dir_exists "$session_dir" "Session directory should exist" || return 1
    assert_dir_exists "$session_dir/builders" "Builders directory should exist" || return 1
    assert_file_exists "$session_dir/context.json" "Context file should exist" || return 1

    # Verify context content
    assert_json_equals "$session_dir/context.json" ".session_id" "$session_id" || return 1
    assert_json_equals "$session_dir/context.json" ".project_name" "test_project" || return 1
    assert_json_equals "$session_dir/context.json" ".current_phase" "initialized" || return 1
}

test_session_exists() {
    local session_id=$(create_session "test_exists")

    session_exists "$session_id"
    assert_success $? "Session should exist" || return 1

    session_exists "nonexistent-session"
    assert_failure $? "Nonexistent session should not exist" || return 1
}

test_get_or_create_session() {
    # Create session
    local session_id1=$(get_or_create_session "test_project")
    assert_not_empty "$session_id1" "First session ID" || return 1

    # Set SESSION_ID environment variable (simulates parent workflow)
    export SESSION_ID="$session_id1"

    # Should return existing session
    local session_id2=$(get_or_create_session "test_project")
    assert_equals "$session_id1" "$session_id2" "Should return existing session" || return 1

    unset SESSION_ID
}

test_get_session_dir() {
    local session_id=$(create_session "test_dir")
    local session_dir=$(get_session_dir "$session_id")

    assert_matches "$session_dir" "/sessions/$session_id$" "Session dir path" || return 1
    assert_dir_exists "$session_dir" "Session directory" || return 1
}

#
# Context Management Tests
#

test_save_and_load_context() {
    local session_id=$(create_session "test_context")

    local test_json='{"session_id":"'$session_id'","custom_field":"test_value"}'
    save_context "$session_id" "$test_json"

    local loaded=$(load_context "$session_id")
    assert_not_empty "$loaded" "Loaded context" || return 1

    # Verify custom field
    local custom_val=$(echo "$loaded" | jq -r '.custom_field')
    assert_equals "test_value" "$custom_val" "Custom field value" || return 1
}

test_save_context_field() {
    local session_id=$(create_session "test_field")

    save_context_field "$session_id" "project_type" "project"
    save_context_field "$session_id" "is_git_repo" "yes"

    assert_json_equals "$SESSIONS_DIR/$session_id/context.json" ".project_type" "project" || return 1
    assert_json_equals "$SESSIONS_DIR/$session_id/context.json" ".is_git_repo" "yes" || return 1
}

test_get_context_field() {
    local session_id=$(create_session "test_get_field")

    save_context_field "$session_id" "test_field" "test_value"

    local value=$(get_context_field "$session_id" "test_field")
    assert_equals "test_value" "$value" "Retrieved field value" || return 1
}

test_update_phase() {
    local session_id=$(create_session "test_phase")

    update_phase "$session_id" "design"
    assert_json_equals "$SESSIONS_DIR/$session_id/context.json" ".current_phase" "design" || return 1

    update_phase "$session_id" "build"
    assert_json_equals "$SESSIONS_DIR/$session_id/context.json" ".current_phase" "build" || return 1
}

#
# Builder State Management Tests
#

test_init_builder_state() {
    local session_id=$(create_session "test_builder")
    local builder_id="argo-A"

    init_builder_state "$session_id" "$builder_id" "feature/test" "Test task"

    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"
    assert_file_exists "$builder_file" "Builder state file" || return 1

    assert_json_equals "$builder_file" ".builder_id" "$builder_id" || return 1
    assert_json_equals "$builder_file" ".feature_branch" "feature/test" || return 1
    assert_json_equals "$builder_file" ".task_description" "Test task" || return 1
    assert_json_equals "$builder_file" ".status" "initialized" || return 1
    assert_json_equals "$builder_file" ".tests_passed" "false" || return 1
}

test_update_builder_status() {
    local session_id=$(create_session "test_status")
    local builder_id="argo-B"

    init_builder_state "$session_id" "$builder_id" "feature/test" "Test"

    update_builder_status "$session_id" "$builder_id" "building"
    assert_json_equals "$SESSIONS_DIR/$session_id/builders/$builder_id.json" ".status" "building" || return 1

    update_builder_status "$session_id" "$builder_id" "testing"
    assert_json_equals "$SESSIONS_DIR/$session_id/builders/$builder_id.json" ".status" "testing" || return 1

    update_builder_status "$session_id" "$builder_id" "completed"
    assert_json_equals "$SESSIONS_DIR/$session_id/builders/$builder_id.json" ".status" "completed" || return 1

    # Verify completion time was set
    local completion=$(jq -r '.completion_time' "$SESSIONS_DIR/$session_id/builders/$builder_id.json")
    assert_not_empty "$completion" "Completion time should be set" || return 1
    assert_matches "$completion" "^[0-9]{4}-[0-9]{2}-[0-9]{2}T" "Completion time format" || return 1
}

test_get_builder_status() {
    local session_id=$(create_session "test_get_status")
    local builder_id="argo-C"

    init_builder_state "$session_id" "$builder_id" "feature/test" "Test"

    local status=$(get_builder_status "$session_id" "$builder_id")
    assert_equals "initialized" "$status" "Initial status" || return 1

    update_builder_status "$session_id" "$builder_id" "building"
    status=$(get_builder_status "$session_id" "$builder_id")
    assert_equals "building" "$status" "Updated status" || return 1

    # Test nonexistent builder
    status=$(get_builder_status "$session_id" "nonexistent")
    assert_equals "not_found" "$status" "Nonexistent builder" || return 1
}

test_save_and_load_builder_state() {
    local session_id=$(create_session "test_builder_state")
    local builder_id="argo-D"

    init_builder_state "$session_id" "$builder_id" "feature/test" "Test"

    local test_state='{"builder_id":"'$builder_id'","custom":"value","status":"testing"}'
    save_builder_state "$session_id" "$builder_id" "$test_state"

    local loaded=$(load_builder_state "$session_id" "$builder_id")
    assert_not_empty "$loaded" "Loaded builder state" || return 1

    local custom_val=$(echo "$loaded" | jq -r '.custom')
    assert_equals "value" "$custom_val" "Custom field in builder state" || return 1
}

test_builder_needs_assistance() {
    local session_id=$(create_session "test_assistance")
    local builder_id="argo-E"

    init_builder_state "$session_id" "$builder_id" "feature/test" "Test"

    # Initially should not need assistance
    builder_needs_assistance "$session_id" "$builder_id"
    assert_failure $? "Should not need assistance initially" || return 1

    # Set needs_assistance to true
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"
    local temp_file=$(mktemp)
    jq '.needs_assistance = true' "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"

    # Now should need assistance
    builder_needs_assistance "$session_id" "$builder_id"
    assert_success $? "Should need assistance" || return 1
}

test_list_builders() {
    local session_id=$(create_session "test_list")

    # Create multiple builders
    init_builder_state "$session_id" "argo-A" "feature/a" "Task A"
    init_builder_state "$session_id" "argo-B" "feature/b" "Task B"
    init_builder_state "$session_id" "argo-C" "feature/c" "Task C"

    local builders=$(list_builders "$session_id")
    local count=$(echo "$builders" | wc -w | tr -d ' ')

    assert_equals "3" "$count" "Number of builders" || return 1

    # Verify each builder is in the list
    echo "$builders" | grep -q "argo-A"
    assert_success $? "Builder A in list" || return 1

    echo "$builders" | grep -q "argo-B"
    assert_success $? "Builder B in list" || return 1

    echo "$builders" | grep -q "argo-C"
    assert_success $? "Builder C in list" || return 1
}

test_get_builder_statistics() {
    local session_id=$(create_session "test_stats")

    # Create builders with different statuses
    init_builder_state "$session_id" "argo-A" "feature/a" "Task A"
    init_builder_state "$session_id" "argo-B" "feature/b" "Task B"
    init_builder_state "$session_id" "argo-C" "feature/c" "Task C"
    init_builder_state "$session_id" "argo-D" "feature/d" "Task D"

    update_builder_status "$session_id" "argo-B" "building"
    update_builder_status "$session_id" "argo-C" "completed"
    update_builder_status "$session_id" "argo-D" "failed"

    local stats=$(get_builder_statistics "$session_id")
    assert_not_empty "$stats" "Statistics JSON" || return 1

    # Verify counts
    local total=$(echo "$stats" | jq -r '.total')
    local initialized=$(echo "$stats" | jq -r '.initialized')
    local building=$(echo "$stats" | jq -r '.building')
    local completed=$(echo "$stats" | jq -r '.completed')
    local failed=$(echo "$stats" | jq -r '.failed')

    assert_equals "4" "$total" "Total builders" || return 1
    assert_equals "1" "$initialized" "Initialized count" || return 1
    assert_equals "1" "$building" "Building count" || return 1
    assert_equals "1" "$completed" "Completed count" || return 1
    assert_equals "1" "$failed" "Failed count" || return 1
}

#
# Session Lifecycle Tests
#

test_archive_session() {
    local session_id=$(create_session "test_archive")

    # Verify session exists
    session_exists "$session_id"
    assert_success $? "Session exists before archive" || return 1

    # Archive session
    archive_session "$session_id" >/dev/null

    # Verify session moved to archives
    session_exists "$session_id"
    assert_failure $? "Session should not exist after archive" || return 1

    local archive_dir="$ARCHIVES_DIR/$session_id"
    assert_dir_exists "$archive_dir" "Archive directory" || return 1
}

test_delete_session() {
    local session_id=$(create_session "test_delete")

    # Verify session exists
    session_exists "$session_id"
    assert_success $? "Session exists before delete" || return 1

    # Delete session
    delete_session "$session_id" >/dev/null

    # Verify session deleted
    session_exists "$session_id"
    assert_failure $? "Session should not exist after delete" || return 1
}

test_list_sessions() {
    # Create multiple sessions (with small delay to ensure unique timestamps)
    local session1=$(create_session "project1")
    sleep 1
    local session2=$(create_session "project2")
    sleep 1
    local session3=$(create_session "project3")

    # Verify all sessions exist first
    assert_dir_exists "$SESSIONS_DIR/$session1" "Session 1 directory" || return 1
    assert_dir_exists "$SESSIONS_DIR/$session2" "Session 2 directory" || return 1
    assert_dir_exists "$SESSIONS_DIR/$session3" "Session 3 directory" || return 1

    local sessions=$(list_sessions | sort)

    # Verify each session is in the list
    echo "$sessions" | grep -q "$session1"
    assert_success $? "Session 1 in list" || return 1

    echo "$sessions" | grep -q "$session2"
    assert_success $? "Session 2 in list" || return 1

    echo "$sessions" | grep -q "$session3"
    assert_success $? "Session 3 in list" || return 1

    # Count should be at least 3 (might have more due to test isolation issues)
    local count=$(echo "$sessions" | grep -v '^$' | wc -l | tr -d ' ')
    if [[ $count -lt 3 ]]; then
        test_fail "Expected at least 3 sessions, got $count"
        return 1
    fi
}

test_get_latest_session() {
    local session1=$(create_session "old_project")
    sleep 1
    local session2=$(create_session "new_project")

    local latest=$(get_latest_session)
    assert_equals "$session2" "$latest" "Latest session should be most recent" || return 1
}

#
# Error Handling Tests
#

test_load_context_nonexistent() {
    load_context "nonexistent-session" 2>/dev/null
    assert_failure $? "Loading nonexistent context should fail" || return 1
}

test_get_context_field_nonexistent() {
    get_context_field "nonexistent-session" "field" 2>/dev/null
    assert_failure $? "Getting field from nonexistent context should fail" || return 1
}

test_update_builder_status_nonexistent() {
    local session_id=$(create_session "test_error")
    update_builder_status "$session_id" "nonexistent-builder" "completed" 2>/dev/null
    assert_failure $? "Updating nonexistent builder should fail" || return 1
}

#
# Run all tests
#

main() {
    echo "Running state-manager.sh unit tests..."

    # Session management tests
    run_test test_create_session
    run_test test_session_exists
    run_test test_get_or_create_session
    run_test test_get_session_dir

    # Context management tests
    run_test test_save_and_load_context
    run_test test_save_context_field
    run_test test_get_context_field
    run_test test_update_phase

    # Builder state management tests
    run_test test_init_builder_state
    run_test test_update_builder_status
    run_test test_get_builder_status
    run_test test_save_and_load_builder_state
    run_test test_builder_needs_assistance
    run_test test_list_builders
    run_test test_get_builder_statistics

    # Session lifecycle tests
    run_test test_archive_session
    run_test test_delete_session
    run_test test_list_sessions
    run_test test_get_latest_session

    # Error handling tests
    run_test test_load_context_nonexistent
    run_test test_get_context_field_nonexistent
    run_test test_update_builder_status_nonexistent

    # Print summary
    test_summary
}

main
