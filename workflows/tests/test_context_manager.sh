#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_context_manager.sh - Unit tests for context-manager.sh
#
# Tests all context management functions for the conversational workflow system

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/../lib"

# Load test framework
source "$SCRIPT_DIR/test_framework.sh"

# Load modules to test
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/context-manager.sh"

# Test: create_initial_context creates valid structure
test_create_initial_context() {
    local session_id=$(create_session "test_project")
    local context_file=$(create_initial_context "$session_id" "test_project")

    assert_file_exists "$context_file" "Context file should be created" || return 1

    # Check required fields
    assert_json_equals "$context_file" ".session_id" "$session_id" "Session ID" || return 1
    assert_json_equals "$context_file" ".project_name" "test_project" "Project name" || return 1

    # Check structure
    local has_requirements=$(jq 'has("requirements")' "$context_file")
    local has_architecture=$(jq 'has("architecture")' "$context_file")
    local has_conversation=$(jq 'has("conversation")' "$context_file")
    local has_metadata=$(jq 'has("metadata")' "$context_file")

    assert_equals "true" "$has_requirements" "Should have requirements section" || return 1
    assert_equals "true" "$has_architecture" "Should have architecture section" || return 1
    assert_equals "true" "$has_conversation" "Should have conversation section" || return 1
    assert_equals "true" "$has_metadata" "Should have metadata section" || return 1
}

# Test: update_context merges JSON correctly
test_update_context() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    update_context "$session_id" '{"requirements":{"platform":"web"}}'

    local platform=$(get_context_value "$session_id" "requirements.platform")
    assert_equals "web" "$platform" "Should update nested field" || return 1
}

# Test: update_context is atomic
test_update_context_atomic() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    local context_file=$(get_context_file "$session_id")

    # Corrupt the update (invalid JSON)
    update_context "$session_id" '{"invalid": json}' 2>/dev/null

    # Original file should still be valid JSON
    jq '.' "$context_file" >/dev/null 2>&1
    assert_success $? "Context file should remain valid after failed update" || return 1
}

# Test: load_context returns valid JSON
test_load_context() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    local context=$(load_context "$session_id")

    # Should be valid JSON
    echo "$context" | jq '.' >/dev/null 2>&1
    assert_success $? "Should return valid JSON" || return 1

    # Should contain session_id
    local sid=$(echo "$context" | jq -r '.session_id')
    assert_equals "$session_id" "$sid" "Should contain correct session_id" || return 1
}

# Test: get_context_value retrieves nested paths
test_get_context_value_nested() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    update_context "$session_id" '{"requirements":{"platform":"web","features":["calc"]}}'

    local platform=$(get_context_value "$session_id" "requirements.platform")
    assert_equals "web" "$platform" "Should retrieve nested value" || return 1

    # Test array access
    local features=$(get_context_value "$session_id" "requirements.features")
    echo "$features" | grep -q "calc"
    assert_success $? "Should retrieve array values" || return 1
}

# Test: get_context_file returns correct path
test_get_context_file() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    local context_file=$(get_context_file "$session_id")
    local expected="$SESSIONS_DIR/$session_id/context.json"

    assert_equals "$expected" "$context_file" "Should return correct path" || return 1
    assert_file_exists "$context_file" "File should exist" || return 1
}

# Test: add_conversation_message adds user message
test_add_conversation_message_user() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    add_conversation_message "$session_id" "user" "Hello, I want to build a calculator"

    local messages=$(get_context_value "$session_id" "conversation.messages")
    echo "$messages" | grep -q "Hello, I want to build a calculator"
    assert_success $? "Should add user message" || return 1

    echo "$messages" | grep -q "\"role\":\"user\""
    assert_success $? "Should have correct role" || return 1
}

# Test: add_conversation_message adds assistant message
test_add_conversation_message_assistant() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    add_conversation_message "$session_id" "assistant" "I can help with that"

    local messages=$(get_context_value "$session_id" "conversation.messages")
    echo "$messages" | grep -q "I can help with that"
    assert_success $? "Should add assistant message" || return 1

    echo "$messages" | grep -q "\"role\":\"assistant\""
    assert_success $? "Should have correct role" || return 1
}

# Test: add_decision appends to array
test_add_decision() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    add_decision "$session_id" "Requirements converged and approved"
    add_decision "$session_id" "Architecture converged and approved"

    local decisions=$(get_context_value "$session_id" "conversation.decisions")

    echo "$decisions" | grep -q "Requirements converged and approved"
    assert_success $? "Should add first decision" || return 1

    echo "$decisions" | grep -q "Architecture converged and approved"
    assert_success $? "Should add second decision" || return 1
}

