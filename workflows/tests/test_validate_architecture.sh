#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_validate_architecture.sh - Unit tests for validate_architecture DC tool
#
# Tests deterministic validation of architecture completeness

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/../lib"
TOOLS_DIR="$SCRIPT_DIR/../tools"

# Load test framework
source "$SCRIPT_DIR/test_framework.sh"
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/context-manager.sh"

# Test: Valid architecture (components + requirements converged)
test_valid_architecture() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"converged":true},
        "architecture":{"components":[{"name":"UI"},{"name":"Logic"}]}
    }'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_architecture" "$context_file"
    assert_success $? "Should pass validation" || return 1

    local validated=$(jq -r '.architecture.validated' "$context_file")
    assert_equals "true" "$validated" "Should mark as validated" || return 1
}

# Test: No components specified
test_no_components() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{"requirements":{"converged":true}}'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_architecture" "$context_file"
    assert_failure $? "Should fail validation" || return 1

    local errors=$(jq -r '.architecture.validation_errors[]' "$context_file")
    echo "$errors" | grep -q "No components specified"
    assert_success $? "Should have components error" || return 1
}

# Test: Requirements not converged
test_requirements_not_converged() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"converged":false},
        "architecture":{"components":[{"name":"UI"}]}
    }'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_architecture" "$context_file"
    assert_failure $? "Should fail validation" || return 1

    local errors=$(jq -r '.architecture.validation_errors[]' "$context_file")
    echo "$errors" | grep -q "Requirements not converged"
    assert_success $? "Should have requirements error" || return 1
}

# Test: Context file updates correctly
test_context_updates() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"converged":true},
        "architecture":{"components":[{"name":"UI"}]}
    }'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_architecture" "$context_file"

    local has_validated=$(jq '.architecture | has("validated")' "$context_file")
    assert_equals "true" "$has_validated" "Should have validated field" || return 1

    local has_errors=$(jq '.architecture | has("validation_errors")' "$context_file")
    assert_equals "true" "$has_errors" "Should have validation_errors field" || return 1
}

# Test: Single-component system (interfaces optional)
test_single_component() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"converged":true},
        "architecture":{"components":[{"name":"Monolith"}]}
    }'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_architecture" "$context_file"
    assert_success $? "Single component should be valid" || return 1
}

# Test: Multi-component system
test_multi_component() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"converged":true},
        "architecture":{"components":[{"name":"UI"},{"name":"API"},{"name":"DB"}]}
    }'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_architecture" "$context_file"
    assert_success $? "Multiple components should be valid" || return 1
}

# Test: Exit codes
test_exit_codes() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    local context_file=$(get_context_file "$session_id")

    # Invalid (no components)
    update_context "$session_id" '{"requirements":{"converged":true}}'
    "$TOOLS_DIR/validate_architecture" "$context_file"
    local invalid_exit=$?

    # Valid
    update_context "$session_id" '{
        "requirements":{"converged":true},
        "architecture":{"components":[{"name":"UI"}]}
    }'
    "$TOOLS_DIR/validate_architecture" "$context_file"
    local valid_exit=$?

    assert_failure $invalid_exit "Invalid should return non-zero" || return 1
    assert_success $valid_exit "Valid should return zero" || return 1
}

# Test: JSON integrity
test_json_integrity() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"converged":true},
        "architecture":{"components":[{"name":"UI"}]}
    }'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_architecture" "$context_file"

    jq '.' "$context_file" >/dev/null 2>&1
    assert_success $? "Should maintain JSON validity" || return 1
}

# Test: Idempotent
test_idempotent() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"converged":true},
        "architecture":{"components":[{"name":"UI"}]}
    }'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_architecture" "$context_file"
    local first=$(jq -r '.architecture.validated' "$context_file")

    "$TOOLS_DIR/validate_architecture" "$context_file"
    local second=$(jq -r '.architecture.validated' "$context_file")

    assert_equals "$first" "$second" "Should be idempotent" || return 1
}

# Test: Missing file
test_missing_file() {
    "$TOOLS_DIR/validate_architecture" "/tmp/nonexistent_context.json" 2>/dev/null
    assert_failure $? "Should fail with missing file" || return 1
}

# Test: Validation errors array
test_errors_array() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_architecture" "$context_file"

    local errors=$(jq -r '.architecture.validation_errors' "$context_file")
    echo "$errors" | jq '.' >/dev/null 2>&1
    assert_success $? "Errors should be valid JSON array" || return 1
}

# Test: Both requirements and components missing
test_multiple_errors() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/validate_architecture" "$context_file"

    local error_count=$(jq -r '.architecture.validation_errors | length' "$context_file")
    [[ $error_count -eq 2 ]] || { echo "Should have 2 errors, got $error_count"; return 1; }
}

# Main test execution
main() {
    echo "Running validate_architecture tests..."
    echo ""

    run_test test_valid_architecture "Valid architecture"
    run_test test_no_components "No components"
    run_test test_requirements_not_converged "Requirements not converged"
    run_test test_context_updates "Context file updates"
    run_test test_single_component "Single component system"
    run_test test_multi_component "Multi-component system"
    run_test test_exit_codes "Exit codes correct"
    run_test test_json_integrity "JSON integrity"
    run_test test_idempotent "Idempotent execution"
    run_test test_missing_file "Missing file error"
    run_test test_errors_array "Errors array"
    run_test test_multiple_errors "Multiple errors"

    test_summary
}

main
