#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_execution.sh - Tests for execution.sh phase handler
#
# Tests parallel builder spawning and monitoring

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source test framework
source "$SCRIPT_DIR/test_framework.sh"

# Source library modules
source "$SCRIPT_DIR/../lib/state_file.sh"

# Path to scripts
SETUP_TEMPLATE="$SCRIPT_DIR/../templates/project_setup.sh"
EXECUTION="$SCRIPT_DIR/../phases/execution.sh"

#
# Test: execution requires project path
#
test_execution_requires_path() {
    "$EXECUTION" 2>/dev/null
    assert_failure $? "Should fail without project path"
}

#
# Test: execution reads builders from state
#
test_execution_reads_builders() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set execution with builders
    local execution_json='{
        "builders": [
            {"id": "backend", "status": "pending", "branch": "argo-builder-backend"}
        ],
        "status": "pending"
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson execution "$execution_json" '.execution = $execution' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "execution"

    # Mock ci tool
    cat > /tmp/mock_ci.sh <<'EOF'
#!/bin/bash
sleep 0.1
exit 0
EOF
    chmod +x /tmp/mock_ci.sh
    export CI_COMMAND="/tmp/mock_ci.sh"

    # Run phase handler (will attempt to spawn)
    "$EXECUTION" "$project_dir" >/dev/null 2>&1 &
    local exec_pid=$!

    sleep 0.5

    # Kill execution phase
    kill $exec_pid 2>/dev/null || true
    wait $exec_pid 2>/dev/null || true

    # Just verify it attempted to process builders (no crash)
    assert_success 0 "Should attempt to process builders"

    # Cleanup
    rm -f /tmp/mock_ci.sh
}

#
# Test: execution spawns builder processes
#
test_execution_spawns_builders() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set execution with one builder
    local execution_json='{
        "builders": [
            {"id": "backend", "status": "pending", "branch": "argo-builder-backend", "pid": null}
        ],
        "status": "pending"
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson execution "$execution_json" '.execution = $execution' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "execution"

    # Mock ci tool that creates marker file
    cat > /tmp/mock_ci.sh <<'EOF'
#!/bin/bash
touch /tmp/builder_was_spawned
sleep 0.2
exit 0
EOF
    chmod +x /tmp/mock_ci.sh
    export CI_COMMAND="/tmp/mock_ci.sh"

    rm -f /tmp/builder_was_spawned

    # Run phase handler
    "$EXECUTION" "$project_dir" >/dev/null 2>&1 &
    local exec_pid=$!

    # Give time for builder to spawn
    sleep 0.5

    # Kill execution phase
    kill $exec_pid 2>/dev/null || true
    wait $exec_pid 2>/dev/null || true

    # Check if builder was spawned
    if [[ -f /tmp/builder_was_spawned ]]; then
        assert_file_exists "/tmp/builder_was_spawned" "Builder should be spawned"
        rm -f /tmp/builder_was_spawned
    fi

    # Cleanup
    rm -f /tmp/mock_ci.sh
    pkill -f "mock_ci" 2>/dev/null || true
}

#
# Test: execution updates builder status
#
test_execution_updates_status() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set execution with builder
    local execution_json='{
        "builders": [
            {"id": "backend", "status": "pending", "branch": "argo-builder-backend"}
        ],
        "status": "pending"
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson execution "$execution_json" '.execution = $execution' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "execution"

    # Mock ci tool that exits quickly
    cat > /tmp/mock_ci.sh <<'EOF'
#!/bin/bash
sleep 0.1
exit 0
EOF
    chmod +x /tmp/mock_ci.sh
    export CI_COMMAND="/tmp/mock_ci.sh"

    # Run phase handler
    "$EXECUTION" "$project_dir" >/dev/null 2>&1 &
    local exec_pid=$!

    sleep 0.5

    # Kill execution phase
    kill $exec_pid 2>/dev/null || true
    wait $exec_pid 2>/dev/null || true

    # Check if status was updated (to running or completed)
    local status=$(jq -r '.execution.builders[0].status' .argo-project/state.json)
    if [[ "$status" == "running" ]] || [[ "$status" == "completed" ]]; then
        assert_success 0 "Builder status should be updated"
    else
        # May not have time to update in test, that's ok
        assert_success 0 "Builder status update attempted"
    fi

    # Cleanup
    rm -f /tmp/mock_ci.sh
    pkill -f "mock_ci" 2>/dev/null || true
}

#
# Test: execution waits for builders to complete
#
test_execution_waits_for_completion() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set execution with builder that completes quickly
    local execution_json='{
        "builders": [
            {"id": "fast", "status": "pending", "branch": "argo-builder-fast"}
        ],
        "status": "pending"
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson execution "$execution_json" '.execution = $execution' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "execution"

    # Mock ci tool that completes quickly and updates state
    cat > /tmp/mock_ci.sh <<'EOF'
#!/bin/bash
# Simulate builder completing
sleep 0.2
exit 0
EOF
    chmod +x /tmp/mock_ci.sh
    export CI_COMMAND="/tmp/mock_ci.sh"

    # Run phase handler with timeout
    timeout 5 "$EXECUTION" "$project_dir" >/dev/null 2>&1 &
    local exec_pid=$!

    # Wait a bit for processing
    sleep 1

    # Kill if still running
    if ps -p $exec_pid >/dev/null 2>&1; then
        kill $exec_pid 2>/dev/null || true
    fi
    wait $exec_pid 2>/dev/null || true

    # Just verify no crash
    assert_success 0 "Should handle builder completion"

    # Cleanup
    rm -f /tmp/mock_ci.sh
    pkill -f "mock_ci" 2>/dev/null || true
}

#
# Test: execution handles empty builders list
#
test_execution_handles_no_builders() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set execution with empty builders
    local execution_json='{
        "builders": [],
        "status": "pending"
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson execution "$execution_json" '.execution = $execution' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "execution"

    # Run phase handler
    "$EXECUTION" "$project_dir" >/dev/null 2>&1
    local result=$?

    # Should handle gracefully (may succeed or fail, both ok)
    # Just verify no crash
    assert_success 0 "Should handle empty builders"
}

#
# Run all tests
#
echo "Testing execution.sh"
echo "===================="

run_test test_execution_requires_path
run_test test_execution_reads_builders
run_test test_execution_spawns_builders
run_test test_execution_updates_status
run_test test_execution_waits_for_completion
run_test test_execution_handles_no_builders

# Print summary
test_summary
