#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# run_phase2_tests.sh - Run all Phase 2 tests with proper cleanup
#
# Ensures orchestrators are cleaned up between test suites

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Cleanup function
cleanup_orchestrators() {
    # Kill any running orchestrators
    pkill -f "orchestrator.sh" 2>/dev/null || true
    # Kill any mock CI processes
    pkill -f "mock_ci" 2>/dev/null || true
    # Give processes time to die
    sleep 0.5
}

# Test files in order
TEST_FILES=(
    "test_start_workflow.sh"
    "test_orchestrator.sh"
    "test_design_program.sh"
    "test_design_build.sh"
    "test_execution.sh"
    "test_merge_build.sh"
)

echo "=== Phase 2 Test Suite ==="
echo ""

# Run each test file
for test_file in "${TEST_FILES[@]}"; do
    echo "Running $test_file..."

    # Cleanup before running test
    cleanup_orchestrators

    # Run test
    "$SCRIPT_DIR/$test_file"
    result=$?

    # Cleanup after test
    cleanup_orchestrators

    # Check result
    if [[ $result -ne 0 ]]; then
        echo ""
        echo "TEST SUITE FAILED: $test_file"
        exit 1
    fi

    echo ""
done

echo "=== Phase 2 Complete ==="
echo "All Phase 2 tests passed!"
exit 0
