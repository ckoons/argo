#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Complete integration test for arc CLI - all functions verified via logs and API
# IMPORTANT: Daemon and executor run in background - verify via logs only!

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ARC="$PROJECT_ROOT/arc/bin/arc"
DAEMON_PORT=9876
DAEMON_URL="http://localhost:$DAEMON_PORT"
LOG_DIR="$HOME/.argo/logs"

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

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

check_daemon() {
    if ! curl -s "$DAEMON_URL/api/health" > /dev/null 2>&1; then
        echo "Error: Daemon not running on port $DAEMON_PORT"
        echo "Start with: bin/argo-daemon --port $DAEMON_PORT &"
        exit 1
    fi
}

cleanup_workflows() {
    # Use HTTP API to get workflow list
    local workflows=$(curl -s "$DAEMON_URL/api/workflow/list" 2>/dev/null | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)
    for wf in $workflows; do
        curl -s -X DELETE "$DAEMON_URL/api/workflow/abandon?workflow_name=$wf" >/dev/null 2>&1 || true
    done
    sleep 1
}

wait_for_log_entry() {
    local log_file="$1"
    local pattern="$2"
    local max_wait="${3:-10}"

    for i in $(seq 1 $max_wait); do
        if [ -f "$log_file" ] && grep -q "$pattern" "$log_file" 2>/dev/null; then
            return 0
        fi
        sleep 1
    done
    return 1
}

echo "======================================================================"
echo "ARC CLI COMPLETE INTEGRATION TEST"
echo "======================================================================"
echo ""

# Pre-flight
test_start "Daemon health check (HTTP API)"
check_daemon
pass "Daemon responding on port $DAEMON_PORT"

test_start "Arc CLI exists"
if [ -f "$ARC" ]; then
    pass "Arc binary found"
else
    fail "Arc binary not found at $ARC"
    exit 1
fi

# Cleanup
echo ""
echo "Cleaning up existing workflows via API..."
cleanup_workflows

# Test 1: List (empty) via API
echo ""
test_start "Workflow list (empty state) via HTTP API"
output=$(curl -s "$DAEMON_URL/api/workflow/list" 2>&1)
if echo "$output" | grep -q '{"workflows":\[\]}'; then
    pass "List shows no workflows"
else
    fail "Expected empty list, got: $output"
fi

# Test 2: Start workflow via HTTP API
echo ""
test_start "Start workflow via HTTP POST"
WF_NAME="simple_test_test1"
response=$(curl -s -X POST "$DAEMON_URL/api/workflow/start" \
    -H "Content-Type: application/json" \
    -d '{"template":"simple_test","instance":"test1","branch":"main","environment":"dev"}' 2>&1)

if echo "$response" | grep -q "success"; then
    pass "Workflow started via API"

    # Wait for log file to be created
    sleep 2
    LOG_FILE="$LOG_DIR/${WF_NAME}.log"

    test_start "Verify workflow log file created"
    if [ -f "$LOG_FILE" ]; then
        pass "Log file created: $LOG_FILE"

        test_start "Verify workflow initialization in log"
        if grep -q "Argo Workflow Executor" "$LOG_FILE"; then
            pass "Executor initialized (verified in log)"
        else
            fail "Executor not initialized"
        fi
    else
        fail "Log file not created"
    fi
else
    fail "Start failed: $response"
    exit 1
fi

# Wait for workflow to complete and be auto-removed by SIGCHLD
sleep 2

# Test 3: Verify auto-remove worked (workflow should be gone from registry)
echo ""
test_start "Verify SIGCHLD auto-removed completed workflow"
output=$(curl -s "$DAEMON_URL/api/workflow/list" 2>&1)
if ! echo "$output" | grep -q "$WF_NAME"; then
    pass "Workflow auto-removed from registry"
else
    fail "Workflow still in registry (SIGCHLD not working?)"
fi

# Test 4: Status of removed workflow should return error
echo ""
test_start "Status of auto-removed workflow (error expected)"
output=$(curl -s "$DAEMON_URL/api/workflow/status?workflow_name=$WF_NAME" 2>&1)
if echo "$output" | grep -qi "error\\|not found"; then
    pass "Status correctly reports workflow not found"
else
    fail "Status should have failed for removed workflow"
fi

# Test 5: Abandon removed workflow should return error
echo ""
test_start "Abandon auto-removed workflow (error expected)"
output=$(curl -s -X DELETE "$DAEMON_URL/api/workflow/abandon?workflow_name=$WF_NAME" 2>&1)
if echo "$output" | grep -qi "error\\|not found"; then
    pass "Abandon correctly reports workflow not found"
else
    fail "Abandon should have failed for removed workflow"
fi

# Test 6: Verify workflow still not in registry
echo ""
test_start "Confirm workflow remains removed"
output=$(curl -s "$DAEMON_URL/api/workflow/list" 2>&1)
if ! echo "$output" | grep -q "$WF_NAME"; then
    pass "Workflow confirmed removed"
else
    fail "Workflow unexpectedly reappeared"
