#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Full integration test for arc CLI
# Tests all arc commands with live daemon and executor

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ARC="$PROJECT_ROOT/arc/bin/arc"
DAEMON_PORT=9876
DAEMON_URL="http://localhost:$DAEMON_PORT"

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test functions
pass() {
    echo -e "${GREEN}✓${NC} $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail() {
    echo -e "${RED}✗${NC} $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

test_start() {
    echo -e "${YELLOW}Testing:${NC} $1"
    TESTS_RUN=$((TESTS_RUN + 1))
}

# Check daemon is running
check_daemon() {
    if ! curl -s "$DAEMON_URL/api/health" > /dev/null 2>&1; then
        echo "Error: Daemon not running on port $DAEMON_PORT"
        echo "Start with: bin/argo-daemon --port $DAEMON_PORT"
        exit 1
    fi
}

# Cleanup any existing workflows
cleanup_workflows() {
    local workflows=$("$ARC" workflow list 2>&1 | grep -v "No active" | grep -v "TEMPLATES" | grep -v "SCOPE" | grep -v "^$" | grep -v "^-" | awk '{print $1}' || true)
    for wf in $workflows; do
        "$ARC" workflow abandon "$wf" 2>/dev/null || true
    done
}

echo "======================================================================"
echo "ARC CLI INTEGRATION TEST SUITE"
echo "======================================================================"
echo ""

# Pre-flight checks
test_start "Daemon health check"
check_daemon
pass "Daemon is running on port $DAEMON_PORT"

test_start "Arc CLI binary exists"
if [ -f "$ARC" ]; then
    pass "Arc binary found at $ARC"
else
    fail "Arc binary not found at $ARC"
    exit 1
fi

# Clean slate
echo ""
echo "Preparing test environment..."
cleanup_workflows

# Test 1: Workflow list (empty)
echo ""
test_start "arc workflow list (empty)"
output=$("$ARC" workflow list 2>&1)
if echo "$output" | grep -q "No active workflows"; then
    pass "List shows no active workflows"
else
    fail "Expected 'No active workflows', got: $output"
fi

# Test 2: Workflow start
echo ""
test_start "arc workflow start simple_test"
output=$("$ARC" workflow start simple_test test_instance 2>&1)
if echo "$output" | grep -q "Started workflow"; then
    pass "Workflow started successfully"
    WORKFLOW_NAME="simple_test_test_instance"
else
    fail "Failed to start workflow: $output"
    WORKFLOW_NAME=""
fi

# Test 3: Workflow list (with workflow)
echo ""
test_start "arc workflow list (with active workflow)"
if [ -n "$WORKFLOW_NAME" ]; then
    output=$("$ARC" workflow list 2>&1)
    if echo "$output" | grep -q "$WORKFLOW_NAME"; then
        pass "Workflow appears in list"
    else
        fail "Workflow not found in list: $output"
    fi
fi

# Test 4: Workflow status
echo ""
test_start "arc workflow status $WORKFLOW_NAME"
if [ -n "$WORKFLOW_NAME" ]; then
    output=$("$ARC" workflow status "$WORKFLOW_NAME" 2>&1)
    if echo "$output" | grep -q "Status:"; then
        pass "Status command works"
    else
        fail "Status command failed: $output"
    fi
fi

# Test 5: Wait for workflow to complete
echo ""
test_start "Wait for workflow completion"
if [ -n "$WORKFLOW_NAME" ]; then
    WAIT_COUNT=0
    MAX_WAIT=30
    while [ $WAIT_COUNT -lt $MAX_WAIT ]; do
        output=$("$ARC" workflow list 2>&1)
        if ! echo "$output" | grep -q "$WORKFLOW_NAME"; then
            pass "Workflow completed and removed from registry"
            break
        fi
        sleep 1
        WAIT_COUNT=$((WAIT_COUNT + 1))
    done

    if [ $WAIT_COUNT -ge $MAX_WAIT ]; then
        fail "Workflow did not complete within ${MAX_WAIT}s"
    fi
fi

# Test 6: Start another workflow for abandon test
echo ""
test_start "arc workflow start simple_test (for abandon test)"
output=$("$ARC" workflow start simple_test abandon_test 2>&1)
if echo "$output" | grep -q "Started workflow"; then
    pass "Second workflow started"
    ABANDON_WORKFLOW="simple_test_abandon_test"
else
    fail "Failed to start second workflow: $output"
    ABANDON_WORKFLOW=""
fi

# Test 7: Workflow abandon with confirmation
echo ""
test_start "arc workflow abandon $ABANDON_WORKFLOW (with confirmation)"
if [ -n "$ABANDON_WORKFLOW" ]; then
    # Give workflow time to start
    sleep 2

    # Abandon with 'y' confirmation
    output=$(echo "y" | "$ARC" workflow abandon "$ABANDON_WORKFLOW" 2>&1)
    if echo "$output" | grep -q "Abandoned workflow"; then
        pass "Abandon command works with confirmation"
    else
        fail "Abandon failed: $output"
    fi

    # Verify workflow is gone
    sleep 1
    output=$("$ARC" workflow list 2>&1)
    if ! echo "$output" | grep -q "$ABANDON_WORKFLOW"; then
        pass "Workflow removed from registry"
    else
        fail "Workflow still in registry after abandon"
    fi
fi

# Test 8: Error handling - start non-existent template
echo ""
test_start "arc workflow start nonexistent_template (error handling)"
output=$("$ARC" workflow start nonexistent_template test 2>&1 || true)
if echo "$output" | grep -q "not found\|failed"; then
    pass "Correctly handles non-existent template"
else
    fail "Should have failed with non-existent template"
fi

# Test 9: Error handling - status of non-existent workflow
echo ""
test_start "arc workflow status nonexistent_workflow (error handling)"
output=$("$ARC" workflow status nonexistent_workflow 2>&1 || true)
if echo "$output" | grep -q "not found\|failed"; then
    pass "Correctly handles non-existent workflow"
else
    fail "Should have failed with non-existent workflow"
fi

# Test 10: Error handling - abandon non-existent workflow
echo ""
test_start "arc workflow abandon nonexistent_workflow (error handling)"
output=$(echo "y" | "$ARC" workflow abandon nonexistent_workflow 2>&1 || true)
if echo "$output" | grep -q "not found\|failed"; then
    pass "Correctly handles non-existent workflow"
else
    fail "Should have failed with non-existent workflow"
fi

# Test 11: Abandon cancelled (no confirmation)
echo ""
test_start "arc workflow start simple_test (for cancel test)"
output=$("$ARC" workflow start simple_test cancel_test 2>&1)
if echo "$output" | grep -q "Started workflow"; then
    CANCEL_WORKFLOW="simple_test_cancel_test"
    sleep 2

    test_start "arc workflow abandon with 'n' (cancel)"
    output=$(echo "n" | "$ARC" workflow abandon "$CANCEL_WORKFLOW" 2>&1)
    if echo "$output" | grep -q "cancelled"; then
        pass "Abandon correctly cancelled"

        # Verify workflow still exists
        list_output=$("$ARC" workflow list 2>&1)
        if echo "$list_output" | grep -q "$CANCEL_WORKFLOW"; then
            pass "Workflow still in registry after cancel"

            # Clean up
            echo "y" | "$ARC" workflow abandon "$CANCEL_WORKFLOW" >/dev/null 2>&1 || true
        else
            fail "Workflow disappeared after cancel"
        fi
    else
        fail "Should show 'cancelled' message"
        # Clean up
        echo "y" | "$ARC" workflow abandon "$CANCEL_WORKFLOW" >/dev/null 2>&1 || true
    fi
fi

# Test 12: Multiple concurrent workflows
echo ""
test_start "Start multiple concurrent workflows"
"$ARC" workflow start simple_test concurrent1 2>&1 >/dev/null
"$ARC" workflow start simple_test concurrent2 2>&1 >/dev/null
sleep 2

output=$("$ARC" workflow list 2>&1)
count=$(echo "$output" | grep "simple_test_concurrent" | wc -l)
if [ "$count" -eq 2 ]; then
    pass "Multiple concurrent workflows supported"
else
    fail "Expected 2 concurrent workflows, found $count"
fi

# Clean up concurrent workflows
echo "y" | "$ARC" workflow abandon simple_test_concurrent1 >/dev/null 2>&1 || true
echo "y" | "$ARC" workflow abandon simple_test_concurrent2 >/dev/null 2>&1 || true

# Final cleanup
echo ""
echo "Cleaning up test workflows..."
cleanup_workflows

# Test summary
echo ""
echo "======================================================================"
echo "TEST SUMMARY"
echo "======================================================================"
echo "Tests run:    $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
else
    echo -e "Tests failed: $TESTS_FAILED"
fi
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed.${NC}"
    exit 1
fi
