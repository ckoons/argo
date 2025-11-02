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

#
# Test: start_workflow requires query argument
#
test_start_requires_query() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Run without query
    "$START_WORKFLOW" 2>/dev/null
    assert_failure $? "Should fail without query"
}

#
# Test: start_workflow requires .argo-project
#
test_start_requires_project() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # No .argo-project directory
    "$START_WORKFLOW" "test query" 2>/dev/null
    assert_failure $? "Should fail without .argo-project"
}

#
# Test: start_workflow saves user query to state
#
test_start_saves_query() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Start workflow (don't actually launch orchestrator for this test)
    # We'll create a mock orchestrator that exits immediately
    cat > "$SCRIPT_DIR/../orchestrator.sh" <<'EOF'
#!/bin/bash
exit 0
EOF
    chmod +x "$SCRIPT_DIR/../orchestrator.sh"

    "$START_WORKFLOW" "Build a calculator" >/dev/null

    # Check state.json
    local query=$(jq -r '.user_query' .argo-project/state.json)
    assert_equals "Build a calculator" "$query" "Query should be saved to state"

    # Cleanup mock
    rm -f "$SCRIPT_DIR/../orchestrator.sh"
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

    # Mock orchestrator
    cat > "$SCRIPT_DIR/../orchestrator.sh" <<'EOF'
#!/bin/bash
exit 0
EOF
    chmod +x "$SCRIPT_DIR/../orchestrator.sh"

    "$START_WORKFLOW" "test" >/dev/null

    # Verify initial state
    local phase=$(jq -r '.phase' .argo-project/state.json)
    assert_equals "init" "$phase" "Phase should be init"

    local action_owner=$(jq -r '.action_owner' .argo-project/state.json)
    assert_equals "code" "$action_owner" "Action owner should be code"

    local action_needed=$(jq -r '.action_needed' .argo-project/state.json)
    assert_equals "design_program" "$action_needed" "Action needed should be design_program"

    # Cleanup
    rm -f "$SCRIPT_DIR/../orchestrator.sh"
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

    # Create mock orchestrator that creates a marker file then exits
    cat > "$SCRIPT_DIR/../orchestrator.sh" <<'EOF'
#!/bin/bash
touch /tmp/argo_orchestrator_was_launched
sleep 0.1
exit 0
EOF
    chmod +x "$SCRIPT_DIR/../orchestrator.sh"

    rm -f /tmp/argo_orchestrator_was_launched

    "$START_WORKFLOW" "test" >/dev/null

    # Give orchestrator time to start
    sleep 0.2

    # Verify orchestrator was launched
    assert_file_exists "/tmp/argo_orchestrator_was_launched" "Orchestrator should have been launched"

    # Cleanup
    rm -f "$SCRIPT_DIR/../orchestrator.sh"
    rm -f /tmp/argo_orchestrator_was_launched
}

#
# Test: start_workflow returns PID
#
test_start_returns_pid() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Mock orchestrator that sleeps
    cat > "$SCRIPT_DIR/../orchestrator.sh" <<'EOF'
#!/bin/bash
sleep 1
EOF
    chmod +x "$SCRIPT_DIR/../orchestrator.sh"

    # Capture output
    local output=$("$START_WORKFLOW" "test" 2>/dev/null)

    # Should contain "PID" or similar (bash regex uses | not \|)
    assert_matches "$output" "PID" "Output should mention PID"

    # Kill any running orchestrators
    pkill -f "orchestrator.sh" 2>/dev/null || true
    sleep 0.1

    # Cleanup
    rm -f "$SCRIPT_DIR/../orchestrator.sh"
}

#
# Test: start_workflow is idempotent (can restart)
#
test_start_idempotent() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Mock orchestrator
    cat > "$SCRIPT_DIR/../orchestrator.sh" <<'EOF'
#!/bin/bash
exit 0
EOF
    chmod +x "$SCRIPT_DIR/../orchestrator.sh"

    # Run twice
    "$START_WORKFLOW" "first run" >/dev/null
    "$START_WORKFLOW" "second run" >/dev/null
    assert_success $? "Should be idempotent"

    # Query should be from second run
    local query=$(jq -r '.user_query' .argo-project/state.json)
    assert_equals "second run" "$query" "Should accept new query"

    # Cleanup
    rm -f "$SCRIPT_DIR/../orchestrator.sh"
}

#
# Test: start_workflow with project path argument
#
test_start_with_project_path() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    cd "$TEST_TMP_DIR"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Mock orchestrator
    cat > "$SCRIPT_DIR/../orchestrator.sh" <<'EOF'
#!/bin/bash
exit 0
EOF
    chmod +x "$SCRIPT_DIR/../orchestrator.sh"

    # Run from different directory with path argument
    cd "$TEST_TMP_DIR"
    "$START_WORKFLOW" "test" "$project_dir" >/dev/null
    assert_success $? "Should work with project path"

    # Verify query saved
    local query=$(jq -r '.user_query' "$project_dir/.argo-project/state.json")
    assert_equals "test" "$query" "Query should be saved"

    # Cleanup
    rm -f "$SCRIPT_DIR/../orchestrator.sh"
}

#
# Test: start_workflow logs workflow start
#
test_start_logs_event() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Mock orchestrator
    cat > "$SCRIPT_DIR/../orchestrator.sh" <<'EOF'
#!/bin/bash
exit 0
EOF
    chmod +x "$SCRIPT_DIR/../orchestrator.sh"

    "$START_WORKFLOW" "test query" >/dev/null

    # Check log file
    if [[ -f .argo-project/logs/workflow.log ]]; then
        local content=$(cat .argo-project/logs/workflow.log)
        # Should contain state transition (represents workflow start)
        assert_matches "$content" "STATE.*init" "Should log workflow initialization"
    fi

    # Cleanup
    rm -f "$SCRIPT_DIR/../orchestrator.sh"
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

# Print summary
test_summary
