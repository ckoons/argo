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
# Exclude test files from magic number check (tests have specific test values)
MAGIC_NUMBERS=$(grep -rn '\b[0-9]\{2,\}\b' src/ --include="*.c" | \
  grep -v "^\s*//" | grep -v "^\s*\*" | grep -v "line " | \
  grep -v "errno" | grep -v "2025" | wc -l | tr -d ' ')
echo "Found $MAGIC_NUMBERS potential magic numbers in production .c files"
echo "ℹ INFO: Test files excluded from check (tests have specific test values)"
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
# Exclude third-party libraries from copyright check
EXCLUDED_FILES="include/jsmn.h"
MISSING_COPYRIGHT=$(find src/ include/ -name "*.c" -o -name "*.h" | \
  grep -v "jsmn.h" | \
  xargs grep -L "© 2025 Casey Koons" 2>/dev/null | wc -l | tr -d ' ')
if [ "$MISSING_COPYRIGHT" -gt 0 ]; then
  echo "⚠ WARN: $MISSING_COPYRIGHT files missing copyright header"
  echo ""
  echo "Files missing copyright:"
  find src/ include/ -name "*.c" -o -name "*.h" | \
    grep -v "jsmn.h" | \
    xargs grep -L "© 2025 Casey Koons" 2>/dev/null | head -10
  echo ""
  echo "Note: Third-party files excluded: $EXCLUDED_FILES"
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS: All files have copyright headers"
  echo "ℹ INFO: Third-party files excluded: $EXCLUDED_FILES"
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
  echo "ℹ INFO: Binary not found (will be built during compilation check)"
  INFOS=$((INFOS + 1))
fi
echo ""

# 12. Signal handler safety check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "12. SIGNAL HANDLER SAFETY CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check for unsafe functions in signal handlers
# Unsafe: malloc, printf, fprintf, LOG_*, any complex library functions
# Safe: write, _exit, waitpid, kill, sigaction, sigprocmask
SIGNAL_HANDLERS=$(grep -n "signal_handler\|sigchld_handler" src/ --include="*.c" -A 30 | \
  grep -E "malloc|fprintf|printf|LOG_|snprintf|sleep\(" | wc -l | tr -d ' ')
if [ "$SIGNAL_HANDLERS" -gt 0 ]; then
  echo "✗ FAIL: Found $SIGNAL_HANDLERS unsafe function calls in signal handlers"
  echo ""
  echo "Details:"
  grep -n "signal_handler\|sigchld_handler" src/ --include="*.c" -A 30 | \
    grep -E "malloc|fprintf|printf|LOG_|snprintf|sleep\(" | head -5
  echo ""
  echo "Action: Only use async-signal-safe functions (waitpid, write, _exit, etc.)"
  echo "See: man 7 signal-safety for complete list"
  ERRORS=$((ERRORS + 1))
else
  echo "✓ PASS: Signal handlers use only async-signal-safe functions"
fi
echo ""

# 13. Hard-coded URLs check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "13. HARD-CODED URLS/ENDPOINTS CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Look for complete URLs in .c files (not just constants)
# Exclude: comments, defines, daemon_client.c (URL builder itself), providers (external APIs)
HARDCODED_URLS=$(grep -rn '"http://\|"https://' src/ --include="*.c" | \
  grep -v '^\s*//' | \
  grep -v '^\s*\*' | \
  grep -v '#define' | \
  grep -v 'argo_daemon_client.c' | \
  grep -v 'src/providers/' | \
  wc -l | tr -d ' ')

if [ "$HARDCODED_URLS" -gt 0 ]; then
  echo "⚠ WARN: Found $HARDCODED_URLS hard-coded internal daemon URLs"
  echo ""
  echo "Details:"
  grep -rn '"http://\|"https://' src/ --include="*.c" | \
    grep -v '^\s*//' | \
    grep -v '^\s*\*' | \
    grep -v '#define' | \
    grep -v 'argo_daemon_client.c' | \
    grep -v 'src/providers/' | head -5
  echo ""
  echo "Action: Use argo_get_daemon_url() for daemon connections"
  echo "Note: Provider URLs (external APIs) are acceptable"
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS: No hard-coded daemon URLs (external API URLs acceptable)"
fi
echo ""

