#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_logging_enhanced.sh - Tests for logging_enhanced.sh module
#
# Tests enhanced logging with semantic tags and multiple outputs

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source test framework
source "$SCRIPT_DIR/test_framework.sh"

# Source modules under test
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"

#
# Test: progress logs to workflow.log
#
test_progress_log_file() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    # Initialize state file
    echo '{"execution": {"builders": []}}' > .argo-project/state.json

    # Log progress
    progress "backend" 50 "Building" >/dev/null

    # Assert log file exists
    assert_file_exists ".argo-project/logs/workflow.log" "Workflow log should be created"

    # Assert contains progress entry
    local content=$(cat .argo-project/logs/workflow.log)
    assert_matches "$content" "PROGRESS" "Log should contain PROGRESS tag"
    assert_matches "$content" "backend" "Log should contain builder ID"
    assert_matches "$content" "50%" "Log should contain percentage"
}

#
# Test: progress outputs to stdout
#
test_progress_stdout() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    echo '{"execution": {"builders": []}}' > .argo-project/state.json

    # Capture stdout
    local output=$(progress "frontend" 75 "Testing")

    # Assert stdout contains progress info
    assert_matches "$output" "frontend" "Stdout should show builder ID"
    assert_matches "$output" "75%" "Stdout should show percentage"
}

#
# Test: log_checkpoint creates entry
#
test_log_checkpoint() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    # Log checkpoint
    log_checkpoint "requirements_done" "Requirements approved" >/dev/null

    # Assert log entry created
    assert_file_exists ".argo-project/logs/workflow.log"
    local content=$(cat .argo-project/logs/workflow.log)
    assert_matches "$content" "CHECKPOINT" "Log should contain CHECKPOINT tag"
    assert_matches "$content" "requirements_done" "Log should contain checkpoint name"
}

#
# Test: log_user_query creates entry
#
test_log_user_query() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    # Log user query
    log_user_query "Approve this plan?" >/dev/null

    # Assert log entry created
    local content=$(cat .argo-project/logs/workflow.log)
    assert_matches "$content" "USER_QUERY" "Log should contain USER_QUERY tag"
    assert_matches "$content" "Approve this plan?" "Log should contain question"
}

#
# Test: log_builder writes to workflow log
#
test_log_builder_workflow_log() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    # Log builder event
    log_builder "backend" "started" "PID: 12345" >/dev/null

    # Assert workflow log entry
    local content=$(cat .argo-project/logs/workflow.log)
    assert_matches "$content" "BUILDER" "Log should contain BUILDER tag"
    assert_matches "$content" "backend" "Log should contain builder ID"
    assert_matches "$content" "started" "Log should contain event"
}

#
# Test: log_builder writes to builder-specific log
#
test_log_builder_specific_log() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    # Log builder event
    log_builder "frontend" "completed" "Exit code: 0" >/dev/null

    # Assert builder-specific log created
    assert_file_exists ".argo-project/logs/builder-frontend.log" "Builder-specific log should exist"

    local content=$(cat .argo-project/logs/builder-frontend.log)
    assert_matches "$content" "completed" "Builder log should contain event"
}

#
# Test: log_merge creates entry
#
test_log_merge() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    # Log merge event
    log_merge "started" "Merging backend" >/dev/null

    # Assert log entry
    local content=$(cat .argo-project/logs/workflow.log)
    assert_matches "$content" "MERGE" "Log should contain MERGE tag"
    assert_matches "$content" "started" "Log should contain event"
}

#
# Test: log_state_transition records phase change
#
test_log_state_transition() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    # Log state transition
    log_state_transition "design" "execution" >/dev/null

    # Assert log entry
    local content=$(cat .argo-project/logs/workflow.log)
    assert_matches "$content" "STATE" "Log should contain STATE tag"
    assert_matches "$content" "design" "Log should contain from phase"
    assert_matches "$content" "execution" "Log should contain to phase"
}

#
# Test: multiple log entries maintain order
#
test_log_ordering() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    echo '{"execution": {"builders": []}}' > .argo-project/state.json

    # Log multiple events
    progress "backend" 0 "Starting" >/dev/null
    log_builder "backend" "started" "PID: 100" >/dev/null
    progress "backend" 50 "Building" >/dev/null
    log_builder "backend" "completed" "Success" >/dev/null
    progress "backend" 100 "Done" >/dev/null

    # Count log entries
    local lines=$(wc -l < .argo-project/logs/workflow.log | tr -d ' ')
    assert_equals "5" "$lines" "Should have 5 log entries"

    # Verify order (first line should be first event)
    local first_line=$(head -n1 .argo-project/logs/workflow.log)
    assert_matches "$first_line" "Starting" "First entry should be 'Starting'"
}

#
# Test: log directory created automatically
#
test_log_directory_creation() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    # Don't create logs directory manually
    # Log should create it

    log_checkpoint "test" "Testing auto-creation" >/dev/null

    # Assert directory and file exist
    assert_dir_exists ".argo-project/logs" "Logs directory should be created"
    assert_file_exists ".argo-project/logs/workflow.log" "Log file should be created"
}

#
# Test: timestamps are included
#
test_log_timestamps() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    log_checkpoint "test" "Test entry" >/dev/null

    # Check for ISO 8601 timestamp format
    local content=$(cat .argo-project/logs/workflow.log)
    # Should match pattern like: 2025-01-15T14:30:00Z
    if ! echo "$content" | grep -qE '[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z'; then
        test_fail "Log should contain ISO 8601 timestamp"
        return 1
    fi
}

#
# Test: progress with 0 percent
#
test_progress_zero() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    echo '{"execution": {"builders": []}}' > .argo-project/state.json

    progress "backend" 0 "Initializing" >/dev/null

    local content=$(cat .argo-project/logs/workflow.log)
    assert_matches "$content" "0%" "Should log 0%"
}

#
# Test: progress with 100 percent
#
test_progress_complete() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    echo '{"execution": {"builders": []}}' > .argo-project/state.json

    progress "frontend" 100 "Complete" >/dev/null

    local content=$(cat .argo-project/logs/workflow.log)
    assert_matches "$content" "100%" "Should log 100%"
}

#
# Run all tests
#
echo "Testing logging_enhanced.sh"
echo "==========================="

run_test test_progress_log_file
run_test test_progress_stdout
run_test test_log_checkpoint
run_test test_log_user_query
run_test test_log_builder_workflow_log
run_test test_log_builder_specific_log
run_test test_log_merge
run_test test_log_state_transition
run_test test_log_ordering
run_test test_log_directory_creation
run_test test_log_timestamps
run_test test_progress_zero
run_test test_progress_complete

# Print summary
test_summary
