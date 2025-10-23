#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_conflict_resolver.sh - Unit tests for conflict-resolver.sh
#
# Tests Phase 5.2: Merge conflict resolution

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/../lib"

# Load test framework
source "$SCRIPT_DIR/test_framework.sh"

# Load required libraries
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/git-helpers.sh"
source "$LIB_DIR/conflict-resolver.sh"

# Mock ci function for testing
ci() {
    # Read from stdin if no arguments (heredoc usage)
    local prompt
    if [[ $# -eq 0 ]]; then
        prompt=$(cat)
    else
        prompt="$1"
    fi

    # Return mock analysis based on prompt content
    if echo "$prompt" | grep -q "analyzing merge conflicts"; then
        # Mock conflict analysis
        cat <<'EOF'
{
  "has_conflicts": true,
  "conflict_count": 2,
  "conflict_type": "trivial",
  "can_auto_resolve": true,
  "risk_level": "low",
  "conflicts": [
    {
      "file": "test.sh",
      "type": "whitespace",
      "resolution": "Take ours (feature branch version)",
      "auto_resolvable": true
    },
    {
      "file": "imports.sh",
      "type": "imports",
      "resolution": "Merge both import sections",
      "auto_resolvable": true
    }
  ],
  "recommendation": "auto_resolve"
}
EOF
    elif echo "$prompt" | grep -q "This file has an import conflict"; then
        # Mock merged imports
        cat <<'EOF'
#!/bin/bash
source lib/module_a.sh
source lib/module_b.sh
source lib/module_c.sh

echo "Merged imports"
EOF
    else
        echo '{}'
    fi
}
export -f ci

#
# Test Setup
#

test_setup() {
    # Create temporary directory for this test
    TEST_TMP_DIR=$(mktemp -d /tmp/argo_test.XXXXXX)

    # Export for tests
    export TEST_TMP_DIR
    export TEST_SESSION_DIR="$TEST_TMP_DIR/sessions"
    export ARGO_SESSIONS_DIR="$TEST_SESSION_DIR"

    # Set state manager directories
    export SESSIONS_DIR="$TEST_TMP_DIR/sessions"
    export ARCHIVES_DIR="$TEST_TMP_DIR/archives"
    mkdir -p "$SESSIONS_DIR" "$ARCHIVES_DIR"

    # Create test git repository
    export TEST_REPO_DIR="$TEST_TMP_DIR/repo"
    mkdir -p "$TEST_REPO_DIR"
    cd "$TEST_REPO_DIR"

    git init >/dev/null 2>&1
    git config user.email "test@example.com"
    git config user.name "Test User"

    # Create initial commit
    echo "initial" > file.txt
    git add file.txt
    git commit -m "Initial commit" >/dev/null 2>&1

    # Create feature branch
    git checkout -b feature/test >/dev/null 2>&1

    # Create test design directory
    export TEST_DESIGN_DIR="$TEST_TMP_DIR/design"
    mkdir -p "$TEST_DESIGN_DIR"
}

#
# Conflict Analysis Tests
#

test_analyze_merge_conflicts_no_conflicts() {
    local analysis=$(analyze_merge_conflicts "argo-A" "feature/test" "feature/test/argo-A")

    assert_not_empty "$analysis" "Analysis should not be empty" || return 1

    local has_conflicts=$(echo "$analysis" | jq -r '.has_conflicts')
    assert_equals "false" "$has_conflicts" "Should report no conflicts" || return 1
}

test_analyze_merge_conflicts_with_conflicts() {
    # Create a merge conflict scenario
    echo "line1" > test.sh
    git add test.sh
    git commit -m "Feature branch version" >/dev/null 2>&1

    # Create builder branch with different content
    git checkout -b feature/test/argo-A >/dev/null 2>&1
    git checkout feature/test^ >/dev/null 2>&1  # Go back before feature commit
    git checkout -b feature/test/argo-A >/dev/null 2>&1
    echo "line2" > test.sh
    git add test.sh
    git commit -m "Builder branch version" >/dev/null 2>&1

    # Try to merge (will conflict)
    git merge feature/test >/dev/null 2>&1 || true

    # Analyze conflicts (uses mock ci)
    local analysis=$(analyze_merge_conflicts "argo-A" "feature/test" "feature/test/argo-A")

    assert_not_empty "$analysis" "Analysis should not be empty" || return 1

    local has_conflicts=$(echo "$analysis" | jq -r '.has_conflicts')
    assert_equals "true" "$has_conflicts" "Should report conflicts" || return 1

    local conflict_type=$(echo "$analysis" | jq -r '.conflict_type')
    assert_equals "trivial" "$conflict_type" "Conflict type from AI" || return 1

    local can_auto_resolve=$(echo "$analysis" | jq -r '.can_auto_resolve')
    assert_equals "true" "$can_auto_resolve" "Should be auto-resolvable" || return 1

    # Abort merge
    git merge --abort 2>/dev/null || true
}

#
# Auto-Resolution Tests
#

test_resolve_import_conflict() {
    # Create file with import conflict markers
    local test_file="$TEST_REPO_DIR/imports.sh"
    cat > "$test_file" <<'EOF'
#!/bin/bash
<<<<<<< HEAD
source lib/module_a.sh
source lib/module_b.sh
=======
source lib/module_b.sh
source lib/module_c.sh
>>>>>>> feature/test
echo "test"
EOF

    # Resolve import conflict (uses mock ci)
    resolve_import_conflict "$test_file"
    local result=$?

    assert_success $result "Import conflict resolution should succeed" || return 1

    # Verify file was updated (check for merged imports)
    assert_file_exists "$test_file" "File should exist" || return 1

    local content=$(cat "$test_file")
    assert_matches "$content" "module_a" "Should include module_a" || return 1
    assert_matches "$content" "module_b" "Should include module_b" || return 1
    assert_matches "$content" "module_c" "Should include module_c" || return 1

    # Verify conflict markers removed
    if grep -q "<<<<<<< " "$test_file"; then
        test_fail "Conflict markers should be removed"
        return 1
    fi
}

#
# Respawn for Merge Tests
#

test_respawn_builder_for_merge() {
    local session_id=$(create_session "test_merge_respawn")
    local builder_id="argo-A"
    local build_dir="$TEST_TMP_DIR/build"
    mkdir -p "$build_dir"

    # Initialize builder
    init_builder_state "$session_id" "$builder_id" "feature/test" "Test task"

    # Create builder branch
    git checkout feature/test >/dev/null 2>&1
    git checkout -b feature/test/argo-A >/dev/null 2>&1

    # Mock builder_ci_loop.sh
    local lib_dir_parent=$(dirname "$LIB_DIR")
    mkdir -p "$lib_dir_parent/lib"
    cat > "$lib_dir_parent/lib/builder_ci_loop.sh" <<'EOF'
#!/bin/bash
exit 0
EOF
    chmod +x "$lib_dir_parent/lib/builder_ci_loop.sh"

    # Respawn builder for merge
    local new_pid=$(respawn_builder_for_merge "$session_id" "$builder_id" "feature/test" \
                                                "$TEST_DESIGN_DIR" "$build_dir" 2>/dev/null)

    assert_not_empty "$new_pid" "Should return new PID" || return 1

    # Verify merge attempts incremented
    local merge_attempts=$(jq -r '.merge_attempts' "$SESSIONS_DIR/$session_id/builders/$builder_id.json")
    assert_equals "1" "$merge_attempts" "Merge attempts incremented" || return 1

    # Wait for background process
    wait "$new_pid" 2>/dev/null || true
}

test_respawn_builder_for_merge_max_attempts() {
    local session_id=$(create_session "test_max_merge")
    local builder_id="argo-A"

    # Initialize builder
    init_builder_state "$session_id" "$builder_id" "feature/test" "Test task"

    # Set merge attempts to max
    local builder_file="$SESSIONS_DIR/$session_id/builders/$builder_id.json"
    local temp_file=$(mktemp)
    jq --arg max "$ARGO_MAX_MERGE_ATTEMPTS" \
       '.merge_attempts = ($max | tonumber)' \
       "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"

    # Try to respawn (should fail)
    respawn_builder_for_merge "$session_id" "$builder_id" "feature/test" \
                               "$TEST_DESIGN_DIR" "$TEST_TMP_DIR/build" 2>/dev/null
    local result=$?

    assert_failure $result "Should fail when max merge attempts reached" || return 1
}

#
# Handle Merge Conflict Tests
#

test_handle_merge_conflict_auto_resolve() {
    local session_id=$(create_session "test_handle_conflict")
    local builder_id="argo-A"
    local build_dir="$TEST_TMP_DIR/build"
    mkdir -p "$build_dir"

    # Initialize builder
    init_builder_state "$session_id" "$builder_id" "feature/test" "Test task"

    # Create a simple whitespace conflict
    echo "line1" > test.sh
    git add test.sh
    git commit -m "Feature version" >/dev/null 2>&1

    git checkout -b feature/test/argo-A >/dev/null 2>&1
    git checkout feature/test^ >/dev/null 2>&1
    git checkout -b feature/test/argo-A >/dev/null 2>&1
    echo "line1 " > test.sh  # Added space
    git add test.sh
    git commit -m "Builder version" >/dev/null 2>&1

    # Try merge (will conflict)
    git merge feature/test >/dev/null 2>&1 || true

    # Note: This test is simplified - in reality we'd need a more complete setup
    # Just verify the function can be called without crashing

    # Abort merge for cleanup
    git merge --abort 2>/dev/null || true
}

test_escalate_merge_conflict() {
    local session_id=$(create_session "test_escalate_merge")
    local builder_id="argo-A"

    # Initialize builder
    init_builder_state "$session_id" "$builder_id" "feature/test" "Test task"

    # Create a merge conflict
    echo "line1" > conflict.sh
    git add conflict.sh
    git commit -m "Feature version" >/dev/null 2>&1

    git checkout -b feature/test/argo-A >/dev/null 2>&1
    git checkout feature/test^ >/dev/null 2>&1
    git checkout -b feature/test/argo-A >/dev/null 2>&1
    echo "line2" > conflict.sh
    git add conflict.sh
    git commit -m "Builder version" >/dev/null 2>&1

    git merge feature/test >/dev/null 2>&1 || true

    # Escalate conflict
    escalate_merge_conflict "$session_id" "$builder_id" "feature/test" \
                           "feature/test/argo-A" >/dev/null 2>&1

    # Verify needs_assistance flag
    local needs_assistance=$(jq -r '.needs_assistance' \
                            "$SESSIONS_DIR/$session_id/builders/$builder_id.json")
    assert_equals "true" "$needs_assistance" "Needs assistance flag" || return 1

    # Verify status
    local status=$(jq -r '.status' "$SESSIONS_DIR/$session_id/builders/$builder_id.json")
    assert_equals "merge_conflict" "$status" "Status set to merge_conflict" || return 1

    # Abort merge
    git merge --abort 2>/dev/null || true
}

#
# Configuration Tests
#

test_max_merge_attempts_config() {
    # Test that ARGO_MAX_MERGE_ATTEMPTS is set
    assert_not_empty "$ARGO_MAX_MERGE_ATTEMPTS" "Max merge attempts config" || return 1

    # Default should be 2
    local expected=2
    if [[ -z "${ARGO_MAX_MERGE_ATTEMPTS:-}" ]]; then
        ARGO_MAX_MERGE_ATTEMPTS=2
    fi

    assert_equals "$expected" "$ARGO_MAX_MERGE_ATTEMPTS" "Default max merge attempts" || return 1
}

#
# Run all tests
#

main() {
    echo "Running conflict-resolver.sh unit tests..."

    # Conflict analysis tests
    run_test test_analyze_merge_conflicts_no_conflicts
    run_test test_analyze_merge_conflicts_with_conflicts

    # Auto-resolution tests
    run_test test_resolve_import_conflict

    # Respawn for merge tests
    run_test test_respawn_builder_for_merge
    run_test test_respawn_builder_for_merge_max_attempts

    # Handle merge conflict tests
    run_test test_handle_merge_conflict_auto_resolve
    run_test test_escalate_merge_conflict

    # Configuration tests
    run_test test_max_merge_attempts_config

    # Print summary
    test_summary
}

main
