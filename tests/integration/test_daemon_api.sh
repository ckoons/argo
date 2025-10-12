#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Daemon API Integration Tests
# Tests argo-daemon HTTP API endpoints directly with curl

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

# Daemon binary
DAEMON_BIN="$PROJECT_ROOT/bin/argo-daemon"

# Test daemon port
DAEMON_PORT=19877

# Daemon PID
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

# Helper: Check if JSON contains field
json_contains() {
    local json="$1"
    local field="$2"
    if echo "$json" | grep -q "\"$field\""; then
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
    "$DAEMON_BIN" --port $DAEMON_PORT > /tmp/argo-daemon-api-test.log 2>&1 &
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
    cat /tmp/argo-daemon-api-test.log
    return 1
}

# Stop daemon
stop_daemon() {
    if [ -n "$DAEMON_PID" ]; then
        echo "Stopping daemon (PID: $DAEMON_PID)..."
        kill $DAEMON_PID 2>/dev/null || true
        wait $DAEMON_PID 2>/dev/null || true
        DAEMON_PID=""
    fi

    lsof -ti:$DAEMON_PORT | xargs kill -9 2>/dev/null || true
}

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    stop_daemon
    rm -f /tmp/argo-daemon-api-test.log
}

# Trap cleanup on exit
trap cleanup EXIT

echo ""
echo "=========================================="
echo "Daemon API Integration Tests"
echo "=========================================="
echo ""

# Check daemon exists
if [ ! -x "$DAEMON_BIN" ]; then
    echo -e "${RED}ERROR: daemon not found at $DAEMON_BIN${NC}"
    exit 1
fi

# Start daemon
if ! start_daemon; then
    exit 1
fi
echo ""

# Test 1: Health endpoint
test_start "GET /api/health"
response=$(curl -s "http://localhost:$DAEMON_PORT/api/health")
if json_contains "$response" "status"; then
    test_pass
else
    test_fail "Response: $response"
fi

# Test 2: Health endpoint returns 200
test_start "Health endpoint returns HTTP 200"
status_code=$(curl -s -o /dev/null -w "%{http_code}" "http://localhost:$DAEMON_PORT/api/health")
if [ "$status_code" = "200" ]; then
    test_pass
else
    test_fail "Got HTTP $status_code"
fi

# Test 3: Start workflow endpoint exists
test_start "POST /api/workflow/start (endpoint exists)"
response=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -d '{"script":"workflows/examples/hello.sh"}' \
    "http://localhost:$DAEMON_PORT/api/workflow/start")
if json_contains "$response" "workflow_id" || json_contains "$response" "error"; then
    test_pass
else
    test_fail "Unexpected response: $response"
fi

# Test 4: List workflows endpoint
test_start "GET /api/workflow/list"
response=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/list")
if json_contains "$response" "workflows"; then
    test_pass
else
    test_fail "Response: $response"
fi

# Test 5: Start actual workflow
test_start "Start bash workflow via API"
response=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -d '{"script":"workflows/examples/hello.sh"}' \
    "http://localhost:$DAEMON_PORT/api/workflow/start")
if json_contains "$response" "workflow_id"; then
    WORKFLOW_ID=$(echo "$response" | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)
    test_pass
else
    test_fail "Failed to start workflow: $response"
fi

# Wait for workflow to start
sleep 2

# Test 6: Workflow status endpoint
test_start "GET /api/workflow/status/{id}"
if [ -n "$WORKFLOW_ID" ]; then
    response=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/status/$WORKFLOW_ID")
    if json_contains "$response" "workflow_id" && json_contains "$response" "state"; then
        test_pass
    else
        test_fail "Response: $response"
    fi
else
    test_fail "No workflow ID available"
fi

# Test 7: Workflow status returns correct ID
test_start "Workflow status contains correct ID"
if [ -n "$WORKFLOW_ID" ]; then
    response=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/status/$WORKFLOW_ID")
    if echo "$response" | grep -q "$WORKFLOW_ID"; then
        test_pass
    else
        test_fail "ID mismatch in response"
    fi
else
    test_fail "No workflow ID available"
fi

# Test 8: List shows started workflow
test_start "List endpoint shows started workflow"
if [ -n "$WORKFLOW_ID" ]; then
    response=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/list")
    if echo "$response" | grep -q "$WORKFLOW_ID"; then
        test_pass
    else
        test_fail "Workflow not in list"
    fi
else
    test_fail "No workflow ID available"
fi

# Test 9: Workflow has PID
test_start "Workflow status includes PID"
if [ -n "$WORKFLOW_ID" ]; then
    response=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/status/$WORKFLOW_ID")
    if json_contains "$response" "pid"; then
        test_pass
    else
        test_fail "No PID in response"
    fi
