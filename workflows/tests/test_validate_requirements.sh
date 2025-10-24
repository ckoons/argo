#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_validate_requirements.sh - Unit tests for validate_requirements DC tool
#
# Tests deterministic validation of requirements completeness

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/../lib"
TOOLS_DIR="$SCRIPT_DIR/../tools"

# Load test framework
source "$SCRIPT_DIR/test_framework.sh"
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/context-manager.sh"

# Test: Valid requirements (platform + features)
test_valid_requirements() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{"requirements":{"platform":"web","features":["calc"]}}'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_requirements" "$context_file"
    assert_success $? "Should pass validation" || return 1

    local validated=$(jq -r '.requirements.validated' "$context_file")
    assert_equals "true" "$validated" "Should mark as validated" || return 1

    local errors=$(jq -r '.requirements.validation_errors | length' "$context_file")
    assert_equals "0" "$errors" "Should have no errors" || return 1
}

# Test: Missing platform
test_missing_platform() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{"requirements":{"features":["calc"]}}'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_requirements" "$context_file"
    assert_failure $? "Should fail validation" || return 1

    local validated=$(jq -r '.requirements.validated' "$context_file")
    assert_equals "false" "$validated" "Should mark as not validated" || return 1

    local errors=$(jq -r '.requirements.validation_errors[]' "$context_file")
    echo "$errors" | grep -q "Platform not specified"
    assert_success $? "Should have platform error" || return 1
}

# Test: Missing features
test_missing_features() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{"requirements":{"platform":"web"}}'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_requirements" "$context_file"
    assert_failure $? "Should fail validation" || return 1

    local validated=$(jq -r '.requirements.validated' "$context_file")
    assert_equals "false" "$validated" "Should mark as not validated" || return 1

    local errors=$(jq -r '.requirements.validation_errors[]' "$context_file")
    echo "$errors" | grep -q "No features specified"
    assert_success $? "Should have features error" || return 1
}

# Test: Empty requirements
test_empty_requirements() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_requirements" "$context_file"
    assert_failure $? "Should fail validation" || return 1

    local error_count=$(jq -r '.requirements.validation_errors | length' "$context_file")
    [[ $error_count -ge 1 ]] || { echo "Should have at least 1 error"; return 1; }
}

# Test: Context file updates correctly
test_context_updates() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{"requirements":{"platform":"web","features":["calc"]}}'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_requirements" "$context_file"

    # Check all updated fields
    local validated=$(jq 'has("requirements") and (.requirements | has("validated"))' "$context_file")
    assert_equals "true" "$validated" "Should update validated field" || return 1

    local has_errors=$(jq '.requirements | has("validation_errors")' "$context_file")
    assert_equals "true" "$has_errors" "Should have validation_errors field" || return 1
}

# Test: Validation errors array populated
test_errors_array() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_requirements" "$context_file"

    local errors=$(jq -r '.requirements.validation_errors' "$context_file")
    echo "$errors" | jq '.' >/dev/null 2>&1
    assert_success $? "Errors should be valid JSON array" || return 1

    local error_count=$(echo "$errors" | jq 'length')
    [[ $error_count -ge 1 ]] || { echo "Should have errors"; return 1; }
}

# Test: Exit codes (0=valid, 1=invalid)
test_exit_codes() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    local context_file=$(get_context_file "$session_id")

    # Invalid requirements
    "$TOOLS_DIR/validate_requirements" "$context_file"
    local invalid_exit=$?

    # Valid requirements
    update_context "$session_id" '{"requirements":{"platform":"web","features":["calc"]}}'
    "$TOOLS_DIR/validate_requirements" "$context_file"
    local valid_exit=$?

    assert_failure $invalid_exit "Invalid should return non-zero" || return 1
    assert_success $valid_exit "Valid should return zero" || return 1
}

# Test: JSON structure integrity
test_json_integrity() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{"requirements":{"platform":"web","features":["calc"]}}'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_requirements" "$context_file"

    # File should still be valid JSON
    jq '.' "$context_file" >/dev/null 2>&1
    assert_success $? "Should maintain JSON validity" || return 1

    # Should have all original fields
    local has_session_id=$(jq 'has("session_id")' "$context_file")
    assert_equals "true" "$has_session_id" "Should preserve session_id" || return 1
}

# Test: Missing context file
test_missing_file() {
    "$TOOLS_DIR/validate_requirements" "/tmp/nonexistent_context.json" 2>/dev/null
    assert_failure $? "Should fail with missing file" || return 1
}

# Test: Idempotent (can run multiple times)
test_idempotent() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{"requirements":{"platform":"web","features":["calc"]}}'

    local context_file=$(get_context_file "$session_id")

    # Run twice
    "$TOOLS_DIR/validate_requirements" "$context_file"
    local first=$(jq -r '.requirements.validated' "$context_file")

    "$TOOLS_DIR/validate_requirements" "$context_file"
    local second=$(jq -r '.requirements.validated' "$context_file")

    assert_equals "$first" "$second" "Should be idempotent" || return 1
}

# Main test execution
main() {
    echo "Running validate_requirements tests..."
    echo ""

    run_test test_valid_requirements "Valid requirements"
    run_test test_missing_platform "Missing platform"
    run_test test_missing_features "Missing features"
    run_test test_empty_requirements "Empty requirements"
    run_test test_context_updates "Context file updates"
    run_test test_errors_array "Errors array populated"
    run_test test_exit_codes "Exit codes correct"
    run_test test_json_integrity "JSON integrity maintained"
    run_test test_missing_file "Missing file error"
    run_test test_idempotent "Idempotent execution"

    test_summary
}

main
