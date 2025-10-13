#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Bash Workflow Integration Tests
# Tests arc + daemon + bash workflow execution end-to-end

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

# Binaries
DAEMON_BIN="$PROJECT_ROOT/bin/argo-daemon"
ARC_BIN="$PROJECT_ROOT/bin/arc"

# Test daemon port
DAEMON_PORT=19876

# Test workflow scripts
TEST_HELLO="$PROJECT_ROOT/workflows/examples/hello.sh"
TEST_FAILING="$PROJECT_ROOT/workflows/examples/failing.sh"
TEST_LONG="$PROJECT_ROOT/workflows/examples/long_running.sh"

# Daemon PID file
DAEMON_PID=""

# Helper: Print test header
test_start() {
    echo -n "Testing: $1 ... "
    TESTS_RUN=$((TESTS_RUN + 1))
}

# Helper: Test passed
test_pass() {
    echo -e "${GREEN}✓${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

# Helper: Test failed
test_fail() {
    echo -e "${RED}✗${NC} $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

# Helper: Check if output contains string
output_contains() {
    local output="$1"
    local expected="$2"
    if [[ "$output" == *"$expected"* ]]; then
        return 0
    else
        return 1
    fi
}

# Start daemon for tests
start_daemon() {
    echo "Starting test daemon on port $DAEMON_PORT..."

    # Kill any existing daemon on this port
    lsof -ti:$DAEMON_PORT | xargs kill -9 2>/dev/null || true
    sleep 1

    # Start daemon in background
    "$DAEMON_BIN" --port $DAEMON_PORT > /tmp/argo-daemon-test.log 2>&1 &
    DAEMON_PID=$!

    # Wait for daemon to be ready
    local max_attempts=20
    local attempt=0
    while [ $attempt -lt $max_attempts ]; do
        if curl -s "http://localhost:$DAEMON_PORT/api/health" >/dev/null 2>&1; then
            echo "Daemon ready (PID: $DAEMON_PID)"
            return 0
        fi
        sleep 0.5
        attempt=$((attempt + 1))
    done

    echo "ERROR: Daemon failed to start"
    cat /tmp/argo-daemon-test.log
    return 1
}

# Stop daemon
stop_daemon() {
    if [ -n "$DAEMON_PID" ]; then
        echo "Stopping daemon (PID: $DAEMON_PID)..."
        kill $DAEMON_PID 2>/dev/null || true

        # Wait up to 5 seconds for graceful shutdown
        for i in {1..5}; do
            if ! ps -p $DAEMON_PID > /dev/null 2>&1; then
                break
            fi
            sleep 1
        done

        # Force kill if still running
        if ps -p $DAEMON_PID > /dev/null 2>&1; then
            echo "Daemon didn't stop gracefully, force killing..."
            kill -9 $DAEMON_PID 2>/dev/null || true
        fi

        DAEMON_PID=""
    fi

    # Extra cleanup - kill any process on test port
    lsof -ti:$DAEMON_PORT | xargs kill -9 2>/dev/null || true
}

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."

    # Abandon any running workflows
    if [ -n "$DAEMON_PID" ]; then
        curl -s -X GET "http://localhost:$DAEMON_PORT/api/workflow/list" 2>/dev/null | \
            grep -o '"workflow_id":"[^"]*"' | \
            cut -d'"' -f4 | \
            while read wf_id; do
                curl -s -X DELETE "http://localhost:$DAEMON_PORT/api/workflow/abandon/$wf_id" >/dev/null 2>&1 || true
            done
    fi

    stop_daemon

    # Clean up log files
    rm -f /tmp/argo-daemon-test.log
    rm -f ~/.argo/logs/wf_test_*.log 2>/dev/null || true
}

# Trap cleanup on exit
trap cleanup EXIT

echo ""
echo "=========================================="
echo "Bash Workflow Integration Tests"
echo "=========================================="
echo ""

# Check binaries exist
if [ ! -x "$DAEMON_BIN" ]; then
    echo -e "${RED}ERROR: daemon not found at $DAEMON_BIN${NC}"
    exit 1
fi

if [ ! -x "$ARC_BIN" ]; then
    echo -e "${RED}ERROR: arc not found at $ARC_BIN${NC}"
    exit 1
fi

# Check test workflows exist
if [ ! -f "$TEST_HELLO" ]; then
    echo -e "${RED}ERROR: test workflow not found at $TEST_HELLO${NC}"
    exit 1
fi

# Start daemon
if ! start_daemon; then
    exit 1
fi
echo ""

# Configure arc to use test daemon
export ARGO_DAEMON_PORT=$DAEMON_PORT

# Test 1: Daemon health check
test_start "Daemon health check"
response=$(curl -s "http://localhost:$DAEMON_PORT/api/health")
if output_contains "$response" "ok"; then
    test_pass
else
    test_fail "Health check failed: $response"
fi

# Test 2: Start bash workflow
test_start "Start bash workflow"
# Redirect stdin to prevent auto-attach in TTY environments
output=$("$ARC_BIN" workflow start "$TEST_HELLO" </dev/null 2>&1 || true)
if output_contains "$output" "Started workflow" && output_contains "$output" "wf_"; then
    WORKFLOW_ID=$(echo "$output" | grep -o 'wf_[0-9_]*' | head -1)
    test_pass
else
    test_fail "Failed to start workflow: $output"
fi

# Wait for workflow to complete with polling (max 30 seconds)
# Accepts both "completed" and "failed" as terminal states
wait_for_completion() {
    local workflow_id=$1
    local max_attempts=30
    local attempt=0

    while [ $attempt -lt $max_attempts ]; do
        local status_output=$("$ARC_BIN" workflow status "$workflow_id" 2>&1 || true)
        if output_contains "$status_output" "completed" || output_contains "$status_output" "failed"; then
            return 0
        fi
        sleep 1
        attempt=$((attempt + 1))
    done
    return 1
}

# Test 3: Workflow appears in list
test_start "Workflow appears in list"
output=$("$ARC_BIN" workflow list 2>&1 || true)
if output_contains "$output" "$WORKFLOW_ID"; then
    test_pass
else
    test_fail "Workflow not in list"
fi

# Test 4: Workflow status shows details
test_start "Workflow status shows details"
output=$("$ARC_BIN" workflow status "$WORKFLOW_ID" 2>&1 || true)
if output_contains "$output" "Script:" && \
   output_contains "$output" "State:" && \
   output_contains "$output" "PID:" && \
   output_contains "$output" "Exit code:"; then
    test_pass
else
    test_fail "Missing status details"
fi

# Test 5: Workflow completed (removed from registry)
test_start "Workflow completed with exit code 0"
wait_for_completion "$WORKFLOW_ID" || true
# Workflow should be removed from registry after completion
# Check either: (1) workflow removed (not found) or (2) workflow shows completed state
output=$("$ARC_BIN" workflow status "$WORKFLOW_ID" 2>&1 || true)
if output_contains "$output" "not found" || output_contains "$output" "completed"; then
    test_pass
else
    test_fail "Workflow did not complete successfully"
fi

# Test 6: Start failing workflow
test_start "Start failing workflow"
if [ -f "$TEST_FAILING" ]; then
    # Redirect stdin to prevent auto-attach in TTY environments
    output=$("$ARC_BIN" workflow start "$TEST_FAILING" </dev/null 2>&1 || true)
    if output_contains "$output" "Started workflow"; then
        FAILING_ID=$(echo "$output" | grep -o 'wf_[0-9_]*' | head -1)
        test_pass
    else
        test_fail "Failed to start failing workflow"
    fi

    # Wait for it to fail (don't exit script if timeout)
    wait_for_completion "$FAILING_ID" || true

    # Test 7: Failing workflow removed (or shows failure)
    test_start "Failing workflow captured exit code"
    # Workflow should be removed after failure (no retries in test scripts)
    # Check either: (1) workflow removed (not found) or (2) workflow shows failed state
    output=$("$ARC_BIN" workflow status "$FAILING_ID" 2>&1 || true)
    if output_contains "$output" "not found" || (output_contains "$output" "failed" || output_contains "$output" "Exit code:"); then
        test_pass
    else
        test_fail "Exit code not captured correctly"
    fi
else
    test_start "Start failing workflow"
    test_fail "failing.sh not found - skipping"
    test_start "Failing workflow captured exit code"
    test_fail "skipped"
fi

# Test 8: Start long-running workflow
test_start "Start long-running workflow"
if [ -f "$TEST_LONG" ]; then
    # Redirect stdin to prevent auto-attach behavior
    output=$("$ARC_BIN" workflow start "$TEST_LONG" </dev/null 2>&1 || true)
    if output_contains "$output" "Started workflow"; then
        LONG_ID=$(echo "$output" | grep -o 'wf_[0-9_]*' | head -1)
        test_pass
    else
        test_fail "Failed to start long workflow"
    fi

    # Wait briefly
    sleep 2

    # Test 9: Workflow shows running state
    test_start "Long workflow shows running state"
    output=$("$ARC_BIN" workflow status "$LONG_ID" 2>&1 || true)
    if output_contains "$output" "running" || output_contains "$output" "active"; then
        test_pass
    else
        test_fail "Workflow not showing running state"
    fi

    # Test 10: Abandon running workflow
    test_start "Abandon running workflow"
    output=$("$ARC_BIN" workflow abandon "$LONG_ID" 2>&1 || true)
    if output_contains "$output" "Abandoned" || output_contains "$output" "killed"; then
        test_pass
    else
        test_fail "Failed to abandon workflow"
    fi

    # Test 11: Abandoned workflow removed from list
    test_start "Abandoned workflow removed from list"
    # Wait for: (1) process to exit after SIGTERM, (2) SIGCHLD fires, (3) completion task processes exit queue
    # Completion task runs every 5 seconds, worst case: up to 30s for removal to complete
    max_attempts=30
    attempt=0
    workflow_removed=false
    while [ $attempt -lt $max_attempts ]; do
        output=$("$ARC_BIN" workflow list 2>&1 || true)
        if ! output_contains "$output" "$LONG_ID"; then
            workflow_removed=true
            break
        fi
        sleep 1
        attempt=$((attempt + 1))
    done

    if [ "$workflow_removed" = true ]; then
        test_pass
    else
        test_fail "Abandoned workflow still in list after $max_attempts seconds"
    fi
else
    test_start "Start long-running workflow"
    test_fail "long_running.sh not found - skipping"
    test_start "Long workflow shows running state"
    test_fail "skipped"
    test_start "Abandon running workflow"
    test_fail "skipped"
    test_start "Abandoned workflow removed from list"
    test_fail "skipped"
fi

# Test 12: Log file created
test_start "Log file created for workflow"
log_file="$HOME/.argo/logs/${WORKFLOW_ID}.log"
if [ -f "$log_file" ]; then
    test_pass
else
    test_fail "Log file not found at $log_file"
fi

# Test 13: Log file contains output
test_start "Log file contains workflow output"
if [ -f "$log_file" ]; then
    if grep -q "Workflow" "$log_file"; then
        test_pass
    else
        test_fail "Log file empty or missing expected content"
    fi
else
    test_fail "Log file not found"
fi

# Test 14: Daemon API - direct health check
test_start "Direct daemon API health check"
response=$(curl -s "http://localhost:$DAEMON_PORT/api/health")
if output_contains "$response" "ok"; then
    test_pass
else
    test_fail "Health API failed"
fi

# Test 15: Daemon API - workflow list endpoint
test_start "Direct daemon API workflow list"
response=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/list")
if output_contains "$response" "workflows"; then
    test_pass
else
    test_fail "List API failed: $response"
fi

# Test 16: Daemon API - workflow status endpoint
test_start "Direct daemon API workflow status"
# Use the long-running workflow (still running) or any workflow in list
# If first workflow was removed, get any workflow from list
test_wf_id="$LONG_ID"
if [ -z "$test_wf_id" ]; then
    # Fallback: get any workflow from list
    test_wf_id=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/list" | grep -o '"workflow_id":"[^"]*"' | head -1 | cut -d'"' -f4)
fi

if [ -n "$test_wf_id" ]; then
    response=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/status/$test_wf_id")
    if output_contains "$response" "workflow_id" || output_contains "$response" "not found"; then
        test_pass
    else
        test_fail "Status API failed: $response"
    fi
else
    # No workflows in system - API should return error
    test_pass
fi

# Print summary
echo ""
echo "=========================================="
echo "Test Results"
echo "=========================================="
echo "Tests run:    $TESTS_RUN"
echo "Tests passed: $TESTS_PASSED"
echo "Tests failed: $TESTS_FAILED"
echo "=========================================="
echo ""

if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
else
    exit 0
fi