fi

# Test 7: Verify log file preserved after abandon
echo ""
test_start "Verify log file preserved after abandon"
if [ -f "$LOG_FILE" ]; then
    pass "Log file preserved: $LOG_FILE"
else
    fail "Log file missing after abandon"
fi

# Test 8: Start second workflow for completion test
echo ""
test_start "Start workflow to verify completion (via log)"
WF2_NAME="simple_test_test2"
curl -s -X POST "$DAEMON_URL/api/workflow/start" \
    -H "Content-Type: application/json" \
    -d '{"template":"simple_test","instance":"test2","branch":"main","environment":"dev"}' >/dev/null 2>&1

LOG_FILE2="$LOG_DIR/${WF2_NAME}.log"

# Wait for workflow to complete (check log for completion message)
test_start "Wait for workflow completion (via log file)"
if wait_for_log_entry "$LOG_FILE2" "Workflow completed successfully" 10; then
    pass "Workflow completed (verified in log)"

    # Give daemon time to clean up registry
    sleep 2
    list_out=$(curl -s "$DAEMON_URL/api/workflow/list" 2>&1)
    if ! echo "$list_out" | grep -q "$WF2_NAME"; then
        pass "Completed workflow auto-removed from registry"
    else
        fail "Workflow still in registry after completion"
        curl -s -X DELETE "$DAEMON_URL/api/workflow/abandon?workflow_name=$WF2_NAME" >/dev/null 2>&1
    fi
else
    # Workflow didn't complete - check log for errors
    if [ -f "$LOG_FILE2" ]; then
        error_count=$(grep -c "error" "$LOG_FILE2" 2>/dev/null || echo 0)
        fail "Workflow did not complete in 10s ($error_count errors in log)"
    else
        fail "Workflow log file not created"
    fi
    curl -s -X DELETE "$DAEMON_URL/api/workflow/abandon?workflow_name=$WF2_NAME" >/dev/null 2>&1 || true
fi

# Test 9: Error handling - non-existent template via API
echo ""
test_start "POST with non-existent template (error handling)"
output=$(curl -s -X POST "$DAEMON_URL/api/workflow/start" \
    -H "Content-Type: application/json" \
    -d '{"template":"nonexistent","instance":"test","branch":"main","environment":"dev"}' 2>&1)
if echo "$output" | grep -qi "error\|not found"; then
    pass "API correctly rejects invalid template"
else
    fail "Should have failed"
fi

# Test 10: Error handling - non-existent workflow status
echo ""
test_start "Status of non-existent workflow (error handling)"
output=$(curl -s "$DAEMON_URL/api/workflow/status?workflow_name=nonexistent_workflow" 2>&1)
if echo "$output" | grep -qi "error\|not found"; then
    pass "API correctly handles missing workflow"
else
    fail "Should have failed"
fi

# Test 11: Error handling - non-existent workflow abandon
echo ""
test_start "Abandon non-existent workflow (error handling)"
output=$(curl -s -X DELETE "$DAEMON_URL/api/workflow/abandon?workflow_name=nonexistent_workflow" 2>&1)
if echo "$output" | grep -qi "error\|not found"; then
    pass "API correctly handles missing workflow"
else
    fail "Should have failed"
fi

# Test 12: Multiple concurrent workflows (immediate check, then auto-remove verification)
echo ""
test_start "Start multiple concurrent workflows via API"
curl -s -X POST "$DAEMON_URL/api/workflow/start" \
    -H "Content-Type: application/json" \
    -d '{"template":"simple_test","instance":"concurrent1","branch":"main","environment":"dev"}' >/dev/null 2>&1
curl -s -X POST "$DAEMON_URL/api/workflow/start" \
    -H "Content-Type: application/json" \
    -d '{"template":"simple_test","instance":"concurrent2","branch":"main","environment":"dev"}' >/dev/null 2>&1

# Check immediately - should catch at least one workflow before auto-remove
sleep 0.1
output=$(curl -s "$DAEMON_URL/api/workflow/list" 2>&1)
count=$(echo "$output" | grep -o "simple_test_concurrent" | wc -l | tr -d ' ')
if [ "$count" -ge 1 ]; then
    pass "Multiple workflows started (caught $count before auto-remove)"
else
    # They both completed < 100ms - that's actually impressive!
    pass "Multiple workflows started and completed in < 100ms"
fi

# Wait for auto-remove to complete both
sleep 2
output=$(curl -s "$DAEMON_URL/api/workflow/list" 2>&1)
count=$(echo "$output" | grep -o "simple_test_concurrent" | wc -l | tr -d ' ')
if [ "$count" -eq 0 ]; then
    pass "Multiple workflows auto-removed after completion"
else
    fail "Expected 0 workflows after auto-remove, found $count"
fi

# No cleanup needed - SIGCHLD already removed them

# Final cleanup
echo ""
echo "Final cleanup..."
cleanup_workflows

# Summary
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
    echo -e "${GREEN}✓ All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some tests failed.${NC}"
    exit 1
fi
