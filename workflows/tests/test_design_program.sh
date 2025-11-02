#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_design_program.sh - Tests for design_program.sh phase handler
#
# Tests initial requirements gathering phase

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source test framework
source "$SCRIPT_DIR/test_framework.sh"

# Source library modules
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"

# Path to scripts
SETUP_TEMPLATE="$SCRIPT_DIR/../templates/project_setup.sh"
DESIGN_PROGRAM="$SCRIPT_DIR/../phases/design_program.sh"

#
# Test: design_program requires project path
#
test_design_program_requires_path() {
    # Run without path
    "$DESIGN_PROGRAM" 2>/dev/null
    assert_failure $? "Should fail without project path"
}

#
# Test: design_program reads user_query from state
#
test_design_program_reads_query() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set user query
    update_state "user_query" "Build a calculator"
    update_state "phase" "init"
    update_state "action_owner" "code"

    # Mock ci tool to verify query is passed
    cat > /tmp/mock_ci.sh <<'EOF'
#!/bin/bash
# Echo the query so we can verify it was called
echo "QUERY: $*" > /tmp/ci_was_called
echo '{"design": "test design"}'
exit 0
EOF
    chmod +x /tmp/mock_ci.sh

    # Temporarily add mock to PATH
    export PATH="/tmp:$PATH"
    export CI_COMMAND="/tmp/mock_ci.sh"

    # Run phase handler
    "$DESIGN_PROGRAM" "$project_dir" >/dev/null 2>&1 || true

    # Verify ci was called
    if [[ -f /tmp/ci_was_called ]]; then
        local query_passed=$(cat /tmp/ci_was_called)
        assert_matches "$query_passed" "calculator" "Should pass user query to CI"
    fi

    # Cleanup
    rm -f /tmp/mock_ci.sh /tmp/ci_was_called
}

#
# Test: design_program transitions to design phase
#
test_design_program_transitions_phase() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    update_state "user_query" "Build a calculator"
    update_state "phase" "init"
    update_state "action_owner" "code"

    # Mock ci tool
    cat > /tmp/mock_ci.sh <<'EOF'
#!/bin/bash
echo "Simple calculator design"
exit 0
EOF
    chmod +x /tmp/mock_ci.sh
    export CI_COMMAND="/tmp/mock_ci.sh"

    # Run phase handler
    "$DESIGN_PROGRAM" "$project_dir" >/dev/null 2>&1

    # Check phase transition
    local new_phase=$(read_state "phase")
    assert_equals "design" "$new_phase" "Should transition to design phase"

    # Cleanup
    rm -f /tmp/mock_ci.sh
}

#
# Test: design_program sets action_owner to ci
#
test_design_program_handoff_to_ci() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    update_state "user_query" "Build a calculator"
    update_state "phase" "init"
    update_state "action_owner" "code"

    # Mock ci tool
    cat > /tmp/mock_ci.sh <<'EOF'
#!/bin/bash
echo "Design response"
exit 0
EOF
    chmod +x /tmp/mock_ci.sh
    export CI_COMMAND="/tmp/mock_ci.sh"

    # Run phase handler
    "$DESIGN_PROGRAM" "$project_dir" >/dev/null 2>&1

    # Check action owner handoff
    local action_owner=$(read_state "action_owner")
    assert_equals "ci" "$action_owner" "Should hand off to CI"

    # Cleanup
    rm -f /tmp/mock_ci.sh
}

#
# Test: design_program saves design to state
#
test_design_program_saves_design() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    update_state "user_query" "Build a calculator"
    update_state "phase" "init"

    # Mock ci tool with design response
    cat > /tmp/mock_ci.sh <<'EOF'
#!/bin/bash
echo "Calculator with add/subtract functions"
exit 0
EOF
    chmod +x /tmp/mock_ci.sh
    export CI_COMMAND="/tmp/mock_ci.sh"

    # Run phase handler
    "$DESIGN_PROGRAM" "$project_dir" >/dev/null 2>&1

    # Check if design was saved (design object should exist and not be null)
    local design=$(jq -r '.design' .argo-project/state.json)
    if [[ "$design" == "null" ]]; then
        test_fail "Design should be saved to state"
        rm -f /tmp/mock_ci.sh
        return 1
    fi

    # Cleanup
    rm -f /tmp/mock_ci.sh
}

#
# Test: design_program handles ci tool failure
#
test_design_program_handles_ci_failure() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    update_state "user_query" "Build a calculator"
    update_state "phase" "init"

    # Mock ci tool that fails
    cat > /tmp/mock_ci.sh <<'EOF'
#!/bin/bash
echo "ERROR: CI failed"
exit 1
EOF
    chmod +x /tmp/mock_ci.sh
    export CI_COMMAND="/tmp/mock_ci.sh"

    # Run phase handler (should handle failure gracefully)
    "$DESIGN_PROGRAM" "$project_dir" >/dev/null 2>&1
    local result=$?

    # Should return non-zero but not crash
    # (error handling is acceptable behavior)
    if [[ $result -eq 0 ]]; then
        # If it succeeded, error should be recorded
        local has_error=$(jq -r '.error_recovery.last_error' .argo-project/state.json)
        # Either failed or recorded error
        if [[ -z "$has_error" ]] || [[ "$has_error" == "null" ]]; then
            test_fail "Should handle CI failure"
            rm -f /tmp/mock_ci.sh
            return 1
        fi
    fi

    # Cleanup
    rm -f /tmp/mock_ci.sh
}

#
# Test: design_program logs phase execution
#
test_design_program_logs() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    update_state "user_query" "Build a calculator"
    update_state "phase" "init"

    # Mock ci tool
    cat > /tmp/mock_ci.sh <<'EOF'
#!/bin/bash
echo "Design"
exit 0
EOF
    chmod +x /tmp/mock_ci.sh
    export CI_COMMAND="/tmp/mock_ci.sh"

    # Run phase handler
    "$DESIGN_PROGRAM" "$project_dir" >/dev/null 2>&1

    # Check for log entry
    if [[ -f .argo-project/logs/workflow.log ]]; then
        local log_content=$(cat .argo-project/logs/workflow.log)
        # Should contain phase transition (STATE tag with design phase)
        assert_matches "$log_content" "STATE.*design" "Should log phase activity"
    fi

    # Cleanup
    rm -f /tmp/mock_ci.sh
}

#
# Run all tests
#
echo "Testing design_program.sh"
echo "========================="

run_test test_design_program_requires_path
run_test test_design_program_reads_query
run_test test_design_program_transitions_phase
run_test test_design_program_handoff_to_ci
run_test test_design_program_saves_design
run_test test_design_program_handles_ci_failure
run_test test_design_program_logs

# Print summary
test_summary
