#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Setup and run code coverage analysis

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "=========================================="
echo "Code Coverage Analysis Setup"
echo "=========================================="
echo ""

# Check for gcov/lcov
if ! command -v gcov &> /dev/null; then
    echo "ERROR: gcov not found (should come with gcc)"
    exit 1
fi

if ! command -v lcov &> /dev/null; then
    echo "lcov not found, installing..."
    brew install lcov
fi

echo "[1/5] Cleaning previous build..."
make clean > /dev/null 2>&1

echo "[2/5] Compiling with coverage instrumentation..."
make CFLAGS="-Wall -Werror -Wextra -std=c11 -g --coverage" LDFLAGS="--coverage" 2>&1 | tail -5

echo "[3/5] Running test suite..."
make test-quick 2>&1 | grep -E "Tests (run|passed|failed)" || true

echo "[4/5] Generating coverage data..."
# Capture coverage info
lcov --capture --directory . --output-file coverage.info \
     --exclude '/usr/*' --exclude '*/tests/*' --quiet

# Generate HTML report
genhtml coverage.info --output-directory coverage-report --quiet

echo "[5/5] Coverage analysis complete!"
echo ""
echo "=========================================="
echo "Coverage Report"
echo "=========================================="

# Show summary
lcov --summary coverage.info 2>&1 | grep -E "lines\.\.\.\.\.|functions\.\.\."

echo ""
echo "Full HTML report: coverage-report/index.html"
echo "Open with: open coverage-report/index.html"
echo ""

# Find files with low coverage
echo "Files with <50% line coverage:"
lcov --list coverage.info 2>&1 | awk '$2 ~ /%/ && $2 ~ /^[0-4]/ {print "  " $2 " - " $1}' | head -10

echo ""
echo "Coverage data saved to: coverage.info"
echo "=========================================="