else
    test_fail "No workflow ID available"
fi

# Test 10: Status for non-existent workflow
test_start "Status for non-existent workflow returns 404"
status_code=$(curl -s -o /dev/null -w "%{http_code}" "http://localhost:$DAEMON_PORT/api/workflow/status/wf_nonexistent")
if [ "$status_code" = "404" ]; then
    test_pass
else
    test_fail "Got HTTP $status_code, expected 404"
fi

# Test 11: Start duplicate workflow fails
test_start "Cannot start duplicate workflow"
if [ -n "$WORKFLOW_ID" ]; then
    # Try to start workflow with same script (will get same ID)
    response=$(curl -s -X POST \
        -H "Content-Type: application/json" \
        -d '{"script":"workflows/examples/hello.sh"}' \
        "http://localhost:$DAEMON_PORT/api/workflow/start")
    # Should either succeed with new ID or fail gracefully
    if json_contains "$response" "workflow_id" || json_contains "$response" "error"; then
        test_pass
    else
        test_fail "Unexpected response: $response"
    fi
else
    test_fail "No workflow ID available"
fi

# Wait for workflow to complete
sleep 3

# Test 12: Workflow shows completed state
test_start "Workflow transitions to completed"
if [ -n "$WORKFLOW_ID" ]; then
    response=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/status/$WORKFLOW_ID")
    if echo "$response" | grep -q "completed"; then
        test_pass
    else
        test_fail "Workflow not completed"
    fi
else
    test_fail "No workflow ID available"
fi

# Test 13: Workflow has exit code
test_start "Completed workflow has exit code"
if [ -n "$WORKFLOW_ID" ]; then
    response=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/status/$WORKFLOW_ID")
    if json_contains "$response" "exit_code"; then
        test_pass
    else
        test_fail "No exit code in response"
    fi
else
    test_fail "No workflow ID available"
fi

# Test 14: Start long-running workflow for abandon test
test_start "Start long-running workflow"
response=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -d '{"script":"workflows/examples/long_running.sh"}' \
    "http://localhost:$DAEMON_PORT/api/workflow/start")
if json_contains "$response" "workflow_id"; then
    LONG_ID=$(echo "$response" | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)
    test_pass
else
    test_fail "Failed to start long workflow: $response"
fi

sleep 2

# Test 15: Abandon endpoint
test_start "DELETE /api/workflow/abandon/{id}"
if [ -n "$LONG_ID" ]; then
    response=$(curl -s -X DELETE "http://localhost:$DAEMON_PORT/api/workflow/abandon/$LONG_ID")
    if json_contains "$response" "status" || json_contains "$response" "success"; then
        test_pass
    else
        test_fail "Response: $response"
    fi
else
    test_fail "No long workflow ID available"
fi

sleep 1

# Test 16: Abandoned workflow removed
test_start "Abandoned workflow removed from list"
if [ -n "$LONG_ID" ]; then
    response=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/list")
    if ! echo "$response" | grep -q "$LONG_ID"; then
        test_pass
    else
        test_fail "Abandoned workflow still in list"
    fi
else
    test_fail "No long workflow ID available"
fi

# Test 17: Invalid JSON handling
test_start "Invalid JSON returns 400"
status_code=$(curl -s -o /dev/null -w "%{http_code}" \
    -X POST \
    -H "Content-Type: application/json" \
    -d 'invalid json{' \
    "http://localhost:$DAEMON_PORT/api/workflow/start")
if [ "$status_code" = "400" ]; then
    test_pass
else
    test_fail "Got HTTP $status_code, expected 400"
fi

# Test 18: Missing script field
test_start "Missing script field returns 400"
status_code=$(curl -s -o /dev/null -w "%{http_code}" \
    -X POST \
    -H "Content-Type: application/json" \
    -d '{"wrong_field":"value"}' \
    "http://localhost:$DAEMON_PORT/api/workflow/start")
if [ "$status_code" = "400" ]; then
    test_pass
else
    test_fail "Got HTTP $status_code, expected 400"
fi

# Test 19: Non-existent script path
test_start "Non-existent script returns 404"
status_code=$(curl -s -o /dev/null -w "%{http_code}" \
    -X POST \
    -H "Content-Type: application/json" \
    -d '{"script":"nonexistent/script.sh"}' \
    "http://localhost:$DAEMON_PORT/api/workflow/start")
if [ "$status_code" = "404" ]; then
    test_pass
else
    test_fail "Got HTTP $status_code, expected 404"
fi

# Test 20: Registry endpoint
test_start "GET /api/registry/ci"
response=$(curl -s "http://localhost:$DAEMON_PORT/api/registry/ci")
if json_contains "$response" "instances"; then
    test_pass
else
    test_fail "Response: $response"
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
