#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_generate_tasks.sh - Unit tests for generate_tasks DC tool
#
# Tests task breakdown generation from architecture

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/../lib"
TOOLS_DIR="$SCRIPT_DIR/../tools"

# Load test framework
source "$SCRIPT_DIR/test_framework.sh"
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/context-manager.sh"

# Test: Generates valid tasks.json
test_generates_valid_json() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web","converged":true},
        "architecture":{"components":[{"name":"UI","description":"User interface"}],"validated":true}
    }'

    local context_file=$(get_context_file "$session_id")

    "$TOOLS_DIR/generate_tasks" "$context_file"
    assert_success $? "Should generate successfully" || return 1

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")
    assert_file_exists "$tasks_file" "Tasks file should be created" || return 1

    jq '.' "$tasks_file" >/dev/null 2>&1
    assert_success $? "Tasks file should be valid JSON" || return 1
}

# Test: Creates setup task
test_creates_setup_task() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web"},
        "architecture":{"components":[{"name":"UI"}]}
    }'

    local context_file=$(get_context_file "$session_id")
    "$TOOLS_DIR/generate_tasks" "$context_file"

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")
    local setup=$(jq '.tasks[] | select(.id == "setup")' "$tasks_file")

    assert_not_empty "$setup" "Should have setup task" || return 1

    local setup_type=$(echo "$setup" | jq -r '.type')
    assert_equals "setup" "$setup_type" "Setup task should have correct type" || return 1
}

# Test: Creates implementation tasks for each component
test_creates_implementation_tasks() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web"},
        "architecture":{"components":[{"name":"UI Component"},{"name":"API Component"}]}
    }'

    local context_file=$(get_context_file "$session_id")
    "$TOOLS_DIR/generate_tasks" "$context_file"

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")
    local impl_count=$(jq '[.tasks[] | select(.type == "implementation")] | length' "$tasks_file")

    assert_equals "2" "$impl_count" "Should have 2 implementation tasks" || return 1
}

# Test: Creates test tasks for each component
test_creates_test_tasks() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web"},
        "architecture":{"components":[{"name":"UI"},{"name":"API"}]}
    }'

    local context_file=$(get_context_file "$session_id")
    "$TOOLS_DIR/generate_tasks" "$context_file"

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")
    local test_count=$(jq '[.tasks[] | select(.type == "test")] | length' "$tasks_file")

    assert_equals "2" "$test_count" "Should have 2 test tasks" || return 1
}

# Test: Creates integration task (multi-component)
test_creates_integration_multi() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web"},
        "architecture":{"components":[{"name":"UI"},{"name":"API"}]}
    }'

    local context_file=$(get_context_file "$session_id")
    "$TOOLS_DIR/generate_tasks" "$context_file"

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")
    local integration=$(jq '.tasks[] | select(.id == "integration")' "$tasks_file")

    assert_not_empty "$integration" "Should have integration task for multi-component" || return 1
}

# Test: No integration task (single-component)
test_no_integration_single() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web"},
        "architecture":{"components":[{"name":"Monolith"}]}
    }'

    local context_file=$(get_context_file "$session_id")
    "$TOOLS_DIR/generate_tasks" "$context_file"

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")
    local integration=$(jq '.tasks[] | select(.id == "integration")' "$tasks_file")

    assert_empty "$integration" "Should not have integration task for single component" || return 1
}

# Test: Creates documentation task
test_creates_documentation() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web"},
        "architecture":{"components":[{"name":"UI"}]}
    }'

    local context_file=$(get_context_file "$session_id")
    "$TOOLS_DIR/generate_tasks" "$context_file"

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")
    local docs=$(jq '.tasks[] | select(.id == "documentation")' "$tasks_file")

    assert_not_empty "$docs" "Should have documentation task" || return 1
}

