#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Programming Guidelines Checker for Argo

REPORT_FILE="${1:-/tmp/argo_programming_guidelines_report.txt}"
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

exec > >(tee "$REPORT_FILE")

echo "=========================================="
echo "ARGO PROGRAMMING GUIDELINES REPORT"
echo "Generated: $TIMESTAMP"
echo "=========================================="
echo ""

ERRORS=0
WARNINGS=0
INFOS=0

# 1. Check for magic numbers in .c files (numeric constants should be in headers)
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "1. MAGIC NUMBERS CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
MAGIC_NUMBERS=$(grep -rn '\b[0-9]\{2,\}\b' src/ --include="*.c" | \
  grep -v "^\s*//" | grep -v "^\s*\*" | grep -v "line " | \
  grep -v "errno" | wc -l | tr -d ' ')
echo "Found $MAGIC_NUMBERS potential magic numbers in .c files"
if [ "$MAGIC_NUMBERS" -gt 50 ]; then
  echo "⚠ WARN: Threshold exceeded (max: 50)"
  echo "Action: Move numeric constants to headers"
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS"
fi
echo ""

# 2. Check for unsafe string functions
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "2. UNSAFE STRING FUNCTIONS CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
UNSAFE_FUNCS=$(grep -rn '\bstrcpy\b\|\bstrcat\b\|\bsprintf\b\|\bgets\b' src/ --include="*.c" | \
  grep -v "strncpy\|strncat\|snprintf" | wc -l | tr -d ' ')
if [ "$UNSAFE_FUNCS" -gt 0 ]; then
  echo "✗ FAIL: Found $UNSAFE_FUNCS unsafe string function calls"
  echo ""
  echo "Details:"
  grep -rn '\bstrcpy\b\|\bstrcat\b\|\bsprintf\b\|\bgets\b' src/ --include="*.c" | \
    grep -v "strncpy\|strncat\|snprintf" | head -10
  echo ""
  echo "Action: Replace with safe alternatives (strncpy, snprintf, strncat)"
  ERRORS=$((ERRORS + 1))
else
  echo "✓ PASS: No unsafe string functions found"
fi
echo ""

# 3. Check file sizes (max 600 lines + 3% tolerance = 618)
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "3. FILE SIZE CHECK (max 618 lines)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
LARGE_FILES=0
for file in $(find src/ -name "*.c"); do
  LINES=$(wc -l < "$file" | tr -d ' ')
  if [ "$LINES" -gt 618 ]; then
    echo "⚠ $file: $LINES lines"
    LARGE_FILES=$((LARGE_FILES + 1))
  fi
done
if [ "$LARGE_FILES" -gt 0 ]; then
  echo ""
  echo "⚠ WARN: $LARGE_FILES files exceed 618 line limit"
  echo "Action: Refactor large files into multiple modules"
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS: All files within size limits"
fi
echo ""

# 4. Check error reporting patterns
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "4. ERROR REPORTING CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
STDERR_ERRORS=$(grep -rn 'fprintf(stderr.*[Ee]rror\|fprintf(stderr.*[Ff]ail' src/ --include="*.c" | \
  wc -l | tr -d ' ')
echo "Found $STDERR_ERRORS fprintf(stderr) error messages"
if [ "$STDERR_ERRORS" -gt 15 ]; then
  echo "⚠ WARN: Prefer argo_report_error() for error reporting"
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS: Error reporting usage acceptable"
fi
echo ""

# 5. Check copyright headers
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "5. COPYRIGHT HEADER CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
MISSING_COPYRIGHT=$(find src/ include/ -name "*.c" -o -name "*.h" | \
  xargs grep -L "© 2025 Casey Koons" 2>/dev/null | wc -l | tr -d ' ')
if [ "$MISSING_COPYRIGHT" -gt 0 ]; then
  echo "⚠ WARN: $MISSING_COPYRIGHT files missing copyright header"
  echo ""
  echo "Files missing copyright:"
  find src/ include/ -name "*.c" -o -name "*.h" | \
    xargs grep -L "© 2025 Casey Koons" 2>/dev/null | head -10
  echo ""
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS: All files have copyright headers"
fi
echo ""

# 6. Check TODO comments
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "6. TODO COMMENTS CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
TODOS=$(grep -rn "TODO" src/ include/ --include="*.c" --include="*.h" | wc -l | tr -d ' ')
echo "Found $TODOS TODO comments"
if [ "$TODOS" -gt 30 ]; then
  echo "⚠ WARN: Consider cleaning up TODO comments"
  WARNINGS=$((WARNINGS + 1))
