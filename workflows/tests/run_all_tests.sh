#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# run_all_tests.sh - Run all workflow tests
#
# Executes all test suites and reports results

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Track results
TOTAL_SUITES=0
PASSED_SUITES=0
FAILED_SUITES=0
FAILED_SUITE_NAMES=()

echo ""
echo "========================================"
echo "Argo Workflow Test Runner"
echo "========================================"
echo ""

# Run test suite
run_suite() {
    local suite_file="$1"
    local suite_name=$(basename "$suite_file" .sh)

    TOTAL_SUITES=$((TOTAL_SUITES + 1))

    echo -e "${BLUE}Running test suite: $suite_name${NC}"
    echo "----------------------------------------"

    if "$suite_file"; then
        PASSED_SUITES=$((PASSED_SUITES + 1))
        echo -e "${GREEN}✓ $suite_name PASSED${NC}"
    else
        FAILED_SUITES=$((FAILED_SUITES + 1))
        FAILED_SUITE_NAMES+=("$suite_name")
        echo -e "${RED}✗ $suite_name FAILED${NC}"
    fi

    echo ""
}

# Find and run all test_*.sh files
for test_file in "$SCRIPT_DIR"/test_*.sh; do
    if [[ -f "$test_file" ]] && [[ -x "$test_file" ]]; then
        run_suite "$test_file"
    fi
done

# Print summary
echo "========================================"
echo "Overall Test Summary"
echo "========================================"
echo "Test suites run:    $TOTAL_SUITES"
echo -e "Test suites passed: ${GREEN}$PASSED_SUITES${NC}"
echo -e "Test suites failed: ${RED}$FAILED_SUITES${NC}"
echo ""

if [[ $FAILED_SUITES -gt 0 ]]; then
    echo -e "${RED}Failed test suites:${NC}"
    for suite in "${FAILED_SUITE_NAMES[@]}"; do
        echo -e "  ${RED}✗${NC} $suite"
    done
    echo ""
    exit 1
else
    echo -e "${GREEN}All test suites passed!${NC}"
    exit 0
fi
