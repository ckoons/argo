#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_checkpoint.sh - Tests for checkpoint.sh module
#
# Tests checkpoint save, restore, list, and cleanup operations

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source test framework
source "$SCRIPT_DIR/test_framework.sh"

# Source modules under test
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"
source "$SCRIPT_DIR/../lib/checkpoint.sh"

#
# Test: save_checkpoint creates checkpoint file
#
test_save_checkpoint() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    # Create state file
    echo '{"phase": "design", "counter": 1}' > .argo-project/state.json

    # Save checkpoint
    save_checkpoint "test_checkpoint" "Test checkpoint" >/dev/null
    assert_success $? "save_checkpoint should succeed"

    # Assert checkpoint directory exists
    assert_dir_exists ".argo-project/checkpoints" "Checkpoints directory should exist"

    # Assert checkpoint file created (pattern: {timestamp}_test_checkpoint.json)
    local checkpoint_count=$(ls .argo-project/checkpoints/*_test_checkpoint.json 2>/dev/null | wc -l | tr -d ' ')
    assert_equals "1" "$checkpoint_count" "One checkpoint file should exist"
}

#
# Test: save_checkpoint preserves state content
#
test_save_checkpoint_content() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    # Create state with specific values
    echo '{"phase": "execution", "counter": 42}' > .argo-project/state.json

    # Save checkpoint
    save_checkpoint "content_test" "Test content" >/dev/null

    # Read checkpoint file
    local checkpoint_file=$(ls .argo-project/checkpoints/*_content_test.json)
    local saved_counter=$(jq -r '.counter' "$checkpoint_file")

    assert_equals "42" "$saved_counter" "Checkpoint should preserve state values"
}

#
# Test: restore_checkpoint restores state
#
test_restore_checkpoint() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    # Create initial state
    echo '{"phase": "design", "counter": 10}' > .argo-project/state.json

    # Save checkpoint
    save_checkpoint "restore_test" "Before modification" >/dev/null

    # Modify state
    echo '{"phase": "execution", "counter": 99}' > .argo-project/state.json

    # Restore checkpoint
    restore_checkpoint "restore_test" >/dev/null
    assert_success $? "restore_checkpoint should succeed"

    # Verify state restored
    local counter=$(jq -r '.counter' .argo-project/state.json)
    assert_equals "10" "$counter" "State should be restored to checkpoint value"
}

#
# Test: restore_checkpoint fails for missing checkpoint
#
test_restore_nonexistent() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    echo '{"phase": "init"}' > .argo-project/state.json

    # Try to restore non-existent checkpoint
    restore_checkpoint "nonexistent" 2>/dev/null
    assert_failure $? "restore_checkpoint should fail for missing checkpoint"
}

#
# Test: list_checkpoints shows all checkpoints
#
test_list_checkpoints() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    echo '{"phase": "init"}' > .argo-project/state.json

    # Create multiple checkpoints
    save_checkpoint "checkpoint1" "First checkpoint" >/dev/null
    sleep 1
    save_checkpoint "checkpoint2" "Second checkpoint" >/dev/null

    # List checkpoints
    local output=$(list_checkpoints)

    # Verify both checkpoints listed
    assert_matches "$output" "checkpoint1" "List should contain first checkpoint"
    assert_matches "$output" "checkpoint2" "List should contain second checkpoint"

    # Verify tab-separated format
    local line_count=$(echo "$output" | wc -l | tr -d ' ')
    assert_equals "2" "$line_count" "Should have 2 checkpoint entries"
}

#
# Test: list_checkpoints returns empty for no checkpoints
#
test_list_checkpoints_empty() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    # No checkpoints directory
    local output=$(list_checkpoints)

    # Should be empty
    assert_empty "$output" "List should be empty when no checkpoints exist"
}

#
# Test: delete_checkpoint removes file
#
test_delete_checkpoint() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    echo '{"phase": "init"}' > .argo-project/state.json

    # Create checkpoint
    save_checkpoint "to_delete" "Will be deleted" >/dev/null

    # Delete checkpoint
    delete_checkpoint "to_delete" >/dev/null
    assert_success $? "delete_checkpoint should succeed"

    # Verify file deleted
    local count=$(ls .argo-project/checkpoints/*_to_delete.json 2>/dev/null | wc -l | tr -d ' ')
    assert_equals "0" "$count" "Checkpoint file should be deleted"
}

#
# Test: delete_checkpoint fails for non-existent checkpoint
#
test_delete_nonexistent() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project"
    cd "$project_dir"

    # Try to delete non-existent checkpoint
    delete_checkpoint "nonexistent" 2>/dev/null
    assert_failure $? "delete_checkpoint should fail for missing checkpoint"
}

#
# Test: cleanup_old_checkpoints removes old files
#
test_cleanup_old_checkpoints() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    echo '{"phase": "init"}' > .argo-project/state.json

    # Create checkpoint
    save_checkpoint "old" "Old checkpoint" >/dev/null

    # Get checkpoint file and modify its timestamp to be old
    local checkpoint_file=$(ls .argo-project/checkpoints/*_old.json)
    touch -t 202001010000 "$checkpoint_file"  # Jan 1, 2020

    # Cleanup checkpoints older than 7 days
    local deleted=$(cleanup_old_checkpoints 7)

    # Verify old checkpoint was deleted
    assert_equals "1" "$deleted" "One checkpoint should be deleted"

    # Verify file is gone
    if [[ -f "$checkpoint_file" ]]; then
        test_fail "Old checkpoint file should be deleted"
        return 1
    fi
}

#
# Test: cleanup_old_checkpoints preserves recent files
#
test_cleanup_preserves_recent() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    echo '{"phase": "init"}' > .argo-project/state.json

    # Create checkpoint (recent)
    save_checkpoint "recent" "Recent checkpoint" >/dev/null

    # Cleanup checkpoints older than 7 days
    local deleted=$(cleanup_old_checkpoints 7)

    # Verify nothing deleted
    assert_equals "0" "$deleted" "No recent checkpoints should be deleted"

    # Verify file still exists
    local count=$(ls .argo-project/checkpoints/*_recent.json 2>/dev/null | wc -l | tr -d ' ')
    assert_equals "1" "$count" "Recent checkpoint should still exist"
}

#
# Test: multiple save/restore cycle
#
test_multiple_save_restore() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    # Initial state
    echo '{"counter": 1}' > .argo-project/state.json
    save_checkpoint "state1" "Counter at 1" >/dev/null

    # Update and save
    echo '{"counter": 2}' > .argo-project/state.json
    save_checkpoint "state2" "Counter at 2" >/dev/null

    # Update again
    echo '{"counter": 3}' > .argo-project/state.json

    # Restore to state1
    restore_checkpoint "state1" >/dev/null
    local counter=$(jq -r '.counter' .argo-project/state.json)
    assert_equals "1" "$counter" "Should restore to first checkpoint"

    # Restore to state2
    restore_checkpoint "state2" >/dev/null
    counter=$(jq -r '.counter' .argo-project/state.json)
    assert_equals "2" "$counter" "Should restore to second checkpoint"
}

#
# Test: checkpoint with special characters in description
#
test_checkpoint_special_chars() {
    local project_dir="$TEST_TMP_DIR/project"
    mkdir -p "$project_dir/.argo-project/logs"
    cd "$project_dir"

    echo '{"phase": "init"}' > .argo-project/state.json

    # Save with special characters
    save_checkpoint "test" "Description with \"quotes\" and 'apostrophes'" >/dev/null
    assert_success $? "Should handle special characters in description"
}

#
# Run all tests
#
echo "Testing checkpoint.sh"
echo "====================="

run_test test_save_checkpoint
run_test test_save_checkpoint_content
run_test test_restore_checkpoint
run_test test_restore_nonexistent
run_test test_list_checkpoints
run_test test_list_checkpoints_empty
run_test test_delete_checkpoint
run_test test_delete_nonexistent
run_test test_cleanup_old_checkpoints
run_test test_cleanup_preserves_recent
run_test test_multiple_save_restore
run_test test_checkpoint_special_chars

# Print summary
test_summary