else
  echo "ℹ INFO: TODO count acceptable"
  INFOS=$((INFOS + 1))
fi
echo ""

# 7. Check for common memory issues patterns
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "7. MEMORY MANAGEMENT PATTERNS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
GOTO_CLEANUP=$(grep -rn "goto cleanup" src/ --include="*.c" | wc -l | tr -d ' ')
echo "Functions using goto cleanup pattern: $GOTO_CLEANUP"
echo "ℹ INFO: Manual review recommended - run 'make valgrind' for leak detection"
INFOS=$((INFOS + 1))
echo ""

# 8. Compilation check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "8. COMPILATION CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if make clean > /dev/null 2>&1 && make > /dev/null 2>&1; then
  echo "✓ PASS: Code compiles cleanly with -Werror"
else
  echo "✗ FAIL: Compilation errors detected"
  echo "Action: Fix compilation errors before committing"
  ERRORS=$((ERRORS + 1))
fi
echo ""

# 9. Test suite check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "9. TEST SUITE CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
TEST_OUTPUT=$(make test-quick 2>&1)
if echo "$TEST_OUTPUT" | grep -q "Tests passed:"; then
  PASSED=$(echo "$TEST_OUTPUT" | grep -o "Tests passed: [0-9]*" | awk '{sum+=$3} END {print sum}')
  FAILED=$(echo "$TEST_OUTPUT" | grep -o "Tests failed: [0-9]*" | awk '{sum+=$3} END {print sum}')
  echo "Tests passed: $PASSED"
  echo "Tests failed: $FAILED"
  if [ "$FAILED" -gt 0 ]; then
    echo "✗ FAIL: $FAILED tests failing"
    ERRORS=$((ERRORS + 1))
  else
    echo "✓ PASS: All tests passing"
  fi
else
  echo "⚠ WARN: Could not determine test results"
  WARNINGS=$((WARNINGS + 1))
fi
echo ""

# 10. Line count budget
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "10. CODE SIZE BUDGET"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
TOTAL_LINES=$(find src/ -name "*.c" -exec cat {} \; | wc -l | tr -d ' ')
echo "Total source lines: $TOTAL_LINES"
if [ "$TOTAL_LINES" -lt 10000 ]; then
  echo "✓ EXCELLENT: Well under original 10K budget"
elif [ "$TOTAL_LINES" -lt 15000 ]; then
  echo "✓ GOOD: Manageable complexity"
elif [ "$TOTAL_LINES" -lt 20000 ]; then
  echo "ℹ INFO: Growing - consider refactoring"
  INFOS=$((INFOS + 1))
else
  echo "⚠ WARN: High complexity - refactoring recommended"
  WARNINGS=$((WARNINGS + 1))
fi

# File count
TOTAL_FILES=$(find src/ -name "*.c" | wc -l | tr -d ' ')
echo "Total .c files: $TOTAL_FILES"
echo ""

# 11. Binary size
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "11. BINARY SIZE"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if [ -f "bin/argo-daemon" ]; then
  SIZE_BYTES=$(stat -f%z bin/argo-daemon 2>/dev/null || stat -c%s bin/argo-daemon 2>/dev/null)
  SIZE_KB=$((SIZE_BYTES / 1024))
  echo "argo-daemon: ${SIZE_KB}KB"
  if [ "$SIZE_KB" -lt 200 ]; then
    echo "✓ PASS: Compact binary"
  else
    echo "ℹ INFO: Binary size reasonable"
    INFOS=$((INFOS + 1))
  fi
else
  echo "⚠ WARN: Binary not found (run 'make' first)"
  WARNINGS=$((WARNINGS + 1))
fi
echo ""

# Summary
echo "=========================================="
echo "SUMMARY"
echo "=========================================="
echo "Errors:   $ERRORS ✗"
echo "Warnings: $WARNINGS ⚠"
echo "Info:     $INFOS ℹ"
echo ""

if [ "$ERRORS" -gt 0 ]; then
  echo "Status: ✗ FAIL"
  echo "Action: Fix errors before committing"
  echo ""
  echo "Report saved to: $REPORT_FILE"
  exit 1
elif [ "$WARNINGS" -gt 3 ]; then
  echo "Status: ⚠ WARN"
  echo "Action: Consider addressing warnings"
  echo ""
  echo "Report saved to: $REPORT_FILE"
  exit 0
else
  echo "Status: ✓ PASS"
  echo "Code meets programming guidelines"
  echo ""
  echo "Report saved to: $REPORT_FILE"
  exit 0
fi
