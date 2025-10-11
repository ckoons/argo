#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Integration tests for workflow execution

set -e

cd "$(dirname "$0")/../.."

FAILED=0
PASSED=0

test_workflow() {
    local name=$1
    local workflow=$2

    echo ""
    echo "=== TEST: $name ==="

    # Start workflow non-interactively (just runs to completion or failure)
    if timeout 10 bin/argo_workflow_executor "$workflow" test 2>&1 | tee /tmp/workflow_test.log; then
        echo "PASS: $name"
        ((PASSED++))
    else
        echo "FAIL: $name"
        echo "Log output:"
        cat /tmp/workflow_test.log
        ((FAILED++))
    fi
}

echo ""
echo "=========================================="
echo "Workflow Integration Tests"
echo "=========================================="

# Test 1: Basic display steps
test_workflow "Basic Display Steps" "test_basic_steps"

# Test 2: Variable flow
test_workflow "Variable Substitution" "test_variable_flow"

# Summary
echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo "=========================================="

if [ $FAILED -gt 0 ]; then
    echo "RESULT: FAILED"
    exit 1
else
    echo "RESULT: SUCCESS"
    exit 0
fi