# Test: update_current_phase updates phase and history
test_update_current_phase() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    update_current_phase "$session_id" "requirements"
    update_current_phase "$session_id" "architecture"

    local current=$(get_context_value "$session_id" "current_phase")
    assert_equals "architecture" "$current" "Should update current phase" || return 1

    local history=$(get_context_value "$session_id" "phase_history")
    echo "$history" | grep -q "requirements"
    assert_success $? "Should track phase history" || return 1
}

# Test: Nested JSON updates
test_nested_json_updates() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    update_context "$session_id" '{"architecture":{"components":[{"name":"UI"},{"name":"Logic"}]}}'

    local components=$(get_context_value "$session_id" "architecture.components")
    echo "$components" | grep -q "UI"
    assert_success $? "Should handle nested arrays" || return 1

    echo "$components" | grep -q "Logic"
    assert_success $? "Should handle multiple array items" || return 1
}

# Test: Array operations (append)
test_array_append() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    update_context "$session_id" '{"requirements":{"features":["add"]}}'
    local context_file=$(get_context_file "$session_id")

    # Append to array
    local temp_file=$(mktemp)
    jq '.requirements.features += ["subtract"]' "$context_file" > "$temp_file"
    mv "$temp_file" "$context_file"

    local features=$(get_context_value "$session_id" "requirements.features")
    echo "$features" | grep -q "add"
    assert_success $? "Should preserve existing items" || return 1

    echo "$features" | grep -q "subtract"
    assert_success $? "Should append new items" || return 1
}

# Test: Timestamp updates
test_timestamp_updates() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    sleep 1
    update_context "$session_id" '{"requirements":{"platform":"web"}}'

    local timestamp=$(get_context_value "$session_id" "metadata.last_updated")
    assert_not_empty "$timestamp" "Should update timestamp" || return 1

    # Should be ISO 8601 format
    echo "$timestamp" | grep -qE "[0-9]{4}-[0-9]{2}-[0-9]{2}T"
    assert_success $? "Should use ISO 8601 format" || return 1
}

# Test: Error handling - missing file
test_error_missing_file() {
    load_context "nonexistent_session" 2>/dev/null
    assert_failure $? "Should fail with missing session" || return 1
}

# Test: Error handling - invalid JSON in context
test_error_invalid_json() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    local context_file=$(get_context_file "$session_id")

    # Corrupt the file
    echo "{ invalid json }" > "$context_file"

    get_context_value "$session_id" "session_id" 2>/dev/null
    assert_failure $? "Should fail with invalid JSON" || return 1
}

# Test: File permissions
test_file_permissions() {
    local session_id=$(create_session "test")
    local context_file=$(create_initial_context "$session_id" "test")

    # File should be readable and writable
    [[ -r "$context_file" ]] || { echo "File not readable"; return 1; }
    [[ -w "$context_file" ]] || { echo "File not writable"; return 1; }

    assert_success 0 "Context file should have correct permissions" || return 1
}

# Test: Multiple conversation messages
test_multiple_messages() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"

    add_conversation_message "$session_id" "user" "Message 1"
    add_conversation_message "$session_id" "assistant" "Response 1"
    add_conversation_message "$session_id" "user" "Message 2"

    local context_file=$(get_context_file "$session_id")
    local msg_count=$(jq '.conversation.messages | length' "$context_file")

    assert_equals "3" "$msg_count" "Should have 3 messages" || return 1
}

# Test: Context validation (required fields)
test_context_validation() {
    local session_id=$(create_session "test")
    local context_file=$(create_initial_context "$session_id" "test")

    # Check all required fields exist
    local has_session_id=$(jq 'has("session_id")' "$context_file")
    local has_project_name=$(jq 'has("project_name")' "$context_file")
    local has_current_phase=$(jq 'has("current_phase")' "$context_file")

    assert_equals "true" "$has_session_id" "Should have session_id" || return 1
    assert_equals "true" "$has_project_name" "Should have project_name" || return 1
    assert_equals "true" "$has_current_phase" "Should have current_phase" || return 1
}

# Main test execution
main() {
    echo "Running context-manager tests..."
    echo ""

    run_test test_create_initial_context "Create initial context"
    run_test test_update_context "Update context (merge)"
    run_test test_update_context_atomic "Update context (atomic)"
    run_test test_load_context "Load context"
    run_test test_get_context_value_nested "Get nested value"
    run_test test_get_context_file "Get context file path"
    run_test test_add_conversation_message_user "Add user message"
    run_test test_add_conversation_message_assistant "Add assistant message"
    run_test test_add_decision "Add decision"
    run_test test_update_current_phase "Update current phase"
    run_test test_nested_json_updates "Nested JSON updates"
    run_test test_array_append "Array append operations"
    run_test test_timestamp_updates "Timestamp updates"
    run_test test_error_missing_file "Error: missing file"
    run_test test_error_invalid_json "Error: invalid JSON"
    run_test test_file_permissions "File permissions"
    run_test test_multiple_messages "Multiple messages"
    run_test test_context_validation "Context validation"

    test_summary
}

main
