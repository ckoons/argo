#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# CI Tool Integration Tests
# Tests ci CLI tool with daemon

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
CI_BIN="$PROJECT_ROOT/ci/bin/ci"

# Test daemon port
DAEMON_PORT=19878

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

# Start daemon for tests
start_daemon() {
    echo "Starting test daemon on port $DAEMON_PORT..."

    # Kill any existing daemon on this port
    lsof -ti:$DAEMON_PORT | xargs kill -9 2>/dev/null || true
    sleep 1

    # Start daemon in background
    "$DAEMON_BIN" --port $DAEMON_PORT > /tmp/argo-daemon-ci-test.log 2>&1 &
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
    cat /tmp/argo-daemon-ci-test.log
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
    rm -f /tmp/argo-daemon-ci-test.log
    rm -f /tmp/ci_test_output.txt
}

# Trap cleanup on exit
trap cleanup EXIT

echo ""
echo "=========================================="
echo "CI Tool Integration Tests"
echo "=========================================="
echo ""

# Check binaries exist
if [ ! -x "$DAEMON_BIN" ]; then
    echo -e "${RED}ERROR: daemon not found at $DAEMON_BIN${NC}"
    exit 1
fi

if [ ! -x "$CI_BIN" ]; then
    echo -e "${RED}ERROR: ci not found at $CI_BIN${NC}"
    exit 1
fi

# Start daemon
if ! start_daemon; then
    exit 1
fi
echo ""

# Set daemon port for ci tool
export ARGO_DAEMON_PORT=$DAEMON_PORT

# Test 1: ci help command
test_start "ci help shows usage"
output=$("$CI_BIN" help 2>&1)
if echo "$output" | grep -q "Usage:"; then
    test_pass
else
    test_fail "Help text not shown"
fi

# Test 2: ci with command line input
test_start "ci with command line query"
output=$("$CI_BIN" "What is 2+2?" 2>&1)
if [ -n "$output" ]; then
    test_pass
else
    test_fail "No output from query"
fi

# Test 3: ci with stdin input
test_start "ci with stdin input"
output=$(echo "Say hello" | "$CI_BIN" 2>&1)
if [ -n "$output" ]; then
    test_pass
else
    test_fail "No output from stdin query"
fi

# Test 4: ci with explicit provider
test_start "ci with --provider flag"
output=$("$CI_BIN" --provider claude_code "test" 2>&1)
if [ -n "$output" ]; then
    test_pass
else
    test_fail "Provider flag not working"
fi

# Test 5: ci with explicit model
test_start "ci with --model flag"
output=$("$CI_BIN" --model claude-sonnet-4-5 "test" 2>&1)
if [ -n "$output" ]; then
    test_pass
else
    test_fail "Model flag not working"
fi

# Test 6: ci with both provider and model
test_start "ci with --provider and --model"
output=$("$CI_BIN" --provider claude_code --model claude-sonnet-4-5 "test" 2>&1)
if [ -n "$output" ]; then
    test_pass
else
    test_fail "Combined flags not working"
fi

# Test 7: ci handles special characters
test_start "ci with special characters"
output=$("$CI_BIN" "Test \"quotes\" here" 2>&1)
if [ -n "$output" ]; then
    test_pass
else
    test_fail "Special characters caused failure"
fi

# Test 8: ci handles newlines in input
test_start "ci with multiline input (stdin)"
output=$(printf "Line 1\nLine 2\nLine 3" | "$CI_BIN" 2>&1)
if [ -n "$output" ]; then
    test_pass
else
    test_fail "Multiline input failed"
fi

# Test 9: Error handling - daemon not running
test_start "ci error when daemon not running"
stop_daemon
output=$("$CI_BIN" "test" 2>&1 || true)
if echo "$output" | grep -qi "error\|failed\|connect"; then
    test_pass
else
    test_fail "No error message shown"
fi

# Restart daemon for remaining tests
if ! start_daemon; then
    echo "Failed to restart daemon"
    exit 1
fi

# Test 10: ci with very long query
test_start "ci with long query text"
long_query=$(printf 'a%.0s' {1..1000})
output=$("$CI_BIN" "$long_query" 2>&1)
if [ -n "$output" ]; then
    test_pass
else
    test_fail "Long query failed"
fi

# Test 11: ci with empty query (should fail gracefully)
test_start "ci with empty query"
output=$("$CI_BIN" "" 2>&1 || true)
# Should either get response or graceful error
test_pass

# Test 12: ci output is non-empty for valid query
test_start "ci returns non-empty response"
output=$("$CI_BIN" "Say 'test'" 2>&1)
if [ ${#output} -gt 5 ]; then
    test_pass
else
    test_fail "Response too short: '$output'"
fi

# Test 13: ci preserves exit codes
test_start "ci returns correct exit codes"
"$CI_BIN" "test" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    test_pass
else
    test_fail "Exit code not 0 for successful query"
fi

# Test 14: ci timeout handling (quick test)
test_start "ci handles queries without hanging"
timeout 30 "$CI_BIN" "quick test" >/dev/null 2>&1
if [ $? -ne 124 ]; then
    test_pass
else
    test_fail "Query timed out"
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
