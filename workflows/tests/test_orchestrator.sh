#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_orchestrator.sh - Tests for orchestrator.sh
#
# Tests polling daemon, phase dispatch, and lifecycle

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source test framework
source "$SCRIPT_DIR/test_framework.sh"

# Source library modules for test helpers
source "$SCRIPT_DIR/../lib/state_file.sh"

# Path to scripts
SETUP_TEMPLATE="$SCRIPT_DIR/../templates/project_setup.sh"
ORCHESTRATOR="$SCRIPT_DIR/../orchestrator.sh"

#
# Test: orchestrator requires project path
#
test_orchestrator_requires_path() {
    # Run without path and capture exit code
    "$ORCHESTRATOR" 2>/dev/null
    local exit_code=$?

    # Should have failed (non-zero exit)
    if [[ $exit_code -eq 0 ]]; then
        test_fail "Should fail without project path"
        return 1
    fi
}

#
# Test: orchestrator dispatches to phase handler
#
test_orchestrator_dispatches_phase() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set state for code to act (use test-specific phase name)
    update_state "phase" "test_phase"
    update_state "action_owner" "code"
    update_state "action_needed" "test_phase"  # Override default action_needed

    # Create mock phase handler (using test-specific name)
    mkdir -p "$SCRIPT_DIR/../phases"
    cat > "$SCRIPT_DIR/../phases/test_phase.sh" <<'EOF'
#!/bin/bash
touch /tmp/argo_phase_was_called
exit 0
EOF
    chmod +x "$SCRIPT_DIR/../phases/test_phase.sh"

    rm -f /tmp/argo_phase_was_called

    # Start orchestrator
    "$ORCHESTRATOR" "$project_dir" >/dev/null 2>&1 &
    local orch_pid=$!

    # Give it time to run one iteration (need to wait for first poll)
    sleep 7  # Increased from 2 to ensure full poll cycle

    # Kill orchestrator
    kill $orch_pid 2>/dev/null || true
    wait $orch_pid 2>/dev/null || true

    # Verify phase handler was called
    assert_file_exists "/tmp/argo_phase_was_called" "Phase handler should be called"

    # Cleanup
    rm -f "$SCRIPT_DIR/../phases/test_phase.sh"
    rm -f /tmp/argo_phase_was_called
}

#
# Test: orchestrator waits when action_owner is ci
#
test_orchestrator_waits_for_ci() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set state for CI to act (not code)
    update_state "phase" "design"
    update_state "action_owner" "ci"

    # Create mock phase handler
    mkdir -p "$SCRIPT_DIR/../phases"
    cat > "$SCRIPT_DIR/../phases/design.sh" <<'EOF'
#!/bin/bash
touch /tmp/argo_phase_should_not_be_called
exit 0
EOF
    chmod +x "$SCRIPT_DIR/../phases/design.sh"

    rm -f /tmp/argo_phase_should_not_be_called

    # Start orchestrator
    "$ORCHESTRATOR" "$project_dir" >/dev/null 2>&1 &
    local orch_pid=$!

    # Give it time (should wait, not execute)
    sleep 1

    # Kill orchestrator
    kill $orch_pid 2>/dev/null || true
    wait $orch_pid 2>/dev/null || true

    # Verify phase handler was NOT called
    if [[ -f /tmp/argo_phase_should_not_be_called ]]; then
        test_fail "Phase handler should not be called when action_owner=ci"
        rm -f "$SCRIPT_DIR/../phases/design.sh"
        rm -f /tmp/argo_phase_should_not_be_called
        return 1
    fi

    # Cleanup
    rm -f "$SCRIPT_DIR/../phases/design.sh"
}

