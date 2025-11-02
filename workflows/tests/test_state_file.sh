#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_state_file.sh - Tests for state_file.sh module
#
# Tests atomic state operations, locking, and validation

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source test framework
source "$SCRIPT_DIR/test_framework.sh"

# Source module under test
source "$SCRIPT_DIR/../lib/state_file.sh"

#
# Test: init_state_file creates valid state.json
#
test_init_state_file() {
    local project_name="test_project"
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir"
    cd "$project_dir"

    # Initialize state file
    init_state_file "$project_name" "$project_dir"
    local result=$?

    # Assert success
    assert_success $result "init_state_file should succeed"

    # Assert file created
    assert_file_exists ".argo-project/state.json" "state.json should be created"

    # Assert valid JSON
    jq empty ".argo-project/state.json" 2>/dev/null
    assert_success $? "state.json should be valid JSON"

    # Assert required fields
    assert_json_equals ".argo-project/state.json" ".project_name" "$project_name"
    assert_json_equals ".argo-project/state.json" ".phase" "init"
    assert_json_equals ".argo-project/state.json" ".action_owner" "ci"

    # Assert limits exist
    local limits=$(jq '.limits' ".argo-project/state.json")
    assert_not_empty "$limits" "limits should exist"
}

#
# Test: update_state writes value atomically
#
test_update_state_simple() {
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    # Create minimal state file
    echo '{"phase": "init", "action_owner": "code"}' > .argo-project/state.json

    # Update phase
    update_state "phase" "execution"
    local result=$?

    assert_success $result "update_state should succeed"
    assert_json_equals ".argo-project/state.json" ".phase" "execution"

    # Update action_owner
    update_state "action_owner" "ci"
    assert_json_equals ".argo-project/state.json" ".action_owner" "ci"
}

#
# Test: update_state handles nested paths
#
test_update_state_nested() {
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    # Create state with nested structure
    cat > .argo-project/state.json <<'EOF'
{
  "execution": {
    "builders": []
  }
}
EOF

    # Update nested field
    update_state "execution.status" "running"

    assert_json_equals ".argo-project/state.json" ".execution.status" "running"
}

#
# Test: read_state retrieves values
#
test_read_state_simple() {
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    # Create state file
    echo '{"phase": "execution", "action_owner": "code"}' > .argo-project/state.json

    # Read phase
    local phase=$(read_state "phase")
    assert_equals "execution" "$phase" "read_state should return correct value"

    # Read action_owner
    local owner=$(read_state "action_owner")
    assert_equals "code" "$owner"
}

#
# Test: read_state handles nested paths
#
test_read_state_nested() {
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    # Create nested state
    cat > .argo-project/state.json <<'EOF'
{
  "execution": {
    "builders": [
      {"id": "backend", "status": "running"}
    ]
  }
}
EOF

    # Read nested value
    local status=$(read_state "execution.builders[0].status")
    assert_equals "running" "$status"
}

#
# Test: read_state returns empty for missing field
#
test_read_state_missing() {
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    echo '{"phase": "init"}' > .argo-project/state.json

    # Read missing field
    local missing=$(read_state "nonexistent_field")
    assert_empty "$missing" "Missing field should return empty"
}

#
# Test: validate_state_file accepts valid JSON
#
test_validate_state_valid() {
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    # Create valid state
    cat > .argo-project/state.json <<'EOF'
{
  "project_name": "test",
  "phase": "init",
  "action_owner": "code",
  "action_needed": "start"
}
EOF

    validate_state_file
    assert_success $? "validate_state_file should succeed for valid JSON"
}

#
# Test: validate_state_file rejects invalid JSON
#
test_validate_state_invalid() {
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    # Create invalid JSON (missing closing brace)
    echo '{"phase": "init"' > .argo-project/state.json

    validate_state_file
    assert_failure $? "validate_state_file should fail for invalid JSON"
}

#
# Test: validate_state_file checks required fields
#
test_validate_state_missing_fields() {
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    # Create JSON missing required fields
    echo '{"some_field": "value"}' > .argo-project/state.json

    validate_state_file
    assert_failure $? "validate_state_file should fail for missing required fields"
}

#
# Test: with_state_lock provides exclusive access
#
test_with_state_lock() {
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    echo '{"counter": 0}' > .argo-project/state.json

    # Function that increments counter
    increment_counter() {
        local current=$(read_state "counter")
        local next=$((current + 1))
        update_state "counter" "$next"
    }

    # Export function so it's available in subshell
    export -f increment_counter

    # Run with lock
    with_state_lock increment_counter
    assert_success $? "with_state_lock should succeed"

    # Verify counter incremented
    local final=$(read_state "counter")
    assert_equals "1" "$final" "Counter should be incremented"
}

#
# Test: update_state is atomic (no .tmp file left behind)
#
test_update_state_atomic() {
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    echo '{"phase": "init"}' > .argo-project/state.json

    # Update state
    update_state "phase" "execution"

    # Assert no temp file left
    if [ -f ".argo-project/state.json.tmp" ]; then
        test_fail "Temp file should not exist after atomic update"
        return 1
    fi

    # Assert state file exists and is valid
    assert_file_exists ".argo-project/state.json"
    jq empty ".argo-project/state.json" 2>/dev/null
    assert_success $? "state.json should be valid after update"
}

#
# Test: concurrent updates with locking
#
test_concurrent_updates() {
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    echo '{"counter": 0}' > .argo-project/state.json

    # Function that increments counter with delay
    slow_increment() {
        with_state_lock bash -c '
            current=$(jq -r ".counter" .argo-project/state.json)
            sleep 0.1  # Simulate slow operation
            next=$((current + 1))
            jq ".counter = $next" .argo-project/state.json > .argo-project/state.json.tmp
            mv .argo-project/state.json.tmp .argo-project/state.json
        '
    }

    # Run 5 concurrent increments
    for i in {1..5}; do
        slow_increment &
    done

    # Wait for all to complete
    wait

    # Verify counter is 5 (not less due to race conditions)
    local final=$(read_state "counter")
    assert_equals "5" "$final" "Concurrent updates should not race"
}

#
# Test: update_state handles special characters
#
test_update_state_special_chars() {
    local project_dir="$TEST_TMP_DIR/project"

    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    echo '{"message": ""}' > .argo-project/state.json

    # Update with special characters
    update_state "message" "Test with \"quotes\" and 'apostrophes'"

    local result=$(read_state "message")
    assert_matches "$result" "quotes" "Special characters should be preserved"
}

#
# Run all tests
#
echo "Testing state_file.sh"
echo "===================="

run_test test_init_state_file
run_test test_update_state_simple
run_test test_update_state_nested
run_test test_read_state_simple
run_test test_read_state_nested
run_test test_read_state_missing
run_test test_validate_state_valid
run_test test_validate_state_invalid
run_test test_validate_state_missing_fields
run_test test_with_state_lock
run_test test_update_state_atomic
run_test test_concurrent_updates
run_test test_update_state_special_chars

# Print summary
test_summary
