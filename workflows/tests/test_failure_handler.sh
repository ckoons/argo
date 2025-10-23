#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_failure_handler.sh - Unit tests for failure-handler.sh
#
# Tests Phase 5.1: Builder failure handling

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/../lib"

# Load test framework
source "$SCRIPT_DIR/test_framework.sh"

# Load required libraries
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/failure-handler.sh"

# Mock ci function for testing
ci() {
    # Read from stdin if no arguments (heredoc usage)
    local prompt
    if [[ $# -eq 0 ]]; then
        prompt=$(cat)
    else
        prompt="$1"
    fi

    # Return mock analysis based on prompt content
    if echo "$prompt" | grep -q "analyze merge conflicts"; then
        # This is for conflict-resolver, not failure-handler
        echo '{"has_conflicts":false}'
    elif echo "$prompt" | grep -q "Analyze this failure"; then
        # Mock failure analysis
        cat <<'EOF'
{
  "problem_type": "bad_tests",
  "explanation": "Tests are too strict",
  "recommended_action": "refactor_tests",
  "specific_fixes": ["Relax timeout constraints", "Allow flexible ordering"]
}
EOF
    elif echo "$prompt" | grep -q "Create better, more realistic tests"; then
        # Mock new tests
        echo '["test_basic_functionality", "test_error_handling", "test_edge_cases"]'
    else
        echo '{}'
    fi
}
export -f ci

#
# Test Setup
#

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

    # Create test design directory
    export TEST_DESIGN_DIR="$TEST_TMP_DIR/design"
    mkdir -p "$TEST_DESIGN_DIR"

    # Create mock requirements.md
    cat > "$TEST_DESIGN_DIR/requirements.md" <<'EOF'
# Test Requirements

Build a calculator with basic operations.
EOF

    # Create mock architecture.md
    cat > "$TEST_DESIGN_DIR/architecture.md" <<'EOF'
# Architecture

Simple calculator with add/subtract/multiply/divide functions.
EOF

    # Create mock tasks.json
    cat > "$TEST_DESIGN_DIR/tasks.json" <<'EOF'
[
  {
    "task_id": "argo-A",
    "description": "Implement calculator",
    "components": ["calc.sh"],
    "tests": ["test_add", "test_subtract", "test_multiply"]
  }
]
EOF
}

#
# Failure Analysis Tests
#

test_analyze_builder_failure() {
    local session_id=$(create_session "test_failure")
    local builder_id="argo-A"

    # Initialize builder with failure state
    init_builder_state "$session_id" "$builder_id" "feature/test" "Test task"

    # Update builder with failure information
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"
    local temp_file=$(mktemp)
    jq '.status = "failed" | .tests_failed_count = 2 | .failed_tests = ["test_add", "test_multiply"]' \
       "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"

    # Analyze failure
    local analysis=$(analyze_builder_failure "$session_id" "$builder_id" "$TEST_DESIGN_DIR")

    assert_not_empty "$analysis" "Failure analysis should not be empty" || return 1

    # Verify JSON fields
    local builder_id_check=$(echo "$analysis" | jq -r '.builder_id')
    assert_equals "$builder_id" "$builder_id_check" "Builder ID in analysis" || return 1

    local tests_failed=$(echo "$analysis" | jq -r '.tests_failed')
    assert_equals "2" "$tests_failed" "Failed test count" || return 1
}

test_analyze_builder_failure_nonexistent() {
    local session_id=$(create_session "test_nonexistent")

    # Analyze nonexistent builder
    local analysis=$(analyze_builder_failure "$session_id" "nonexistent" "$TEST_DESIGN_DIR" 2>/dev/null)

    # Should return error JSON
    local error=$(echo "$analysis" | jq -r '.error')
    assert_matches "$error" "Builder state file not found" "Error message" || return 1
}

test_analyze_test_failures() {
    local session_id=$(create_session "test_analysis")
    local builder_id="argo-A"

    # Initialize builder
    init_builder_state "$session_id" "$builder_id" "feature/test" "Test task"

    # Set failure state
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"
    local temp_file=$(mktemp)
    jq '.status = "failed" | .tests_failed_count = 1' \
       "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"

    # Analyze test failures (uses mock ci)
    local analysis=$(analyze_test_failures "$session_id" "$builder_id" "$TEST_DESIGN_DIR")

    assert_not_empty "$analysis" "Test failure analysis" || return 1

    # Verify AI analysis fields
    local problem_type=$(echo "$analysis" | jq -r '.problem_type')
    assert_equals "bad_tests" "$problem_type" "Problem type from AI" || return 1

    local recommended_action=$(echo "$analysis" | jq -r '.recommended_action')
    assert_equals "refactor_tests" "$recommended_action" "Recommended action" || return 1
}

#
# Test Refactoring Tests
#

test_refactor_builder_tests() {
    local session_id=$(create_session "test_refactor")
    local builder_id="argo-A"

    # Initialize builder
    init_builder_state "$session_id" "$builder_id" "feature/test" "Test task"

    # Set failure state
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"
    local temp_file=$(mktemp)
    jq '.status = "failed" | .tests_failed_count = 2' \
       "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"

    # Refactor tests
    refactor_builder_tests "$session_id" "$builder_id" "$TEST_DESIGN_DIR" >/dev/null 2>&1
    local result=$?

    assert_success $result "Test refactoring should succeed" || return 1

    # Verify tasks.json was updated
    local new_tests=$(jq -r '.[0].tests | join(",")' "$TEST_DESIGN_DIR/tasks.json")
    assert_matches "$new_tests" "test_basic_functionality" "New tests include basic functionality" || return 1
}

#
# Builder Respawn Tests
#

test_respawn_builder() {
    local session_id=$(create_session "test_respawn")
    local builder_id="argo-A"
    local build_dir="$TEST_TMP_DIR/build"
    mkdir -p "$build_dir"

    # Initialize builder
    init_builder_state "$session_id" "$builder_id" "feature/test" "Test task"

    # Mock builder_ci_loop.sh
    local lib_dir_parent=$(dirname "$LIB_DIR")
    mkdir -p "$lib_dir_parent/lib"
    cat > "$lib_dir_parent/lib/builder_ci_loop.sh" <<'EOF'
#!/bin/bash
# Mock builder CI loop - just exit successfully
exit 0
EOF
    chmod +x "$lib_dir_parent/lib/builder_ci_loop.sh"

    # Respawn builder
    local new_pid=$(respawn_builder "$session_id" "$builder_id" "$TEST_DESIGN_DIR" "$build_dir" 2>/dev/null)

    # Verify respawn count was incremented
    local respawn_count=$(jq -r '.respawn_count' "$SESSIONS_DIR/$session_id/builders/$builder_id.json")
    assert_equals "1" "$respawn_count" "Respawn count incremented" || return 1

    # Verify status updated
    local status=$(jq -r '.status' "$SESSIONS_DIR/$session_id/builders/$builder_id.json")
    assert_equals "respawning" "$status" "Status set to respawning" || return 1

    # Wait for mock process
    wait "$new_pid" 2>/dev/null || true
}

test_respawn_builder_max_attempts() {
    local session_id=$(create_session "test_max_respawn")
    local builder_id="argo-A"

    # Initialize builder
    init_builder_state "$session_id" "$builder_id" "feature/test" "Test task"

    # Set respawn count to max
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"
    local temp_file=$(mktemp)
    jq --arg max "$ARGO_MAX_RESPAWN_ATTEMPTS" \
       '.respawn_count = ($max | tonumber)' \
       "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"

    # Try to respawn (should fail)
    respawn_builder "$session_id" "$builder_id" "$TEST_DESIGN_DIR" "$TEST_TMP_DIR/build" 2>/dev/null
    local result=$?

    assert_failure $result "Should fail when max respawns reached" || return 1
}

#
# Handle Failure Tests
#

test_handle_builder_failure_success() {
    local session_id=$(create_session "test_handle")
    local builder_id="argo-A"
    local build_dir="$TEST_TMP_DIR/build"
    mkdir -p "$build_dir"

    # Initialize builder
    init_builder_state "$session_id" "$builder_id" "feature/test" "Test task"

    # Set failure state
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"
    local temp_file=$(mktemp)
    jq '.status = "failed" | .tests_failed_count = 1' \
       "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"

    # Mock builder_ci_loop.sh
    local lib_dir_parent=$(dirname "$LIB_DIR")
    mkdir -p "$lib_dir_parent/lib"
    cat > "$lib_dir_parent/lib/builder_ci_loop.sh" <<'EOF'
#!/bin/bash
exit 0
EOF
    chmod +x "$lib_dir_parent/lib/builder_ci_loop.sh"

    # Handle failure
    handle_builder_failure "$session_id" "$builder_id" "$TEST_DESIGN_DIR" "$build_dir" >/dev/null 2>&1
    local result=$?

    assert_success $result "Failure handling should succeed" || return 1

    # Wait for background process
    sleep 1
}

test_escalate_to_user() {
    local session_id=$(create_session "test_escalate")
    local builder_id="argo-A"

    # Initialize builder
    init_builder_state "$session_id" "$builder_id" "feature/test" "Test task"

    # Set failure info
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"
    local temp_file=$(mktemp)
    jq '.status = "failed" | .tests_failed_count = 3 | .task_description = "Test escalation"' \
       "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"

    # Escalate to user
    escalate_to_user "$session_id" "$builder_id" "$TEST_DESIGN_DIR" >/dev/null 2>&1

    # Verify needs_assistance flag set
    local needs_assistance=$(jq -r '.needs_assistance' "$builder_file")
    assert_equals "true" "$needs_assistance" "Needs assistance flag" || return 1

    # Verify status updated
    local status=$(jq -r '.status' "$builder_file")
    assert_equals "needs_assistance" "$status" "Status set to needs_assistance" || return 1
}

#
# Run all tests
#

main() {
    echo "Running failure-handler.sh unit tests..."

    # Failure analysis tests
    run_test test_analyze_builder_failure
    run_test test_analyze_builder_failure_nonexistent
    run_test test_analyze_test_failures

    # Test refactoring tests
    run_test test_refactor_builder_tests

    # Builder respawn tests
    run_test test_respawn_builder
    run_test test_respawn_builder_max_attempts

    # Handle failure tests
    run_test test_handle_builder_failure_success
    run_test test_escalate_to_user

    # Print summary
    test_summary
}

main
