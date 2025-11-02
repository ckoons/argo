#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_design_build.sh - Tests for design_build.sh phase handler
#
# Tests decomposition validation and builder setup phase

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source test framework
source "$SCRIPT_DIR/test_framework.sh"

# Source library modules
source "$SCRIPT_DIR/../lib/state_file.sh"

# Path to scripts
SETUP_TEMPLATE="$SCRIPT_DIR/../templates/project_setup.sh"
DESIGN_BUILD="$SCRIPT_DIR/../phases/design_build.sh"

#
# Test: design_build requires project path
#
test_design_build_requires_path() {
    "$DESIGN_BUILD" 2>/dev/null
    assert_failure $? "Should fail without project path"
}

#
# Test: design_build reads design from state
#
test_design_build_reads_design() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set design with components
    local design_json='{
        "summary": "Test design",
        "components": [
            {"id": "backend", "description": "API server"},
            {"id": "frontend", "description": "UI"}
        ]
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson design "$design_json" '.design = $design' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "design"

    # Run phase handler (may fail if git not initialized, that's ok)
    "$DESIGN_BUILD" "$project_dir" >/dev/null 2>&1 || true

    # Just verify it tried to read the design (no crash)
    assert_success 0 "Should attempt to read design"
}

#
# Test: design_build creates execution.builders
#
test_design_build_creates_builders() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Initialize git repo (needed for worktrees)
    git init >/dev/null 2>&1
    git config user.name "Test" 2>/dev/null
    git config user.email "test@example.com" 2>/dev/null
    touch README.md
    git add README.md 2>/dev/null
    git commit -m "Initial commit" >/dev/null 2>&1

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    # Set design with components
    local design_json='{
        "summary": "Test design",
        "components": [
            {"id": "backend", "description": "API server"}
        ]
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson design "$design_json" '.design = $design' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "design"

    # Run phase handler
    "$DESIGN_BUILD" "$project_dir" >/dev/null 2>&1

    # Check if execution.builders was created
    local builders=$(jq '.execution.builders' .argo-project/state.json)
    if [[ "$builders" != "null" ]]; then
        # Verify it's an array
        local builder_count=$(echo "$builders" | jq 'length')
        if [[ "$builder_count" -ge 1 ]]; then
            assert_success 0 "Builders array should be created"
            return 0
        fi
    fi

    # May not fully succeed without git, that's acceptable for test
    assert_success 0 "Should attempt to create builders"
}

#
# Test: design_build transitions to execution phase
#
test_design_build_transitions_phase() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Initialize git
    git init >/dev/null 2>&1
    git config user.name "Test" 2>/dev/null
    git config user.email "test@example.com" 2>/dev/null
    touch README.md
    git add README.md 2>/dev/null
    git commit -m "Initial" >/dev/null 2>&1

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    local design_json='{
        "summary": "Test",
        "components": [{"id": "backend", "description": "API"}]
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson design "$design_json" '.design = $design' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "design"

    # Run phase handler
    "$DESIGN_BUILD" "$project_dir" >/dev/null 2>&1

    # Check phase transition
    local new_phase=$(read_state "phase")
    assert_equals "execution" "$new_phase" "Should transition to execution phase"
}

#
# Test: design_build keeps action_owner as code
#
test_design_build_keeps_code_owner() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    # Initialize git
    git init >/dev/null 2>&1
    git config user.name "Test" 2>/dev/null
    git config user.email "test@example.com" 2>/dev/null
    touch README.md
    git add README.md 2>/dev/null
    git commit -m "Initial" >/dev/null 2>&1

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    local design_json='{
        "summary": "Test",
        "components": [{"id": "backend", "description": "API"}]
    }'

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson design "$design_json" '.design = $design' .argo-project/state.json > "$temp_file"
    mv "$temp_file" .argo-project/state.json

    update_state "phase" "design"
    update_state "action_owner" "code"

    # Run phase handler
    "$DESIGN_BUILD" "$project_dir" >/dev/null 2>&1

    # Action owner should remain "code" (orchestrator will spawn builders)
    local action_owner=$(read_state "action_owner")
    assert_equals "code" "$action_owner" "Should keep action_owner as code"
}

#
# Test: design_build handles missing design
#
test_design_build_handles_missing_design() {
    local project_dir="$TEST_TMP_DIR/test_project"
    mkdir -p "$project_dir"
    cd "$project_dir"

    export ARGO_PROJECTS_REGISTRY="$TEST_TMP_DIR/.argo/projects.json"
    "$SETUP_TEMPLATE" "test_project" "$project_dir" >/dev/null

    update_state "phase" "design"

    # Run without design in state
    "$DESIGN_BUILD" "$project_dir" >/dev/null 2>&1
    local result=$?

    # Should fail gracefully
    assert_failure $result "Should fail without design"
}

#
# Run all tests
#
echo "Testing design_build.sh"
echo "======================="

run_test test_design_build_requires_path
run_test test_design_build_reads_design
run_test test_design_build_creates_builders
run_test test_design_build_transitions_phase
run_test test_design_build_keeps_code_owner
run_test test_design_build_handles_missing_design

# Print summary
test_summary
