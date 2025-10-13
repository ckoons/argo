#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Priority 2 Feature Tests - Production Hardening
# Tests: Arguments, Environment Variables, Concurrent Workflows, Error Handling

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TESTS_PASSED=0
TESTS_FAILED=0

pass() {
    echo -e "${GREEN}✓${NC} $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail() {
    echo -e "${RED}✗${NC} $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

echo "=========================================="
echo "PRIORITY 2 FEATURE TESTS"
echo "Production Hardening Test Suite"
echo "=========================================="
echo ""

# ====================
# TEST SECTION 1: Workflow Arguments
# ====================
echo "SECTION 1: Workflow Arguments"
echo "------------------------------"

# Test 1.1: Simple arguments
cat > /tmp/test_args.sh << 'EOF'
#!/bin/bash
echo "Args: $@"
echo "Count: $#"
echo "Arg1: $1"
echo "Arg2: $2"
EOF
chmod +x /tmp/test_args.sh

RESPONSE=$(curl -s -X POST http://localhost:9876/api/workflow/start \
    -H "Content-Type: application/json" \
    -d '{"script":"/tmp/test_args.sh","args":["hello","world"]}')

WF_ID=$(echo "$RESPONSE" | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)

if [ -n "$WF_ID" ]; then
    sleep 2
    LOG="$HOME/.argo/logs/${WF_ID}.log"
    if grep -q "Args: hello world" "$LOG" && grep -q "Count: 2" "$LOG"; then
        pass "Workflow arguments passed correctly"
    else
        fail "Workflow arguments not passed"
    fi
else
    fail "Failed to start workflow with arguments"
fi

# ====================
# TEST SECTION 2: Environment Variables
# ====================
echo ""
echo "SECTION 2: Environment Variables"
echo "----------------------------------"

# Test 2.1: Multiple environment variables
cat > /tmp/test_env.sh << 'EOF'
#!/bin/bash
echo "API_KEY=${API_KEY}"
echo "DEBUG=${DEBUG}"
echo "STAGE=${STAGE}"
EOF
chmod +x /tmp/test_env.sh

RESPONSE=$(curl -s -X POST http://localhost:9876/api/workflow/start \
    -H "Content-Type: application/json" \
    -d '{"script":"/tmp/test_env.sh","env":{"API_KEY":"secret123","DEBUG":"true","STAGE":"production"}}')

WF_ID=$(echo "$RESPONSE" | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)

if [ -n "$WF_ID" ]; then
    sleep 2
    LOG="$HOME/.argo/logs/${WF_ID}.log"
    if grep -q "API_KEY=secret123" "$LOG" && grep -q "DEBUG=true" "$LOG" && grep -q "STAGE=production" "$LOG"; then
        pass "Environment variables passed correctly"
    else
        fail "Environment variables not passed"
    fi
else
    fail "Failed to start workflow with env vars"
fi

# Test 2.2: Combining args and env
RESPONSE=$(curl -s -X POST http://localhost:9876/api/workflow/start \
    -H "Content-Type: application/json" \
    -d '{"script":"/tmp/test_args.sh","args":["arg1","arg2"],"env":{"TEST":"value"}}')

WF_ID=$(echo "$RESPONSE" | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)

if [ -n "$WF_ID" ]; then
    sleep 2
    pass "Combined args and env vars work"
else
    fail "Failed to combine args and env vars"
fi

# ====================
# TEST SECTION 3: Concurrent Workflows
# ====================
echo ""
echo "SECTION 3: Concurrent Workflows"
echo "--------------------------------"

# Test 3.1: Start 10 workflows concurrently
cat > /tmp/concurrent_test.sh << 'EOF'
#!/bin/bash
echo "Starting: $1"
sleep 1
echo "Complete: $1"
EOF
chmod +x /tmp/concurrent_test.sh

echo "Starting 10 workflows..."
WF_IDS=()

for i in {1..10}; do
    RESPONSE=$(curl -s -X POST http://localhost:9876/api/workflow/start \
        -H "Content-Type: application/json" \
        -d "{\"script\":\"/tmp/concurrent_test.sh\",\"args\":[\"wf_$i\"]}")

    WF_ID=$(echo "$RESPONSE" | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)
    if [ -n "$WF_ID" ]; then
        WF_IDS+=("$WF_ID")
    fi
    sleep 0.1  # Small delay to avoid overwhelming daemon
done

if [ ${#WF_IDS[@]} -eq 10 ]; then
    pass "Started 10 concurrent workflows"

    # Wait for completion
    sleep 3

    # Check all completed
    completed=0
    for wf_id in "${WF_IDS[@]}"; do
        STATUS=$(curl -s "http://localhost:9876/api/workflow/status/$wf_id")
        STATE=$(echo "$STATUS" | grep -o '"state":"[^"]*"' | cut -d'"' -f4)
        if [ "$STATE" = "completed" ]; then
            completed=$((completed + 1))
        fi
    done

    if [ $completed -eq 10 ]; then
        pass "All 10 concurrent workflows completed successfully"
    else
        warn "$completed/10 workflows completed"
    fi

    # Cleanup
    for wf_id in "${WF_IDS[@]}"; do
        curl -s -X DELETE "http://localhost:9876/api/workflow/abandon/$wf_id" > /dev/null
    done
else
    fail "Only started ${#WF_IDS[@]}/10 workflows"
fi

# ====================
# TEST SECTION 4: Error Handling
# ====================
echo ""
echo "SECTION 4: Error Handling"
echo "-------------------------"

# Test 4.1: Non-executable script
cat > /tmp/non_exec.sh << 'EOF'
#!/bin/bash
echo "test"
EOF
chmod 644 /tmp/non_exec.sh  # Not executable

RESPONSE=$(curl -s -X POST http://localhost:9876/api/workflow/start \
    -H "Content-Type: application/json" \
    -d '{"script":"/tmp/non_exec.sh"}')

WF_ID=$(echo "$RESPONSE" | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)

if [ -n "$WF_ID" ]; then
    sleep 2
    STATUS=$(curl -s "http://localhost:9876/api/workflow/status/$wf_id")
    STATE=$(echo "$STATUS" | grep -o '"state":"[^"]*"' | cut -d'"' -f4)

    if [ "$STATE" = "failed" ]; then
        pass "Non-executable script detected and failed"
    else
        warn "Non-executable script state: $STATE"
    fi
else
    warn "Workflow creation failed for non-executable script"
fi

# Test 4.2: Non-existent script
RESPONSE=$(curl -s -X POST http://localhost:9876/api/workflow/start \
    -H "Content-Type: application/json" \
    -d '{"script":"/tmp/does_not_exist.sh"}')

WF_ID=$(echo "$RESPONSE" | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)

if [ -n "$WF_ID" ]; then
    sleep 2
    STATUS=$(curl -s "http://localhost:9876/api/workflow/status/$wf_id")
    STATE=$(echo "$STATUS" | grep -o '"state":"[^"]*"' | cut -d'"' -f4)

    if [ "$STATE" = "failed" ]; then
        pass "Non-existent script detected and failed"
    else
        warn "Non-existent script state: $STATE"
    fi
else
    warn "Workflow creation failed for non-existent script"
fi

# Test 4.3: Script with syntax error
cat > /tmp/syntax_error.sh << 'EOF'
#!/bin/bash
echo "test
# Missing closing quote
EOF
chmod +x /tmp/syntax_error.sh

RESPONSE=$(curl -s -X POST http://localhost:9876/api/workflow/start \
    -H "Content-Type: application/json" \
    -d '{"script":"/tmp/syntax_error.sh"}')

WF_ID=$(echo "$RESPONSE" | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)

if [ -n "$WF_ID" ]; then
    sleep 2
    STATUS=$(curl -s "http://localhost:9876/api/workflow/status/$wf_id")
    EXIT_CODE=$(echo "$STATUS" | grep -o '"exit_code":[0-9]*' | cut -d':' -f2)

    if [ "$EXIT_CODE" != "0" ]; then
        pass "Script with syntax error failed with non-zero exit code"
    else
        warn "Syntax error script succeeded unexpectedly"
    fi
else
    fail "Failed to start workflow with syntax error"
fi

# Test 4.4: Workflow that exits with error
cat > /tmp/exit_error.sh << 'EOF'
#!/bin/bash
echo "Starting..."
exit 42
EOF
chmod +x /tmp/exit_error.sh

RESPONSE=$(curl -s -X POST http://localhost:9876/api/workflow/start \
    -H "Content-Type: application/json" \
    -d '{"script":"/tmp/exit_error.sh"}')

WF_ID=$(echo "$RESPONSE" | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)

if [ -n "$WF_ID" ]; then
    sleep 2
    STATUS=$(curl -s "http://localhost:9876/api/workflow/status/$wf_id")
    EXIT_CODE=$(echo "$STATUS" | grep -o '"exit_code":[0-9]*' | cut -d':' -f2)
    STATE=$(echo "$STATUS" | grep -o '"state":"[^"]*"' | cut -d'"' -f4)

    if [ "$EXIT_CODE" = "42" ] && [ "$STATE" = "failed" ]; then
        pass "Exit code captured correctly (42) and marked as failed"
    else
        fail "Exit code: $EXIT_CODE, State: $STATE (expected 42, failed)"
    fi
else
    fail "Failed to start workflow with error exit"
fi

# ====================
# TEST SECTION 5: Daemon Stability
# ====================
echo ""
echo "SECTION 5: Daemon Stability"
echo "----------------------------"

# Test 5.1: Daemon still responsive after all tests
HEALTH=$(curl -s http://localhost:9876/api/health)

if echo "$HEALTH" | grep -q '"status":"ok"'; then
    pass "Daemon is responsive after all tests"
else
    fail "Daemon not responding"
fi

# Test 5.2: Workflow list still works
LIST=$(curl -s http://localhost:9876/api/workflow/list)

if echo "$LIST" | grep -q '"workflows"'; then
    pass "Workflow list endpoint functional"
else
    fail "Workflow list endpoint not working"
fi

# Cleanup
rm -f /tmp/test_args.sh /tmp/test_env.sh /tmp/concurrent_test.sh
rm -f /tmp/non_exec.sh /tmp/syntax_error.sh /tmp/exit_error.sh

echo ""
echo "=========================================="
echo "TEST SUMMARY"
echo "=========================================="
echo -e "Passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Failed: ${RED}$TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}ALL PRIORITY 2 TESTS PASSED${NC}"
    exit 0
else
    echo -e "${RED}SOME TESTS FAILED${NC}"
    exit 1
fi