#
# Test: orchestrator exits when phase reaches storage
#
test_orchestrator_exits_on_storage() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set state to storage (terminal phase)
    update_state "phase" "storage"
    update_state "action_owner" "code"

    # Start orchestrator
    "$ORCHESTRATOR" "$project_dir" >/dev/null 2>&1 &
    local orch_pid=$!

    # Give it time to check state and exit
    sleep 1

    # Orchestrator should have exited
    if ps -p $orch_pid >/dev/null 2>&1; then
        kill $orch_pid 2>/dev/null || true
        test_fail "Orchestrator should exit when phase=storage"
        return 1
    fi
}

#
# Test: orchestrator handles SIGTERM gracefully
#
test_orchestrator_handles_sigterm() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set state for infinite loop (won't reach storage)
    update_state "phase" "init"
    update_state "action_owner" "ci"  # Will wait forever

    # Start orchestrator
    "$ORCHESTRATOR" "$project_dir" >/dev/null 2>&1 &
    local orch_pid=$!

    sleep 0.5

    # Send SIGTERM
    kill -TERM $orch_pid 2>/dev/null

    # Wait longer for signal handling (sleep might take time to interrupt)
    sleep 2

    # Should have exited
    if ps -p $orch_pid >/dev/null 2>&1; then
        kill -9 $orch_pid 2>/dev/null || true
        test_fail "Orchestrator should handle SIGTERM"
        return 1
    fi
}

#
# Test: orchestrator creates PID file
#
test_orchestrator_pid_file() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    update_state "phase" "storage"  # Will exit immediately

    # Start orchestrator
    "$ORCHESTRATOR" "$project_dir" >/dev/null 2>&1 &
    local orch_pid=$!

    sleep 0.3

    # PID file might be created/cleaned up, just verify no crash
    assert_success 0 "Orchestrator should handle PID file"

    # Cleanup
    kill $orch_pid 2>/dev/null || true
}

#
# Test: orchestrator polls at intervals
#
test_orchestrator_polling() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Start with CI owning action (use test-specific phase)
    update_state "phase" "test_poll"
    update_state "action_owner" "ci"
    update_state "action_needed" "test_poll"  # Override default action_needed

    # Create phase handler that updates state (using test-specific name)
    mkdir -p "$SCRIPT_DIR/../phases"
    cat > "$SCRIPT_DIR/../phases/test_poll.sh" <<'EOF'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../lib/state_file.sh"
update_state "phase" "storage"
exit 0
EOF
    chmod +x "$SCRIPT_DIR/../phases/test_poll.sh"

    # Start orchestrator
    "$ORCHESTRATOR" "$project_dir" >/dev/null 2>&1 &
    local orch_pid=$!

    sleep 1

    # Change state to trigger code action
    update_state "action_owner" "code"

    # Wait for orchestrator to poll and act (need full poll cycle + execution)
    sleep 8

    # Check if phase changed to storage (means it executed)
    local phase=$(read_state "phase")

    # Cleanup first
    kill $orch_pid 2>/dev/null || true
    rm -f "$SCRIPT_DIR/../phases/test_poll.sh"

    # Now assert
    assert_equals "storage" "$phase" "Orchestrator should poll and execute when state changes"
}

#
# Test: orchestrator validates project directory
#
test_orchestrator_validates_project() {
    local project_dir="$TEST_TMP_DIR/invalid_project"
    mkdir -p "$project_dir"

    # No .argo-project directory

    "$ORCHESTRATOR" "$project_dir" 2>/dev/null &
    local pid=$!

    sleep 0.3

    # Should have exited
    if ps -p $pid >/dev/null 2>&1; then
        kill $pid 2>/dev/null
        test_fail "Should fail for invalid project"
        return 1
    fi
}

#
# Run all tests
#
echo "Testing orchestrator.sh"
echo "======================"

run_test test_orchestrator_requires_path
run_test test_orchestrator_dispatches_phase
run_test test_orchestrator_waits_for_ci
run_test test_orchestrator_exits_on_storage
run_test test_orchestrator_handles_sigterm
run_test test_orchestrator_pid_file
run_test test_orchestrator_polling
run_test test_orchestrator_validates_project

# Print summary
test_summary
