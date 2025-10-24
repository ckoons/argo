#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_resolve_task_dependencies.sh - Unit tests for task dependency resolver
#
# Tests topological sort algorithm (Kahn's algorithm) for correct task ordering

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/../lib"
TOOLS_DIR="$SCRIPT_DIR/../tools"

# Load test framework
source "$SCRIPT_DIR/test_framework.sh"

# Test: Linear dependencies (A→B→C)
test_linear_dependencies() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "task_c", "name": "Task C", "depends_on": ["task_b"]},
    {"id": "task_a", "name": "Task A", "depends_on": []},
    {"id": "task_b", "name": "Task B", "depends_on": ["task_a"]}
  ]
}
EOF

    local result=$("$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json")
    local expected="task_a
task_b
task_c"

    assert_equals "$expected" "$result" "Should resolve linear dependencies correctly" || return 1
    assert_success $? "Should exit successfully" || return 1
}

# Test: Parallel independent tasks
test_parallel_tasks() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "task_a", "name": "Task A", "depends_on": []},
    {"id": "task_b", "name": "Task B", "depends_on": []},
    {"id": "task_c", "name": "Task C", "depends_on": []}
  ]
}
EOF

    local result=$("$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json")

    # All tasks should be present (order doesn't matter for independent tasks)
    echo "$result" | grep -q "task_a" || { echo "Missing task_a"; return 1; }
    echo "$result" | grep -q "task_b" || { echo "Missing task_b"; return 1; }
    echo "$result" | grep -q "task_c" || { echo "Missing task_c"; return 1; }

    assert_success $? "Should handle parallel tasks" || return 1
}

# Test: Diamond dependencies (A→B,C→D)
test_diamond_dependencies() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "task_d", "name": "Task D", "depends_on": ["task_b", "task_c"]},
    {"id": "task_b", "name": "Task B", "depends_on": ["task_a"]},
    {"id": "task_c", "name": "Task C", "depends_on": ["task_a"]},
    {"id": "task_a", "name": "Task A", "depends_on": []}
  ]
}
EOF

    local result=$("$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json")

    # task_a must come first
    local a_pos=$(echo "$result" | grep -n "task_a" | cut -d: -f1)
    local b_pos=$(echo "$result" | grep -n "task_b" | cut -d: -f1)
    local c_pos=$(echo "$result" | grep -n "task_c" | cut -d: -f1)
    local d_pos=$(echo "$result" | grep -n "task_d" | cut -d: -f1)

    [[ $a_pos -lt $b_pos ]] || { echo "task_a should come before task_b"; return 1; }
    [[ $a_pos -lt $c_pos ]] || { echo "task_a should come before task_c"; return 1; }
    [[ $b_pos -lt $d_pos ]] || { echo "task_b should come before task_d"; return 1; }
    [[ $c_pos -lt $d_pos ]] || { echo "task_c should come before task_d"; return 1; }

    assert_success $? "Should handle diamond dependencies" || return 1
}

# Test: Circular dependency detection (A→B→C→A)
test_circular_dependency() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "task_a", "name": "Task A", "depends_on": ["task_c"]},
    {"id": "task_b", "name": "Task B", "depends_on": ["task_a"]},
    {"id": "task_c", "name": "Task C", "depends_on": ["task_b"]}
  ]
}
EOF

    "$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json" 2>/dev/null
    local exit_code=$?

    assert_failure $exit_code "Should fail with circular dependency" || return 1
}

# Test: Complex circular (A→B→C, B→D→A)
test_complex_circular() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "task_a", "name": "Task A", "depends_on": []},
    {"id": "task_b", "name": "Task B", "depends_on": ["task_a"]},
    {"id": "task_c", "name": "Task C", "depends_on": ["task_b"]},
    {"id": "task_d", "name": "Task D", "depends_on": ["task_b"]},
    {"id": "task_e", "name": "Task E", "depends_on": ["task_d", "task_a"]}
  ]
}
EOF

    # This is NOT circular (it's a valid DAG)
    local result=$("$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json")
    assert_success $? "Should handle complex but valid dependencies" || return 1
}

# Test: Self-dependency (error)
test_self_dependency() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "task_a", "name": "Task A", "depends_on": ["task_a"]}
  ]
}
EOF

    "$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json" 2>/dev/null
    local exit_code=$?

    assert_failure $exit_code "Should fail with self-dependency" || return 1
}

# Test: Unmet dependency
test_unmet_dependency() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "task_a", "name": "Task A", "depends_on": ["task_missing"]}
  ]
}
EOF

    "$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json" 2>/dev/null
    local exit_code=$?

    assert_failure $exit_code "Should fail with unmet dependency" || return 1
}

# Test: Empty task list
test_empty_task_list() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": []
}
EOF

    "$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json" 2>/dev/null
    local exit_code=$?

    assert_failure $exit_code "Should fail with empty task list" || return 1
}

# Test: Single task
test_single_task() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "task_a", "name": "Task A", "depends_on": []}
  ]
}
EOF

    local result=$("$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json")

    assert_equals "task_a" "$result" "Should handle single task" || return 1
    assert_success $? "Should exit successfully" || return 1
}

# Test: Empty depends_on array
test_empty_depends_on() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "task_a", "name": "Task A", "depends_on": []}
  ]
}
EOF

    local result=$("$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json")

    assert_equals "task_a" "$result" "Should handle empty depends_on" || return 1
}

