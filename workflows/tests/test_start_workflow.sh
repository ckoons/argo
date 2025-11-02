#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_start_workflow.sh - Tests for start_workflow.sh
#
# Tests workflow initialization and orchestrator launch

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source test framework
source "$SCRIPT_DIR/test_framework.sh"

# Path to scripts
SETUP_TEMPLATE="$SCRIPT_DIR/../templates/project_setup.sh"
START_WORKFLOW="$SCRIPT_DIR/../start_workflow.sh"

# Helper function to cleanup orchestrator processes
cleanup_orchestrator() {
    local project_dir="$1"
    if [[ -f "$project_dir/.argo-project/orchestrator.pid" ]]; then
        local pid=$(cat "$project_dir/.argo-project/orchestrator.pid")
        kill $pid 2>/dev/null || true
        wait $pid 2>/dev/null || true
    fi
    # Also kill by pattern as backup
    pkill -f "orchestrator.sh.*$(basename $project_dir)" 2>/dev/null || true
}

#
# Test: start_workflow requires query argument
#
test_start_requires_query() {
    "$START_WORKFLOW" 2>/dev/null
    assert_failure $? "Should fail without query"
}

#
# Test: start_workflow requires project directory
#
test_start_requires_project() {
    "$START_WORKFLOW" "test query" "/nonexistent/path" 2>/dev/null
    assert_failure $? "Should fail with invalid project path"
}

#
# Test: start_workflow saves user query
#
test_start_saves_query() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Start workflow (will launch real orchestrator)
    "$START_WORKFLOW" "Build a calculator" >/dev/null 2>&1

    # Check state.json
    local query=$(jq -r '.user_query' .argo-project/state.json)
    assert_equals "Build a calculator" "$query" "Query should be saved to state"

    # Cleanup
    cleanup_orchestrator "$project_dir"
}

#
# Test: start_workflow sets initial phase
#
test_start_sets_initial_phase() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    "$START_WORKFLOW" "test" >/dev/null 2>&1

    # Verify initial state
    local phase=$(jq -r '.phase' .argo-project/state.json)
    assert_equals "init" "$phase" "Phase should be init"

    local action_owner=$(jq -r '.action_owner' .argo-project/state.json)
    assert_equals "code" "$action_owner" "Action owner should be code"

    local action_needed=$(jq -r '.action_needed' .argo-project/state.json)
    assert_equals "design_program" "$action_needed" "Action needed should be design_program"

    # Cleanup
    cleanup_orchestrator "$project_dir"
}

#
# Test: start_workflow launches orchestrator
#
test_start_launches_orchestrator() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    "$START_WORKFLOW" "test" >/dev/null 2>&1

    sleep 0.5

    # Check if orchestrator PID exists and process is running
    if [[ -f .argo-project/orchestrator.pid ]]; then
        local pid=$(cat .argo-project/orchestrator.pid)
        if ps -p "$pid" >/dev/null 2>&1; then
            assert_success 0 "Orchestrator should be launched"
        fi
    fi

    # Cleanup
    cleanup_orchestrator "$project_dir"
}

#
# Test: start_workflow returns orchestrator PID
#
test_start_returns_pid() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    local output=$("$START_WORKFLOW" "test" 2>&1)

    # Should contain PID
    if echo "$output" | grep -q "PID"; then
        assert_success 0 "Output should contain PID"
    fi

    # Cleanup
    cleanup_orchestrator "$project_dir"
}

#
# Test: start_workflow restarts orchestrator if already running (self-healing)
#
test_start_idempotent() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Start workflow first time
    "$START_WORKFLOW" "test" >/dev/null 2>&1
    sleep 0.3

    local first_pid=$(cat .argo-project/orchestrator.pid)

    # Start workflow second time - should stop old and start new
    local output=$("$START_WORKFLOW" "test2" 2>&1)
    sleep 0.3

    local second_pid=$(cat .argo-project/orchestrator.pid)

    # Should have stopped old orchestrator and started new one
    if [[ "$first_pid" != "$second_pid" ]]; then
        assert_success 0 "Should restart orchestrator (self-healing)"
    fi

    # Cleanup
    cleanup_orchestrator "$project_dir"
}

#
# Test: start_workflow with custom project path
#
test_start_with_project_path() {
    local project_dir="$TEST_TMP_DIR/custom_project"
    mkdir -p "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "custom_project" "$project_dir" >/dev/null

    "$START_WORKFLOW" "test" "$project_dir" >/dev/null 2>&1

    # Check if state was created in custom path
    if [[ -f "$project_dir/.argo-project/state.json" ]]; then
        local query=$(jq -r '.user_query' "$project_dir/.argo-project/state.json")
        assert_equals "test" "$query" "Should work with custom project path"
    fi

    # Cleanup
    cleanup_orchestrator "$project_dir"
}

#
# Test: start_workflow logs to workflow log
#
test_start_logs_event() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    "$START_WORKFLOW" "test" >/dev/null 2>&1

    sleep 0.3

    # Check for log entry (STATE tag with ORCHESTRATOR)
    if [[ -f .argo-project/logs/workflow.log ]]; then
        local log_content=$(cat .argo-project/logs/workflow.log)
        if echo "$log_content" | grep -q "ORCHESTRATOR"; then
            assert_success 0 "Should log orchestrator start"
        fi
    fi

    # Cleanup
    cleanup_orchestrator "$project_dir"
}

#
# Run all tests
#
echo "Testing start_workflow.sh"
echo "========================="

run_test test_start_requires_query
run_test test_start_requires_project
run_test test_start_saves_query
run_test test_start_sets_initial_phase
run_test test_start_launches_orchestrator
run_test test_start_returns_pid
run_test test_start_idempotent
run_test test_start_with_project_path
run_test test_start_logs_event

# Cleanup any remaining orchestrators
pkill -f "orchestrator.sh" 2>/dev/null || true

# Print summary
test_summary