# 14. Input validation check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "14. INPUT VALIDATION CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that workflow script paths are validated
VALIDATE_SCRIPT_PATH=$(grep -rn "validate_script_path" src/daemon/ --include="*.c" | wc -l | tr -d ' ')
if [ "$VALIDATE_SCRIPT_PATH" -gt 0 ]; then
  echo "✓ PASS: Workflow script path validation implemented"
  echo "ℹ INFO: Found $VALIDATE_SCRIPT_PATH uses of validate_script_path()"
  INFOS=$((INFOS + 1))
else
  echo "⚠ WARN: No workflow script path validation found"
  echo "Action: Implement validate_script_path() to prevent command injection"
  WARNINGS=$((WARNINGS + 1))
fi
echo ""

# 15. Environment variable sanitization check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "15. ENVIRONMENT VARIABLE SANITIZATION CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that environment variables are validated
IS_SAFE_ENV_VAR=$(grep -rn "is_safe_env_var" src/daemon/ --include="*.c" | wc -l | tr -d ' ')
if [ "$IS_SAFE_ENV_VAR" -gt 0 ]; then
  echo "✓ PASS: Environment variable sanitization implemented"
  echo "ℹ INFO: Found $IS_SAFE_ENV_VAR uses of is_safe_env_var()"
  INFOS=$((INFOS + 1))
else
  echo "⚠ WARN: No environment variable sanitization found"
  echo "Action: Implement is_safe_env_var() to block dangerous env vars (LD_PRELOAD, PATH, etc.)"
  WARNINGS=$((WARNINGS + 1))
fi
echo ""

# 16. AI provider timeout enforcement check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "16. AI PROVIDER TIMEOUT ENFORCEMENT CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that curl commands have --max-time flag
CURL_TIMEOUT=$(grep -rn "curl.*--max-time" src/foundation/argo_http.c 2>/dev/null | wc -l | tr -d ' ')
if [ "$CURL_TIMEOUT" -gt 0 ]; then
  echo "✓ PASS: AI provider timeout enforcement implemented"
  echo "ℹ INFO: Found $CURL_TIMEOUT curl commands with --max-time"
  INFOS=$((INFOS + 1))
else
  echo "⚠ WARN: No timeout enforcement in HTTP client"
  echo "Action: Add --max-time flag to curl commands to prevent indefinite hangs"
  WARNINGS=$((WARNINGS + 1))
fi
echo ""

# 17. Thread safety annotations check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "17. THREAD SAFETY ANNOTATIONS CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that structs with pthread_mutex_t have THREAD SAFETY documentation
STRUCTS_WITH_MUTEX=$(grep -rn "pthread_mutex_t" include/ --include="*.h" | grep -v "^[^:]*:.*extern" | wc -l | tr -d ' ')
THREAD_SAFETY_DOCS=$(grep -rn "THREAD SAFETY:" include/ --include="*.h" | wc -l | tr -d ' ')
if [ "$STRUCTS_WITH_MUTEX" -gt 0 ]; then
  if [ "$THREAD_SAFETY_DOCS" -ge "$STRUCTS_WITH_MUTEX" ]; then
    echo "✓ PASS: Thread safety documented for critical data structures"
    echo "ℹ INFO: Found $THREAD_SAFETY_DOCS thread safety annotations for $STRUCTS_WITH_MUTEX structs with mutexes"
    INFOS=$((INFOS + 1))
  else
    echo "⚠ WARN: $STRUCTS_WITH_MUTEX structs with mutexes, but only $THREAD_SAFETY_DOCS thread safety docs"
    echo "Action: Add THREAD SAFETY comments to document mutex protection"
    WARNINGS=$((WARNINGS + 1))
  fi
else
  echo "ℹ INFO: No structs with pthread_mutex_t found (single-threaded)"
  INFOS=$((INFOS + 1))
fi
echo ""

# 18. Return value checking
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "18. RETURN VALUE CHECKING (MISRA SUBSET)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check for unchecked return values of critical functions
# Critical functions that should always be checked:
# - malloc/calloc/realloc/strdup (memory allocation)
# - fopen/open (file operations)
# - pthread_mutex_lock/pthread_create (threading)
# - fork (process creation)

# Look for patterns where these functions are called without assignment or if-check
# This is a heuristic check - may have false positives/negatives
UNCHECKED_CALLS=0

# Check for standalone malloc/calloc calls (not assigned or checked)
UNCHECKED_MALLOC=$(grep -rn "^\s*malloc\|^\s*calloc\|^\s*realloc\|^\s*strdup" src/ --include="*.c" | \
  grep -v "=" | grep -v "if\s*(" | grep -v "//" | grep -v "/\*" | wc -l | tr -d ' ')