# Test: Dependency graph correct
test_dependency_graph() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web"},
        "architecture":{"components":[{"name":"UI"}]}
    }'

    local context_file=$(get_context_file "$session_id")
    "$TOOLS_DIR/generate_tasks" "$context_file"

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")

    # Implementation should depend on setup
    local impl_deps=$(jq -r '.tasks[] | select(.type == "implementation") | .depends_on[]' "$tasks_file")
    echo "$impl_deps" | grep -q "setup"
    assert_success $? "Implementation should depend on setup" || return 1

    # Test should depend on implementation
    local test_deps=$(jq -r '.tasks[] | select(.type == "test") | .depends_on[]' "$tasks_file")
    echo "$test_deps" | grep -qE "implement_"
    assert_success $? "Test should depend on implementation" || return 1
}

# Test: Task IDs sanitized
test_task_ids_sanitized() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web"},
        "architecture":{"components":[{"name":"UI Component With Spaces"}]}
    }'

    local context_file=$(get_context_file "$session_id")
    "$TOOLS_DIR/generate_tasks" "$context_file"

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")
    local impl_id=$(jq -r '.tasks[] | select(.type == "implementation") | .id' "$tasks_file")

    # Should be lowercase with underscores, no spaces
    echo "$impl_id" | grep -qE '^[a-z0-9_]+$'
    assert_success $? "Task IDs should be sanitized" || return 1
}

# Test: All tasks start with status="pending"
test_initial_status() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web"},
        "architecture":{"components":[{"name":"UI"}]}
    }'

    local context_file=$(get_context_file "$session_id")
    "$TOOLS_DIR/generate_tasks" "$context_file"

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")
    local non_pending=$(jq '[.tasks[] | select(.status != "pending")] | length' "$tasks_file")

    assert_equals "0" "$non_pending" "All tasks should start as pending" || return 1
}

# Test: Updates context.json with file path
test_updates_context() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web"},
        "architecture":{"components":[{"name":"UI"}]}
    }'

    local context_file=$(get_context_file "$session_id")
    "$TOOLS_DIR/generate_tasks" "$context_file"

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")
    assert_not_empty "$tasks_file" "Should update context with tasks file path" || return 1

    [[ "$tasks_file" != "null" ]] || { echo "Tasks file path should not be null"; return 1; }
}

# Test: Task count correct
test_task_count() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web"},
        "architecture":{"components":[{"name":"A"},{"name":"B"}]}
    }'

    local context_file=$(get_context_file "$session_id")
    "$TOOLS_DIR/generate_tasks" "$context_file"

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")

    # 2 components: setup + 2*impl + 2*test + integration + docs = 7 tasks
    local count=$(jq '.tasks | length' "$tasks_file")
    assert_equals "7" "$count" "Should have correct task count" || return 1
}

# Test: JSON structure valid
test_json_structure() {
    local session_id=$(create_session "test")
    create_initial_context "$session_id" "test"
    update_context "$session_id" '{
        "requirements":{"platform":"web"},
        "architecture":{"components":[{"name":"UI"}]}
    }'

    local context_file=$(get_context_file "$session_id")
    "$TOOLS_DIR/generate_tasks" "$context_file"

    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")

    # Check required fields
    local has_project=$(jq 'has("project_name")' "$tasks_file")
    local has_tasks=$(jq 'has("tasks")' "$tasks_file")

    assert_equals "true" "$has_project" "Should have project_name" || return 1
    assert_equals "true" "$has_tasks" "Should have tasks array" || return 1
}

# Main test execution
main() {
    echo "Running generate_tasks tests..."
    echo ""

    run_test test_generates_valid_json "Generates valid JSON"
    run_test test_creates_setup_task "Creates setup task"
    run_test test_creates_implementation_tasks "Creates implementation tasks"
    run_test test_creates_test_tasks "Creates test tasks"
    run_test test_creates_integration_multi "Creates integration (multi-component)"
    run_test test_no_integration_single "No integration (single-component)"
    run_test test_creates_documentation "Creates documentation task"
    run_test test_dependency_graph "Dependency graph correct"
    run_test test_task_ids_sanitized "Task IDs sanitized"
    run_test test_initial_status "Initial status pending"
    run_test test_updates_context "Updates context.json"
    run_test test_task_count "Task count correct"
    run_test test_json_structure "JSON structure valid"

    test_summary
}

main
