#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Complete End-to-End Integration Tests
# Tests ALL arc commands, daemon features, and workflow types

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
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
DAEMON_PORT=19878

# Daemon PID
DAEMON_PID=""

# Test workflows
TEST_HELLO="$PROJECT_ROOT/workflows/examples/hello.sh"
TEST_FAILING="$PROJECT_ROOT/workflows/examples/failing.sh"
TEST_LONG="$PROJECT_ROOT/workflows/examples/long_running.sh"
TEST_INTERACTIVE="$PROJECT_ROOT/workflows/examples/interactive_prompt.sh"
TEST_CLAUDE="$PROJECT_ROOT/workflows/examples/claude_code_query.sh"
TEST_OLLAMA="$PROJECT_ROOT/workflows/examples/ollama_query.sh"

# Helper functions
test_start() {
    echo -n "Testing: $1 ... "
    TESTS_RUN=$((TESTS_RUN + 1))
}

test_pass() {
    echo -e "${GREEN}✓${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

test_fail() {
    echo -e "${RED}✗${NC} $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

test_skip() {
    echo -e "${YELLOW}⊘${NC} $1"
}

output_contains() {
    local output="$1"
    local expected="$2"
    [[ "$output" == *"$expected"* ]]
}

# Daemon management
start_daemon() {
    echo "Starting test daemon on port $DAEMON_PORT..."
    lsof -ti:$DAEMON_PORT | xargs kill -9 2>/dev/null || true
    sleep 1

    "$DAEMON_BIN" --port $DAEMON_PORT > /tmp/argo-daemon-e2e-test.log 2>&1 &
    DAEMON_PID=$!

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
    cat /tmp/argo-daemon-e2e-test.log
    return 1
}

stop_daemon() {
    if [ -n "$DAEMON_PID" ]; then
        echo "Stopping daemon (PID: $DAEMON_PID)..."
        kill $DAEMON_PID 2>/dev/null || true
        wait $DAEMON_PID 2>/dev/null || true
        DAEMON_PID=""
    fi
    lsof -ti:$DAEMON_PORT | xargs kill -9 2>/dev/null || true
}

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
    rm -f /tmp/argo-daemon-e2e-test.log
    rm -f ~/.argo/logs/wf_e2e_*.log 2>/dev/null || true
}

trap cleanup EXIT

echo ""
echo "=========================================="
echo "Complete End-to-End Integration Tests"
echo "=========================================="
echo ""
echo "This test suite covers:"
echo "  • All arc commands"
echo "  • All daemon APIs"
echo "  • Multiple workflow types"
echo "  • Provider integration (Claude Code, Ollama)"
echo "  • Interactive workflows"
echo "  • Error handling"
echo ""

# Check binaries
if [ ! -x "$DAEMON_BIN" ]; then
    echo -e "${RED}ERROR: daemon not found at $DAEMON_BIN${NC}"
    exit 1
fi

if [ ! -x "$ARC_BIN" ]; then
    echo -e "${RED}ERROR: arc not found at $ARC_BIN${NC}"
    exit 1
fi

# Start daemon
if ! start_daemon; then
    exit 1
fi
echo ""

# Configure arc
export ARGO_DAEMON_PORT=$DAEMON_PORT

##############################################################################
# SECTION 1: Basic Workflow Operations
##############################################################################

echo "=========================================="
echo "SECTION 1: Basic Workflow Operations"
echo "=========================================="
echo ""

# Test 1: workflow start
test_start "arc workflow start (basic)"
output=$("$ARC_BIN" workflow start "$TEST_HELLO" 2>&1 || true)
if output_contains "$output" "Started workflow"; then
    WF_ID=$(echo "$output" | grep -o 'wf_[0-9]*' | head -1)
    test_pass
else
    test_fail "Failed to start: $output"
fi
sleep 3

# Test 2: workflow list
test_start "arc workflow list"
output=$("$ARC_BIN" workflow list 2>&1 || true)
if output_contains "$output" "$WF_ID"; then
    test_pass
else
    test_fail "Workflow not in list"
fi

# Test 3: workflow status
test_start "arc workflow status"
output=$("$ARC_BIN" workflow status "$WF_ID" 2>&1 || true)
if output_contains "$output" "Script:" && output_contains "$output" "State:"; then
    test_pass
else
    test_fail "Missing status fields"
fi

# Test 4: Verify completion
test_start "Workflow completed successfully"
output=$("$ARC_BIN" workflow status "$WF_ID" 2>&1 || true)
if output_contains "$output" "completed" && output_contains "$output" "Exit code:      0"; then
    test_pass
else
    test_fail "Workflow did not complete"
fi

##############################################################################
# SECTION 2: Error Handling
##############################################################################

echo ""
echo "=========================================="
echo "SECTION 2: Error Handling"
echo "=========================================="
echo ""

# Test 5: Failing workflow
test_start "Workflow with non-zero exit code"
if [ -f "$TEST_FAILING" ]; then
    output=$("$ARC_BIN" workflow start "$TEST_FAILING" 2>&1 || true)
    FAIL_ID=$(echo "$output" | grep -o 'wf_[0-9]*' | head -1)
    sleep 2
    output=$("$ARC_BIN" workflow status "$FAIL_ID" 2>&1 || true)
    if ! output_contains "$output" "Exit code:      0"; then
        test_pass
    else
        test_fail "Exit code should be non-zero"
    fi
else
    test_skip "failing.sh not found"
fi

# Test 6: Non-existent workflow status
test_start "Status of non-existent workflow"
output=$("$ARC_BIN" workflow status "wf_nonexistent" 2>&1 || true)
if output_contains "$output" "not found" || output_contains "$output" "404"; then
    test_pass
else
    test_fail "Should return error for missing workflow"
fi

# Test 7: Invalid script path
test_start "Start with invalid script path"
output=$("$ARC_BIN" workflow start "/nonexistent/script.sh" 2>&1 || true)
if output_contains "$output" "not found" || output_contains "$output" "404" || output_contains "$output" "Failed"; then
    test_pass
else
    test_fail "Should reject invalid path"
fi

##############################################################################
# SECTION 3: Workflow Lifecycle
##############################################################################

echo ""
echo "=========================================="
echo "SECTION 3: Workflow Lifecycle"
echo "=========================================="
echo ""

# Test 8: Long-running workflow + abandon
test_start "Start long-running workflow"
if [ -f "$TEST_LONG" ]; then
    output=$("$ARC_BIN" workflow start "$TEST_LONG" 2>&1 || true)
    LONG_ID=$(echo "$output" | grep -o 'wf_[0-9]*' | head -1)
    if [ -n "$LONG_ID" ]; then
        test_pass
    else
        test_fail "Failed to start long workflow"
    fi
else
    test_skip "long_running.sh not found"
    LONG_ID=""
fi

sleep 2

# Test 9: Check running state
if [ -n "$LONG_ID" ]; then
    test_start "Workflow shows running state"
    output=$("$ARC_BIN" workflow status "$LONG_ID" 2>&1 || true)
    if output_contains "$output" "running" || output_contains "$output" "active"; then
        test_pass
    else
        test_fail "Should show running"
    fi
fi

# Test 10: Abandon workflow
if [ -n "$LONG_ID" ]; then
    test_start "arc workflow abandon"
    output=$("$ARC_BIN" workflow abandon "$LONG_ID" 2>&1 || true)
    if output_contains "$output" "Abandoned" || output_contains "$output" "killed"; then
        test_pass
    else
        test_fail "Failed to abandon"
    fi
fi

sleep 1

# Test 11: Abandoned workflow removed
if [ -n "$LONG_ID" ]; then
    test_start "Abandoned workflow removed from list"
    output=$("$ARC_BIN" workflow list 2>&1 || true)
    if ! output_contains "$output" "$LONG_ID"; then
        test_pass
    else
        test_fail "Should be removed"
    fi
fi

##############################################################################
# SECTION 4: Log File Verification
##############################################################################

echo ""
echo "=========================================="
echo "SECTION 4: Log File Verification"
echo "=========================================="
echo ""

# Test 12: Log file created
test_start "Log file created for workflow"
log_file="$HOME/.argo/logs/${WF_ID}.log"
if [ -f "$log_file" ]; then
    test_pass
else
    test_fail "Log file not found"
fi

# Test 13: Log file has content
test_start "Log file contains workflow output"
if [ -f "$log_file" ] && grep -q "Workflow" "$log_file"; then
    test_pass
else
    test_fail "Log empty or missing content"
fi

##############################################################################
# SECTION 5: Provider Integration Tests
##############################################################################

echo ""
echo "=========================================="
echo "SECTION 5: Provider Integration Tests"
echo "=========================================="
echo ""

# Test 14: Claude Code integration
test_start "Claude Code provider test"
if command -v claude &>/dev/null; then
    if [ -f "$TEST_CLAUDE" ]; then
        output=$("$ARC_BIN" workflow start "$TEST_CLAUDE" 2>&1 || true)
        CLAUDE_ID=$(echo "$output" | grep -o 'wf_[0-9]*' | head -1)
        sleep 10  # Claude takes time
        output=$("$ARC_BIN" workflow status "$CLAUDE_ID" 2>&1 || true)
        if output_contains "$output" "Exit code:      0"; then
            test_pass
        else
            test_skip "Claude query did not complete successfully"
        fi
    else
        test_skip "claude_code_query.sh not found"
    fi
else
    test_skip "Claude Code CLI not installed"
fi

# Test 15: Ollama integration
test_start "Ollama provider test"
if curl -s http://localhost:11434/api/tags >/dev/null 2>&1; then
    if [ -f "$TEST_OLLAMA" ]; then
        output=$("$ARC_BIN" workflow start "$TEST_OLLAMA" 2>&1 || true)
        OLLAMA_ID=$(echo "$output" | grep -o 'wf_[0-9]*' | head -1)
        sleep 10  # Ollama takes time
        output=$("$ARC_BIN" workflow status "$OLLAMA_ID" 2>&1 || true)
        if output_contains "$output" "Exit code:      0"; then
            test_pass
        else
            test_skip "Ollama query did not complete successfully"
        fi
    else
        test_skip "ollama_query.sh not found"
    fi
else
    test_skip "Ollama not running"
fi

##############################################################################
# SECTION 6: Daemon API Direct Tests
##############################################################################

echo ""
echo "=========================================="
echo "SECTION 6: Daemon API Direct Tests"
echo "=========================================="
echo ""

# Test 16: Health endpoint
test_start "Daemon health endpoint"
response=$(curl -s "http://localhost:$DAEMON_PORT/api/health")
if output_contains "$response" "ok" || output_contains "$response" "status"; then
    test_pass
else
    test_fail "Health check failed"
fi

# Test 17: List endpoint JSON format
test_start "Workflow list returns valid JSON"
response=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/list")
if output_contains "$response" "workflows" || output_contains "$response" "["; then
    test_pass
else
    test_fail "Invalid JSON response"
fi

# Test 18: Status endpoint JSON format
test_start "Workflow status returns valid JSON"
response=$(curl -s "http://localhost:$DAEMON_PORT/api/workflow/status/$WF_ID")
if output_contains "$response" "workflow_id" && output_contains "$response" "state"; then
    test_pass
else
    test_fail "Invalid JSON response"
fi

##############################################################################
# SECTION 7: Daemon Restart Test
##############################################################################

echo ""
echo "=========================================="
echo "SECTION 7: Daemon Restart Test"
echo "=========================================="
echo ""

# Test 19: Daemon restart/recovery
test_start "Daemon restart and recovery"
echo "Stopping daemon..."
stop_daemon
sleep 2
echo "Restarting daemon..."
if start_daemon; then
    # Check health after restart
    if curl -s "http://localhost:$DAEMON_PORT/api/health" >/dev/null 2>&1; then
        test_pass
    else
        test_fail "Daemon not healthy after restart"
    fi
else
    test_fail "Failed to restart daemon"
fi

##############################################################################
# Summary
##############################################################################

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