UNCHECKED_CALLS=$((UNCHECKED_CALLS + UNCHECKED_MALLOC))

# Check for standalone pthread calls
UNCHECKED_PTHREAD=$(grep -rn "^\s*pthread_mutex_lock\|^\s*pthread_create" src/ --include="*.c" | \
  grep -v "=" | grep -v "if\s*(" | grep -v "//" | grep -v "/\*" | wc -l | tr -d ' ')
UNCHECKED_CALLS=$((UNCHECKED_CALLS + UNCHECKED_PTHREAD))

if [ "$UNCHECKED_CALLS" -gt 5 ]; then
  echo "⚠ WARN: Found $UNCHECKED_CALLS potentially unchecked critical function calls"
  echo ""
  echo "Sample violations:"
  grep -rn "^\s*malloc\|^\s*calloc\|^\s*realloc\|^\s*strdup\|^\s*pthread_mutex_lock" src/ --include="*.c" | \
    grep -v "=" | grep -v "if\s*(" | grep -v "//" | grep -v "/\*" | head -5
  echo ""
  echo "Action: Check return values of malloc, pthread_mutex_lock, fopen, etc."
  echo "Note: Some false positives possible (e.g., void casts)"
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS: Most critical function return values appear to be checked"
  echo "ℹ INFO: Found $UNCHECKED_CALLS potentially unchecked calls (threshold: 5)"
  INFOS=$((INFOS + 1))
fi
echo ""

# 19. File descriptor leak detection
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "19. FILE DESCRIPTOR LEAK DETECTION"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that functions with fopen/open also have corresponding fclose/close
# This is a heuristic - looks for functions that open files but might not close them

# Count open operations
FOPEN_COUNT=$(grep -rn "\bfopen\b\|\bopen\b" src/ --include="*.c" | wc -l | tr -d ' ')
FCLOSE_COUNT=$(grep -rn "\bfclose\b\|\bclose\b" src/ --include="*.c" | wc -l | tr -d ' ')

if [ "$FOPEN_COUNT" -gt 0 ]; then
  # Check ratio - should be roughly equal
  RATIO=$((FCLOSE_COUNT * 100 / FOPEN_COUNT))

  if [ "$RATIO" -lt 80 ]; then
    echo "⚠ WARN: Potential file descriptor leaks detected"
    echo "  Open operations: $FOPEN_COUNT"
    echo "  Close operations: $FCLOSE_COUNT"
    echo "  Ratio: ${RATIO}%"
    echo ""
    echo "Action: Verify all fopen/open calls have corresponding fclose/close"
    echo "Recommendation: Run 'make valgrind' to detect actual leaks"
    WARNINGS=$((WARNINGS + 1))
  else
    echo "✓ PASS: File descriptor open/close ratio looks healthy"
    echo "ℹ INFO: Open: $FOPEN_COUNT, Close: $FCLOSE_COUNT (${RATIO}%)"
    INFOS=$((INFOS + 1))
  fi
else
  echo "ℹ INFO: No file operations found"
  INFOS=$((INFOS + 1))
fi
echo ""

# 20. Workflow state transition validation
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "20. WORKFLOW STATE TRANSITION VALIDATION"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that workflow state updates go through workflow_registry_update_state
# This ensures state transitions are validated and logged

# Count direct state assignments vs update_state calls
DIRECT_STATE_WRITES=$(grep -rn "\.state\s*=" src/daemon/ --include="*.c" | \
  grep -v "entry\.state\s*=\s*WORKFLOW_STATE_PENDING" | \
  grep -v "//" | grep -v "/\*" | wc -l | tr -d ' ')

UPDATE_STATE_CALLS=$(grep -rn "workflow_registry_update_state" src/daemon/ --include="*.c" | wc -l | tr -d ' ')

if [ "$DIRECT_STATE_WRITES" -gt 5 ]; then
  echo "⚠ WARN: Found $DIRECT_STATE_WRITES direct workflow state assignments"
  echo "  workflow_registry_update_state calls: $UPDATE_STATE_CALLS"
  echo ""
  echo "Action: Use workflow_registry_update_state() for all state changes"
  echo "Note: Initial state in workflow_entry_t is acceptable"
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS: Workflow state transitions properly use registry API"
  echo "ℹ INFO: Direct writes: $DIRECT_STATE_WRITES, API calls: $UPDATE_STATE_CALLS"
  INFOS=$((INFOS + 1))
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