# Test: All tasks depend on setup
test_all_depend_on_setup() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "setup", "name": "Setup", "depends_on": []},
    {"id": "task_a", "name": "Task A", "depends_on": ["setup"]},
    {"id": "task_b", "name": "Task B", "depends_on": ["setup"]},
    {"id": "task_c", "name": "Task C", "depends_on": ["setup"]}
  ]
}
EOF

    local result=$("$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json")

    # setup must come first
    local setup_pos=$(echo "$result" | grep -n "^setup$" | cut -d: -f1)

    assert_equals "1" "$setup_pos" "Setup should be first" || return 1
    assert_success $? "Should handle common dependency" || return 1
}

# Test: Output format (one ID per line)
test_output_format() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "task_a", "name": "Task A", "depends_on": []},
    {"id": "task_b", "name": "Task B", "depends_on": ["task_a"]}
  ]
}
EOF

    local result=$("$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json")
    local line_count=$(echo "$result" | wc -l | tr -d ' ')

    assert_equals "2" "$line_count" "Should output one task per line" || return 1
}

# Test: Preserves task IDs correctly
test_preserves_task_ids() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "implement_calculator_ui", "name": "UI", "depends_on": []},
    {"id": "test_calculator_ui", "name": "Test UI", "depends_on": ["implement_calculator_ui"]}
  ]
}
EOF

    local result=$("$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json")

    echo "$result" | grep -q "implement_calculator_ui" || { echo "Missing implement_calculator_ui"; return 1; }
    echo "$result" | grep -q "test_calculator_ui" || { echo "Missing test_calculator_ui"; return 1; }

    assert_success $? "Should preserve complex task IDs" || return 1
}

# Test: Large task graph (performance)
test_large_task_graph() {
    # Generate 50 tasks with dependencies
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "setup", "depends_on": []}
EOF

    for i in {1..49}; do
        if [[ $i -eq 49 ]]; then
            echo "    ,{\"id\": \"task_$i\", \"depends_on\": [\"setup\"]}" >> "$TEST_TMP_DIR/tasks.json"
        else
            echo "    ,{\"id\": \"task_$i\", \"depends_on\": [\"setup\"]}" >> "$TEST_TMP_DIR/tasks.json"
        fi
    done

    echo "  ]" >> "$TEST_TMP_DIR/tasks.json"
    echo "}" >> "$TEST_TMP_DIR/tasks.json"

    local start=$(date +%s)
    local result=$("$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json")
    local end=$(date +%s)
    local duration=$((end - start))

    local line_count=$(echo "$result" | wc -l | tr -d ' ')

    assert_equals "50" "$line_count" "Should resolve all 50 tasks" || return 1

    # Should complete in under 2 seconds
    [[ $duration -lt 2 ]] || { echo "Too slow: ${duration}s"; return 1; }

    assert_success $? "Should handle large graphs efficiently" || return 1
}

# Test: Missing file
test_missing_file() {
    "$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/nonexistent.json" 2>/dev/null
    local exit_code=$?

    assert_failure $exit_code "Should fail with missing file" || return 1
}

# Test: Invalid JSON
test_invalid_json() {
    echo "{ invalid json }" > "$TEST_TMP_DIR/tasks.json"

    "$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json" 2>/dev/null
    local exit_code=$?

    assert_failure $exit_code "Should fail with invalid JSON" || return 1
}

# Test: Multiple dependency chains
test_multiple_chains() {
    cat > "$TEST_TMP_DIR/tasks.json" <<'EOF'
{
  "tasks": [
    {"id": "setup", "depends_on": []},
    {"id": "impl_a", "depends_on": ["setup"]},
    {"id": "test_a", "depends_on": ["impl_a"]},
    {"id": "impl_b", "depends_on": ["setup"]},
    {"id": "test_b", "depends_on": ["impl_b"]},
    {"id": "integration", "depends_on": ["test_a", "test_b"]}
  ]
}
EOF

    local result=$("$TOOLS_DIR/resolve_task_dependencies" "$TEST_TMP_DIR/tasks.json")

    # Verify ordering constraints
    local setup_pos=$(echo "$result" | grep -n "^setup$" | cut -d: -f1)
    local integration_pos=$(echo "$result" | grep -n "^integration$" | cut -d: -f1)

    assert_equals "1" "$setup_pos" "Setup should be first" || return 1
    assert_equals "6" "$integration_pos" "Integration should be last" || return 1
    assert_success $? "Should handle multiple chains" || return 1
}

# Main test execution
main() {
    echo "Running resolve_task_dependencies tests..."
    echo ""

    run_test test_linear_dependencies "Linear dependencies (A→B→C)"
    run_test test_parallel_tasks "Parallel independent tasks"
    run_test test_diamond_dependencies "Diamond dependencies (A→B,C→D)"
    run_test test_circular_dependency "Circular dependency detection"
    run_test test_complex_circular "Complex valid dependencies"
    run_test test_self_dependency "Self-dependency detection"
    run_test test_unmet_dependency "Unmet dependency detection"
    run_test test_empty_task_list "Empty task list"
    run_test test_single_task "Single task"
    run_test test_empty_depends_on "Empty depends_on array"
    run_test test_all_depend_on_setup "All tasks depend on setup"
    run_test test_output_format "Output format (one per line)"
    run_test test_preserves_task_ids "Preserves task IDs"
    run_test test_large_task_graph "Large task graph (50 tasks)"
    run_test test_missing_file "Missing file error"
    run_test test_invalid_json "Invalid JSON error"
    run_test test_multiple_chains "Multiple dependency chains"

    test_summary
}

main
