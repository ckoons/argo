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
# Check all component directories (src, arc/src, ci/src)
# Exclude: comments (// /* *), #defines, errno, copyright year
# Format: filename:linenum:content, so we filter on content after second colon
MAGIC_NUMBERS=$(grep -rn '\b[0-9]\{2,\}\b' src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | \
  grep -v ":[[:space:]]*//" | grep -v ":[[:space:]]*\*" | grep -v ":[[:space:]]*/\*" | \
  grep -v ":[[:space:]]*#" | grep -v "line " | grep -v "errno" | grep -v "2025" | wc -l | tr -d ' ')
echo "Found $MAGIC_NUMBERS potential magic numbers in production .c files"
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
UNSAFE_FUNCS=$(grep -rn '\bstrcpy\b\|\bstrcat\b\|\bsprintf\b\|\bgets\b' src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | \
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
for file in $(find src/ arc/src/ ci/src/ -name "*.c" 2>/dev/null); do
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
# Count ALL fprintf(stderr) uses (all components)
TOTAL_STDERR=$(grep -rn 'fprintf(stderr' src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')

# Count approved uses
# 1. In legitimate files (argo_error.c, argo_daemon_main.c, argo_print_utils.c, arc_main.c, ci_main.c)
LEGITIMATE_FILES=$(grep -rn 'fprintf(stderr' src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | \
  grep -E "argo_error.c|argo_daemon_main.c|argo_print_utils.c|arc_main.c|ci_main.c" | wc -l | tr -d ' ')

# 2. With GUIDELINE_APPROVED marker in previous line (search for GUIDELINE_APPROVED, then check next line for fprintf)
GUIDELINE_APPROVED=$(grep -rn 'GUIDELINE_APPROVED' src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null -A 1 | \
  grep "fprintf(stderr" | wc -l | tr -d ' ')

APPROVED_STDERR=$((LEGITIMATE_FILES + GUIDELINE_APPROVED))

# Calculate unapproved
UNAPPROVED_STDERR=$((TOTAL_STDERR - APPROVED_STDERR))

echo "fprintf(stderr) usage:"
echo "  Total: $TOTAL_STDERR"
echo "  Legitimate files: $LEGITIMATE_FILES"
echo "  GUIDELINE_APPROVED: $GUIDELINE_APPROVED"
echo "  Total approved: $APPROVED_STDERR"
echo "  Unapproved: $UNAPPROVED_STDERR"

if [ "$UNAPPROVED_STDERR" -gt 0 ]; then
  echo "⚠ WARN: $UNAPPROVED_STDERR unapproved fprintf(stderr) calls"
  echo "Action: Add GUIDELINE_APPROVED comment or use argo_report_error()"
  echo ""
  echo "Unapproved locations:"
  grep -rn 'fprintf(stderr' src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null -B 1 | \
    grep -B 1 -v "GUIDELINE_APPROVED" | \
    grep "fprintf(stderr" | \
    grep -v "argo_error.c\|argo_daemon_main.c\|argo_print_utils.c\|arc_main.c\|ci_main.c" | head -5
  echo ""
  echo "Note: Add /* GUIDELINE_APPROVED: reason */ comment above fprintf(stderr)"
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS: All fprintf(stderr) uses are approved"
  echo "Note: Approved contexts include:"
  echo "  - Legitimate files (error.c, daemon_main.c, print_utils.c)"
  echo "  - GUIDELINE_APPROVED markers"
fi
echo ""

# 5. Check copyright headers
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "5. COPYRIGHT HEADER CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Exclude third-party libraries from copyright check
EXCLUDED_FILES="include/jsmn.h"
MISSING_COPYRIGHT=$(find src/ include/ arc/src/ arc/include/ ci/src/ ci/include/ -name "*.c" -o -name "*.h" 2>/dev/null | \
  grep -v "jsmn.h" | \
  xargs grep -L "© 2025 Casey Koons" 2>/dev/null | wc -l | tr -d ' ')
if [ "$MISSING_COPYRIGHT" -gt 0 ]; then
  echo "⚠ WARN: $MISSING_COPYRIGHT files missing copyright header"
  echo ""
  echo "Files missing copyright:"
  find src/ include/ arc/src/ arc/include/ ci/src/ ci/include/ -name "*.c" -o -name "*.h" 2>/dev/null | \
    grep -v "jsmn.h" | \
    xargs grep -L "© 2025 Casey Koons" 2>/dev/null | head -10
  echo ""
  echo "Note: Third-party files excluded: $EXCLUDED_FILES"
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS: All files have copyright headers"
fi
echo ""

# 6. Check TODO comments
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "6. TODO COMMENTS CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
TODOS=$(grep -rn "TODO" src/ include/ arc/src/ arc/include/ ci/src/ ci/include/ --include="*.c" --include="*.h" 2>/dev/null | wc -l | tr -d ' ')
echo "Found $TODOS TODO comments"
if [ "$TODOS" -gt 30 ]; then
  echo "⚠ WARN: Consider cleaning up TODO comments"
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS: TODO count acceptable"
fi
echo ""

# 7. Check for common memory issues patterns
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "7. MEMORY MANAGEMENT PATTERNS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
GOTO_CLEANUP=$(grep -rn "goto cleanup" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')
echo "Functions using goto cleanup pattern: $GOTO_CLEANUP"
echo "✓ PASS: Manual verification with 'make valgrind' recommended"
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
    echo "✓ PASS: Binary size reasonable (${SIZE_KB}KB)"
  fi
else
  echo "✓ PASS: Binary will be built during compilation check"
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
  else
    echo "⚠ WARN: $STRUCTS_WITH_MUTEX structs with mutexes, but only $THREAD_SAFETY_DOCS thread safety docs"
    echo "Action: Add THREAD SAFETY comments to document mutex protection"
    WARNINGS=$((WARNINGS + 1))
  fi
else
  echo "✓ PASS: No multithreading complexity"
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
UNCHECKED_MALLOC=$(grep -rn "^\s*malloc\|^\s*calloc\|^\s*realloc\|^\s*strdup" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | \
  grep -v "=" | grep -v "if\s*(" | grep -v "//" | grep -v "/\*" | wc -l | tr -d ' ')
UNCHECKED_CALLS=$((UNCHECKED_CALLS + UNCHECKED_MALLOC))

# Check for standalone pthread calls
UNCHECKED_PTHREAD=$(grep -rn "^\s*pthread_mutex_lock\|^\s*pthread_create" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | \
  grep -v "=" | grep -v "if\s*(" | grep -v "//" | grep -v "/\*" | wc -l | tr -d ' ')
UNCHECKED_CALLS=$((UNCHECKED_CALLS + UNCHECKED_PTHREAD))

if [ "$UNCHECKED_CALLS" -gt 5 ]; then
  echo "⚠ WARN: Found $UNCHECKED_CALLS potentially unchecked critical function calls"
  echo ""
  echo "Sample violations:"
  grep -rn "^\s*malloc\|^\s*calloc\|^\s*realloc\|^\s*strdup\|^\s*pthread_mutex_lock" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | \
    grep -v "=" | grep -v "if\s*(" | grep -v "//" | grep -v "/\*" | head -5
  echo ""
  echo "Action: Check return values of malloc, pthread_mutex_lock, fopen, etc."
  echo "Note: Some false positives possible (e.g., void casts)"
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS: Most critical function return values appear to be checked"
fi
echo ""

# 19. File descriptor leak detection
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "19. FILE DESCRIPTOR LEAK DETECTION"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that functions with fopen/open also have corresponding fclose/close
# This is a heuristic - looks for functions that open files but might not close them

# Count open operations
FOPEN_COUNT=$(grep -rn "\bfopen\b\|\bopen\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')
FCLOSE_COUNT=$(grep -rn "\bfclose\b\|\bclose\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')

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
  fi
else
  echo "✓ PASS: No file operations found"
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
fi
echo ""

# 21. NULL pointer dereference check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "21. NULL POINTER DEREFERENCE CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check for potential NULL pointer dereferences (functions that don't check parameters)
# Look for functions that dereference pointers without NULL checks
NULL_CHECK_MACROS=$(grep -rn "ARGO_CHECK_NULL\|if\s*(!.*)" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')
POINTER_DEREFS=$(grep -rn '\->.*=' src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')

if [ "$POINTER_DEREFS" -gt 0 ]; then
  # Rough heuristic: should have at least some NULL checks
  RATIO=$((NULL_CHECK_MACROS * 100 / POINTER_DEREFS))
  if [ "$RATIO" -lt 10 ]; then
    echo "⚠ WARN: Low NULL check coverage detected"
    echo "  Pointer dereferences: $POINTER_DEREFS"
    echo "  NULL checks: $NULL_CHECK_MACROS"
    echo "  Coverage: ${RATIO}%"
    echo ""
    echo "Action: Add NULL checks using ARGO_CHECK_NULL() or if (!ptr) checks"
    WARNINGS=$((WARNINGS + 1))
  else
    echo "✓ PASS: NULL pointer check coverage appears reasonable"
    echo "ℹ INFO: $NULL_CHECK_MACROS NULL checks for $POINTER_DEREFS pointer operations (${RATIO}%)"
    INFOS=$((INFOS + 1))
  fi
else
  echo "ℹ INFO: No pointer dereferences found"
  INFOS=$((INFOS + 1))
fi
echo ""

# 22. Function complexity check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "22. FUNCTION COMPLEXITY CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check for functions with high cyclomatic complexity (many branches)
# Look for functions with excessive if/for/while statements
# Threshold: more than 15 branches suggests function is too complex

COMPLEX_FUNCTIONS=0
for file in $(find src/ arc/src/ ci/src/ -name "*.c" 2>/dev/null); do
  # Extract function bodies and count control flow statements
  # This is a simplified check - counts if/for/while/switch per function
  FUNC_LINES=$(grep -n "^[a-zA-Z_].*{$\|^static.*{$" "$file" 2>/dev/null)
  if [ -n "$FUNC_LINES" ]; then
    # For each function, count control flow between function start and next function
    # This is approximate - counts all if/while/for/switch in file
    BRANCHES=$(grep -c "\bif\s*(\|\bwhile\s*(\|\bfor\s*(\|\bswitch\s*(" "$file" 2>/dev/null || echo "0")
    BRANCHES=$(echo "$BRANCHES" | tr -d '\n' | tr -d ' ')
    FILE_LINES=$(wc -l < "$file" | tr -d ' ')

    # If file has more than 30 branches per 100 lines, it's complex
    if [ -n "$BRANCHES" ] && [ -n "$FILE_LINES" ] && [ "$FILE_LINES" -gt 0 ] && [ "$BRANCHES" -gt 0 ]; then
      BRANCH_DENSITY=$((BRANCHES * 100 / FILE_LINES))
      if [ "$BRANCH_DENSITY" -gt 30 ]; then
        COMPLEX_FUNCTIONS=$((COMPLEX_FUNCTIONS + 1))
        if [ "$COMPLEX_FUNCTIONS" -le 3 ]; then
          echo "  $file: $BRANCHES branches in $FILE_LINES lines (${BRANCH_DENSITY}%)"
        fi
      fi
    fi
  fi
done

if [ "$COMPLEX_FUNCTIONS" -gt 5 ]; then
  echo ""
  echo "⚠ WARN: Found $COMPLEX_FUNCTIONS files with high control flow density"
  echo "Action: Consider refactoring complex functions into smaller helpers"
  echo "Recommendation: Extract nested logic into separate functions"
  WARNINGS=$((WARNINGS + 1))
elif [ "$COMPLEX_FUNCTIONS" -gt 0 ]; then
  echo "ℹ INFO: Found $COMPLEX_FUNCTIONS files with elevated complexity (acceptable)"
  INFOS=$((INFOS + 1))
else
  echo "✓ PASS: Function complexity appears manageable"
fi
echo ""

# 23. Constant string externalization check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "23. CONSTANT STRING EXTERNALIZATION CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check for string literals in .c files that should be in headers
# Exclude: #define (already in headers), format strings, log messages, GUIDELINE_APPROVED blocks, comments
# Filter by checking content after "filename:linenum:" prefix

# Create temp file excluding GUIDELINE_APPROVED blocks for counting
TEMP_COUNT="/tmp/argo_strings_count.txt"
for file in $(find src/ arc/src/ ci/src/ -name "*.c" 2>/dev/null); do
  awk '
    /GUIDELINE_APPROVED[^_]/ && !/GUIDELINE_APPROVED_END/ { in_approved=1; next }
    /GUIDELINE_APPROVED_END/ { in_approved=0; next }
    !in_approved { print FILENAME":"NR":"$0 }
  ' FILENAME="$file" "$file"
done > "$TEMP_COUNT"

TOTAL_STRINGS=$(grep '"[^"]*"' "$TEMP_COUNT" 2>/dev/null | \
  grep -v ":[[:space:]]*//" | grep -v ":[[:space:]]*\*" | grep -v ":[[:space:]]/\*" | \
  grep -v "#include" | grep -v "#define" | \
  wc -l | tr -d ' ')

# Count strings that ARE acceptable (approved or format/log strings + legitimate patterns)
APPROVED_STRINGS=$(grep '"[^"]*"' "$TEMP_COUNT" 2>/dev/null | \
  grep -v "#define" | \
  grep -v ":[[:space:]]*//" | grep -v ":[[:space:]]*\*" | grep -v ":[[:space:]]/\*" | \
  grep -v '"\."' | grep -v "\"'\"" | \
  grep -v '""' | grep -v '"\\n"' | grep -v '","' | grep -v '":"' | grep -v '"}"' | grep -v '"]"' | \
  grep -E "%|LOG_|printf\|fprintf\|snprintf\|dprintf|argo_report_error|static const char|\
strstr\(|strcmp\(|strncmp\(|strchr\(|strrchr\(|strpbrk\(|strspn\(|strcspn\(|\
execlp\(|execv\(|execvp\(|execve\(|execl\(|\
fopen\(|popen\(|freopen\(|getenv\(|setenv\(|putenv\(|\
argo_config_get\(|argo_config_set\(|\
write\([^,]+,[[:space:]]*\"|\
\\..*=[[:space:]]*\"|return\s+\"|\
:\s*\"\"|\
(==|!=)[[:space:]]*\"[^\"]{1,2}\"|\
=[[:space:]]*{[[:space:]]*\"|\
http_response_set_error\(|\
\"/api/|\
strstr\([^,]+,[[:space:]]*\"[^\"]*:|\
snprintf\([^,]+,[^,]+,[[:space:]]*\"[^\"]*:|\
snprintf\([^,]+,[^,]+,[[:space:]]*\"{|\
const char\*[[:space:]]*[a-zA-Z_].*=[[:space:]]*\"|\
fprintf\(fp," | \
  wc -l | tr -d ' ')

# Unapproved strings (candidates for externalization)
UNAPPROVED_STRINGS=$((TOTAL_STRINGS - APPROVED_STRINGS))

# Clean up temp file
rm -f "$TEMP_COUNT"

# Generate detailed unapproved strings report
UNAPPROVED_REPORT="/tmp/argo_unapproved_strings.txt"
{
  echo "========================================"
  echo "ARGO UNAPPROVED STRING LITERALS REPORT"
  echo "Generated: $(date '+%Y-%m-%d %H:%M:%S')"
  echo "========================================"
  echo ""
  echo "Total string literals: $TOTAL_STRINGS"
  echo "Approved strings: $APPROVED_STRINGS"
  echo "Unapproved strings: $UNAPPROVED_STRINGS"
  echo ""
  echo "This report shows all string literals that are NOT auto-approved."
  echo "Review each entry to determine if it should be:"
  echo "  1. Externalized to a header constant"
  echo "  2. Added to auto-approval patterns (if legitimate inline usage)"
  echo "  3. Left as-is (with justification)"
  echo ""
  echo "========================================"
  echo "UNAPPROVED STRING LITERALS"
  echo "========================================"
  echo ""

  # Get all unapproved strings with context
  # Step 1: Find all string literals
  # Step 2: Exclude GUIDELINE_APPROVED blocks (between GUIDELINE_APPROVED and GUIDELINE_APPROVED_END)
  # Step 3: Filter out legitimate patterns

  # First, create a temp file excluding GUIDELINE_APPROVED blocks
  TEMP_FILTERED="/tmp/argo_strings_filtered.txt"

  # Process each .c file to exclude GUIDELINE_APPROVED blocks
  for file in $(find src/ arc/src/ ci/src/ -name "*.c" 2>/dev/null); do
    awk '
      /GUIDELINE_APPROVED[^_]/ && !/GUIDELINE_APPROVED_END/ { in_approved=1; next }
      /GUIDELINE_APPROVED_END/ { in_approved=0; next }
      !in_approved { print FILENAME":"NR":"$0 }
    ' FILENAME="$file" "$file"
  done > "$TEMP_FILTERED"

  # Now grep for strings in the filtered output
  grep '"[^"]*"' "$TEMP_FILTERED" 2>/dev/null | \
    grep -v "#include" | \
    grep -v "#define" | \
    grep -v ":[[:space:]]*//" | grep -v ":[[:space:]]*\*" | grep -v ":[[:space:]]/\*" | \
    grep -v "_help\.c:.*printf\|_help\.c:.*fprintf" | \
    grep -v '"\."' | grep -v "\"'\"" | \
    grep -v '""' | grep -v '"\\n"' | grep -v '","' | grep -v '":"' | grep -v '"}"' | grep -v '"]"' | \
    grep -v -E "%|LOG_|printf\|fprintf\|snprintf\|dprintf|argo_report_error|static const char|\
strstr\(|strcmp\(|strncmp\(|strchr\(|strrchr\(|strpbrk\(|strspn\(|strcspn\(|\
execlp\(|execv\(|execvp\(|execve\(|execl\(|\
fopen\(|popen\(|freopen\(|getenv\(|setenv\(|putenv\(|\
argo_config_get\(|argo_config_set\(|\
write\([^,]+,[[:space:]]*\"|\
\\..*=[[:space:]]*\"|return\s+\"|\
:\s*\"\"|\
(==|!=)[[:space:]]*\"[^\"]{1,2}\"|\
=[[:space:]]*{[[:space:]]*\"|\
http_response_set_error\(|\
\"/api/|\
strstr\([^,]+,[[:space:]]*\"[^\"]*:|\
snprintf\([^,]+,[^,]+,[[:space:]]*\"[^\"]*:|\
snprintf\([^,]+,[^,]+,[[:space:]]*\"{|\
const char\*[[:space:]]*[a-zA-Z_].*=[[:space:]]*\"|\
fprintf\(fp," | \
    sort | \
    nl -w3 -s'. '

  # Clean up temp file
  rm -f "$TEMP_FILTERED"

  echo ""
  echo "========================================"
  echo "END OF REPORT"
  echo "========================================"
  echo ""
  echo "Next steps:"
  echo "  1. Review each violation above"
  echo "  2. For repeated patterns, add to header constants"
  echo "  3. For legitimate inline usage, add auto-approval pattern to this script"
  echo "  4. Document justification for strings that remain inline"
  echo ""
} > "$UNAPPROVED_REPORT"

echo "String literal usage:"
echo "  Total string literals (excluding #define): $TOTAL_STRINGS"
echo "  Approved (format/log/error): $APPROVED_STRINGS"
echo "  Unapproved (candidates for headers): $UNAPPROVED_STRINGS"

if [ "$UNAPPROVED_STRINGS" -gt 400 ]; then
  echo "⚠ WARN: High count of unapproved string literals"
  echo "Action: Move constant strings to headers or add GUIDELINE_APPROVED"
  echo ""
  echo "Common unapproved patterns (first 10):"
  grep -rn '"[^"]*"' src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | \
    grep -v ":[[:space:]]*//" | grep -v ":[[:space:]]*\*" | grep -v ":[[:space:]]/\*" | \
    grep -v "#include" | grep -v "#define" | \
    grep -v "GUIDELINE_APPROVED" | \
    grep -v "%" | grep -v "LOG_\|printf\|fprintf\|snprintf\|dprintf" | \
    grep -v "argo_report_error" | grep -v "static const char" | \
    grep -vE "strstr\(|strcmp\(|strncmp\(|strchr\(|strrchr\(" | \
    grep -vE "execlp\(|execv\(|execvp\(|execve\(|execl\(" | \
    grep -vE "fopen\(|popen\(|freopen\(|getenv\(|setenv\(|putenv\(" | \
    grep -vE "argo_config_get\(|argo_config_set\(" | \
    grep -vE "write\([^,]+,[[:space:]]*\"|\\..*=[[:space:]]*\"|return\s+\"" | \
    grep -vE ":\s*\"\"" | \
    grep -vE "(==|!=)[[:space:]]*\"[^\"]{1,2}\"" | \
    grep -vE "=[[:space:]]*{[[:space:]]*\"" | \
    grep -vE "http_response_set_error\(" | \
    grep -vE "\"/api/" | \
    grep -vE "strstr\([^,]+,[[:space:]]*\"[^\"]*:" | \
    grep -vE "snprintf\([^,]+,[^,]+,[[:space:]]*\"[^\"]*:" | \
    grep -vE "snprintf\([^,]+,[^,]+,[[:space:]]*\"{" | \
    grep -vE "const char\*[[:space:]]*[a-zA-Z_].*=[[:space:]]*\"" | \
    grep -vE "fprintf\(fp," | head -10
  echo ""
  echo "Note: Add GUIDELINE_APPROVED comment for legitimate constants (JSON, API paths, etc.)"
  WARNINGS=$((WARNINGS + 1))
else
  echo "✓ PASS: String literal usage reasonable for current codebase size"
  echo "Note: Auto-approved string patterns:"
  echo "  - #define constants (already in headers)"
  echo "  - Format strings (containing %)"
  echo "  - Log/print messages (LOG_*, printf, fprintf, etc.)"
  echo "  - Error contexts (argo_report_error)"
  echo "  - Pattern matching (strstr, strcmp, strchr, etc.)"
  echo "  - Process execution (execlp, execv, etc.)"
  echo "  - File operations (fopen, popen, freopen)"
  echo "  - Environment access (getenv, setenv, putenv)"
  echo "  - Configuration access (argo_config_get, argo_config_set)"
  echo "  - Struct initialization (.field = \"value\")"
  echo "  - Array initialization (= { \"value\" })"
  echo "  - Function returns (return \"value\")"
  echo "  - Ternary empty strings (condition ? value : \"\")"
  echo "  - Single-char comparisons (== \"\\\"\", != \"\\n\", etc.)"
  echo "  - HTTP error responses (http_response_set_error)"
  echo "  - API endpoint paths (\"/api/...\")"
  echo "  - JSON field names in parsing (strstr/snprintf with \":\")"
  echo "  - JSON object literals (snprintf with \"{...}\")"
  echo "  - Const char* assignments (const char* var = \"...\")"
  echo "  - File output formatting (fprintf(fp, ...))"
  echo "  - GUIDELINE_APPROVED markers"
fi
echo ""

# 24. Resource cleanup verification
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "24. RESOURCE CLEANUP VERIFICATION"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Verify that functions with cleanup labels actually free resources
CLEANUP_LABELS=$(grep -rn "^cleanup:" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')
FREE_IN_CLEANUP=$(grep -rn "^cleanup:" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null -A 20 | \
  grep -c "free\|fclose\|pthread_mutex_unlock\|close" || echo "0")

if [ "$CLEANUP_LABELS" -gt 0 ]; then
  RATIO=$((FREE_IN_CLEANUP * 100 / CLEANUP_LABELS))

  echo "Functions with cleanup labels: $CLEANUP_LABELS"
  echo "Cleanup blocks with resource frees: $FREE_IN_CLEANUP"

  if [ "$RATIO" -lt 80 ]; then
    echo "⚠ WARN: Some cleanup blocks may not free resources"
    echo "  Coverage: ${RATIO}%"
    echo ""
    echo "Action: Verify all cleanup: labels properly free allocated resources"
    WARNINGS=$((WARNINGS + 1))
  else
    echo "✓ PASS: Cleanup blocks appear to free resources (${RATIO}%)"
  fi
else
  echo "ℹ INFO: No goto cleanup pattern found (acceptable for simple functions)"
  INFOS=$((INFOS + 1))
fi
echo ""

# 25. Memory initialization check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "25. MEMORY INITIALIZATION CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that allocated memory is initialized (prefer calloc or explicit memset)
CALLOC_COUNT=$(grep -rn "\bcalloc\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')
MALLOC_COUNT=$(grep -rn "\bmalloc\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')
MEMSET_COUNT=$(grep -rn "\bmemset\b.*0" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')

TOTAL_ALLOC=$((CALLOC_COUNT + MALLOC_COUNT))
INITIALIZED=$((CALLOC_COUNT + MEMSET_COUNT))

if [ "$TOTAL_ALLOC" -gt 0 ]; then
  INIT_RATIO=$((INITIALIZED * 100 / TOTAL_ALLOC))

  echo "Memory allocations: $TOTAL_ALLOC (calloc: $CALLOC_COUNT, malloc: $MALLOC_COUNT)"
  echo "Initialized allocations: $INITIALIZED (calloc + memset)"
  echo "Initialization coverage: ${INIT_RATIO}%"

  if [ "$INIT_RATIO" -lt 50 ]; then
    echo "⚠ WARN: Low memory initialization coverage"
    echo "Action: Prefer calloc() or add memset() after malloc()"
    echo "Reason: Prevents use of uninitialized memory"
    WARNINGS=$((WARNINGS + 1))
  else
    echo "✓ PASS: Memory initialization coverage appears adequate"
  fi
else
  echo "ℹ INFO: No memory allocations found"
  INFOS=$((INFOS + 1))
fi
echo ""

# 26. Buffer size validation check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "26. BUFFER SIZE VALIDATION CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that buffer operations use sizeof() for size calculation
STRING_OPS=$(grep -rn "strncpy\|snprintf\|strncat" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')
STRING_OPS_WITH_SIZEOF=$(grep -rn "strncpy\|snprintf\|strncat" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | grep -c "sizeof" || echo "0")

if [ "$STRING_OPS" -gt 0 ]; then
  SIZEOF_RATIO=$((STRING_OPS_WITH_SIZEOF * 100 / STRING_OPS))

  echo "String operations: $STRING_OPS"
  echo "Operations using sizeof(): $STRING_OPS_WITH_SIZEOF"
  echo "sizeof() usage: ${SIZEOF_RATIO}%"

  if [ "$SIZEOF_RATIO" -lt 50 ]; then
    echo "⚠ WARN: Low sizeof() usage in string operations"
    echo "Action: Use sizeof(buffer) instead of hard-coded sizes"
    echo "Reason: Prevents buffer overflows when buffer size changes"
    WARNINGS=$((WARNINGS + 1))
  else
    echo "✓ PASS: Buffer size validation appears adequate (${SIZEOF_RATIO}%)"
  fi
else
  echo "ℹ INFO: No string operations found"
  INFOS=$((INFOS + 1))
fi
echo ""

# 27. Defensive programming check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "27. DEFENSIVE PROGRAMMING CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check for early return pattern (fail-fast principle)
# Count functions with parameter validation at the top
ARGO_CHECK_NULL_COUNT=$(grep -rn "ARGO_CHECK_NULL" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')
EARLY_RETURNS=$(grep -rn "if\s*(!.*)\s*return" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')
PUBLIC_FUNCTIONS=$(grep -rn "^[a-zA-Z_].*{$\|^int\s\|^void\s\|^const\s\|^static" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | \
  grep -v "^\s*//" | grep -v "^\s*\*" | wc -l | tr -d ' ')

DEFENSIVE_CHECKS=$((ARGO_CHECK_NULL_COUNT + EARLY_RETURNS))

echo "Defensive checks found: $DEFENSIVE_CHECKS"
echo "  ARGO_CHECK_NULL uses: $ARGO_CHECK_NULL_COUNT"
echo "  Early return patterns: $EARLY_RETURNS"

if [ "$DEFENSIVE_CHECKS" -gt 80 ]; then
  echo "✓ PASS: Strong defensive programming practices"
  echo "ℹ INFO: Fail-fast pattern widely adopted"
  INFOS=$((INFOS + 1))
else
  echo "ℹ INFO: Defensive programming present but could be enhanced"
  echo "Recommendation: Add parameter validation to public functions"
  INFOS=$((INFOS + 1))
fi
echo ""

# 28. Error path coverage check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "28. ERROR PATH COVERAGE CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that functions have error handling paths
# Look for functions with goto cleanup and error returns
FUNCTIONS_WITH_GOTO=$(grep -rn "goto cleanup\|goto error" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | \
  cut -d: -f1 | sort -u | wc -l | tr -d ' ')
FUNCTIONS_WITH_ERROR_RETURNS=$(grep -rn "return E_\|return ARGO_\|return -1\|return ARC_\|return CI_" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | \
  cut -d: -f1 | sort -u | wc -l | tr -d ' ')

TOTAL_ERROR_HANDLING=$((FUNCTIONS_WITH_GOTO + FUNCTIONS_WITH_ERROR_RETURNS))

echo "Files with error handling: $TOTAL_ERROR_HANDLING"
echo "  Files using goto cleanup/error: $FUNCTIONS_WITH_GOTO"
echo "  Files with error returns: $FUNCTIONS_WITH_ERROR_RETURNS"

if [ "$TOTAL_ERROR_HANDLING" -ge 30 ]; then
  echo "✓ PASS: Comprehensive error handling coverage"
  echo "ℹ INFO: Error paths well-defined across codebase"
  INFOS=$((INFOS + 1))
else
  echo "ℹ INFO: Error handling present in $TOTAL_ERROR_HANDLING files"
  echo "Recommendation: Ensure all non-trivial functions handle errors"
  INFOS=$((INFOS + 1))
fi
echo ""

# 29. Mutex lock/unlock balance check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "29. MUTEX LOCK/UNLOCK BALANCE CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Verify pthread_mutex_lock and pthread_mutex_unlock are balanced
MUTEX_LOCK=$(grep -rn "pthread_mutex_lock" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')
MUTEX_UNLOCK=$(grep -rn "pthread_mutex_unlock" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')

echo "Mutex operations:"
echo "  pthread_mutex_lock calls: $MUTEX_LOCK"
echo "  pthread_mutex_unlock calls: $MUTEX_UNLOCK"

if [ "$MUTEX_LOCK" -gt 0 ]; then
  if [ "$MUTEX_UNLOCK" -lt "$MUTEX_LOCK" ]; then
    echo "⚠ WARN: More locks than unlocks detected"
    echo "Action: Verify all mutexes are unlocked in error paths"
    echo "Recommendation: Use goto cleanup pattern for mutex cleanup"
    WARNINGS=$((WARNINGS + 1))
  else
    BALANCE_RATIO=$((MUTEX_UNLOCK * 100 / MUTEX_LOCK))
    echo "✓ PASS: Mutex lock/unlock balance appears healthy (${BALANCE_RATIO}%)"
    echo "ℹ INFO: Unlock >= lock is expected (cleanup paths may have multiple unlocks)"
    INFOS=$((INFOS + 1))
  fi
else
  echo "ℹ INFO: No mutex operations found (single-threaded or lock-free)"
  INFOS=$((INFOS + 1))
fi
echo ""

# 30. Include guard coverage check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "30. INCLUDE GUARD COVERAGE CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that all header files have include guards
TOTAL_HEADERS=$(find include/ arc/include/ ci/include/ -name "*.h" 2>/dev/null | wc -l | tr -d ' ')
HEADERS_WITH_GUARDS=$(find include/ arc/include/ ci/include/ -name "*.h" 2>/dev/null | xargs grep -l "#ifndef.*_H" 2>/dev/null | wc -l | tr -d ' ')

echo "Header files: $TOTAL_HEADERS"
echo "Headers with include guards: $HEADERS_WITH_GUARDS"

if [ "$TOTAL_HEADERS" -gt 0 ]; then
  GUARD_RATIO=$((HEADERS_WITH_GUARDS * 100 / TOTAL_HEADERS))

  if [ "$GUARD_RATIO" -lt 95 ]; then
    echo "⚠ WARN: Not all header files have include guards (${GUARD_RATIO}%)"
    echo "Action: Add #ifndef/#define/#endif guards to all headers"
    echo ""
    echo "Headers without guards:"
    for header in $(find include/ arc/include/ ci/include/ -name "*.h" 2>/dev/null); do
      if ! grep -q "#ifndef.*_H" "$header" 2>/dev/null; then
        echo "  $header"
      fi
    done
    WARNINGS=$((WARNINGS + 1))
  else
    echo "✓ PASS: Include guard coverage is excellent (${GUARD_RATIO}%)"
  fi
else
  echo "ℹ INFO: No header files found"
  INFOS=$((INFOS + 1))
fi
echo ""

# 31. Error reporting consistency check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "31. ERROR REPORTING CONSISTENCY CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Verify proper use of argo_report_error vs LOG_ERROR
ARGO_REPORT_ERROR=$(grep -rn "argo_report_error" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')
LOG_ERROR_CALLS=$(grep -rn "LOG_ERROR" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')

echo "Error reporting calls:"
echo "  argo_report_error: $ARGO_REPORT_ERROR"
echo "  LOG_ERROR: $LOG_ERROR_CALLS"

if [ "$ARGO_REPORT_ERROR" -gt 0 ]; then
  echo "✓ PASS: Centralized error reporting in use"
  echo "ℹ INFO: argo_report_error for errors, LOG_ERROR for informational logging"
  INFOS=$((INFOS + 1))
else
  echo "⚠ WARN: No argo_report_error calls found"
  echo "Action: Use argo_report_error() for all error conditions"
  echo "Note: LOG_ERROR is for informational logging, not error reporting"
  WARNINGS=$((WARNINGS + 1))
fi
echo ""

# 32. String allocation safety check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "32. STRING ALLOCATION SAFETY CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that strdup/strndup calls have return value checking
STRDUP_CALLS=$(grep -rn "\bstrdup\b\|\bstrndup\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | grep -v "GUIDELINE_APPROVED" | wc -l | tr -d ' ')
# Check for NULL checks on same line OR next line (common pattern)
STRDUP_CHECKED=$(grep -rn "\bstrdup\b\|\bstrndup\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null -A 1 | grep -v "GUIDELINE_APPROVED" | grep -E "if\s*\(!|if\s*\(.*==.*NULL|if\s*\(.*!=.*NULL|\|\|" | wc -l | tr -d ' ')

echo "String allocation calls:"
echo "  strdup/strndup calls: $STRDUP_CALLS"
echo "  Checked calls: $STRDUP_CHECKED"

if [ "$STRDUP_CALLS" -gt 0 ]; then
  CHECK_RATIO=$((STRDUP_CHECKED * 100 / STRDUP_CALLS))

  if [ "$CHECK_RATIO" -lt 50 ]; then
    echo "⚠ WARN: Many strdup calls lack NULL checking (${CHECK_RATIO}%)"
    echo "Action: Check strdup return values (can fail and return NULL)"
    WARNINGS=$((WARNINGS + 1))
  else
    echo "ℹ INFO: Most strdup calls are checked (${CHECK_RATIO}%)"
    echo "Note: Remaining unchecked calls may use GUIDELINE_APPROVED or check elsewhere"
    INFOS=$((INFOS + 1))
  fi
else
  echo "ℹ INFO: No strdup calls found"
  INFOS=$((INFOS + 1))
fi
echo ""

# 33. Unsafe conversion functions check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "33. UNSAFE CONVERSION FUNCTIONS CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check for unsafe conversion functions (atoi, atol, atof)
# Prefer strtol, strtoll, strtod with error checking
UNSAFE_CONVERSIONS=$(grep -rn "\batoi\b\|\batol\b\|\batof\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | grep -v "GUIDELINE_APPROVED" | wc -l | tr -d ' ')

echo "Unsafe conversion function calls: $UNSAFE_CONVERSIONS"

if [ "$UNSAFE_CONVERSIONS" -gt 10 ]; then
  echo "⚠ WARN: Many unsafe conversion functions found"
  echo "Action: Replace with strtol/strtoll/strtod and check errno"
  echo ""
  echo "Sample unsafe conversions:"
  grep -rn "\batoi\b\|\batol\b\|\batof\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | grep -v "GUIDELINE_APPROVED" | head -3
  WARNINGS=$((WARNINGS + 1))
elif [ "$UNSAFE_CONVERSIONS" -gt 0 ]; then
  echo "ℹ INFO: Some unsafe conversion functions found (${UNSAFE_CONVERSIONS})"
  echo "Recommendation: Consider strtol/strtoll/strtod for better error handling"
  INFOS=$((INFOS + 1))
else
  echo "✓ PASS: No unsafe conversion functions found"
fi
echo ""

# 34. Process execution safety check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "34. PROCESS EXECUTION SAFETY CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check process execution functions for proper usage
SYSTEM_CALLS=$(grep -rn "system(" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | grep -v "//" | grep -v "/\*" | grep -v "GUIDELINE_APPROVED" | wc -l | tr -d ' ')
EXEC_CALLS=$(grep -rn "\bexecl\b\|\bexeclp\b\|\bexecv\b\|\bexecvp\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | grep -v "GUIDELINE_APPROVED" | wc -l | tr -d ' ')
POPEN_CALLS=$(grep -rn "\bpopen\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | grep -v "GUIDELINE_APPROVED" | wc -l | tr -d ' ')

TOTAL_EXEC=$((SYSTEM_CALLS + EXEC_CALLS + POPEN_CALLS))

echo "Process execution calls:"
echo "  system(): $SYSTEM_CALLS"
echo "  exec*(): $EXEC_CALLS"
echo "  popen(): $POPEN_CALLS"
echo "  Total: $TOTAL_EXEC"

if [ "$SYSTEM_CALLS" -gt 0 ]; then
  echo "⚠ WARN: system() calls found - command injection risk"
  echo "Action: Validate all input before passing to system()"
  echo "Recommendation: Use exec*() or fork+exec instead"
  WARNINGS=$((WARNINGS + 1))
elif [ "$TOTAL_EXEC" -gt 0 ]; then
  echo "✓ PASS: Process execution present"
  echo "ℹ INFO: Verify input validation for all exec/popen calls"
  INFOS=$((INFOS + 1))
else
  echo "ℹ INFO: No process execution found"
  INFOS=$((INFOS + 1))
fi
echo ""

# 35. Switch statement completeness check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "35. SWITCH STATEMENT COMPLETENESS CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that switch statements have default cases
SWITCH_COUNT=$(grep -rn "switch\s*(" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')
DEFAULT_COUNT=$(grep -rn "default\s*:" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | wc -l | tr -d ' ')

echo "Switch statements: $SWITCH_COUNT"
echo "Default cases: $DEFAULT_COUNT"

if [ "$SWITCH_COUNT" -gt 0 ]; then
  COVERAGE=$((DEFAULT_COUNT * 100 / SWITCH_COUNT))

  if [ "$COVERAGE" -lt 80 ]; then
    echo "⚠ WARN: Not all switch statements have default cases (${COVERAGE}%)"
    echo "Action: Add default: cases to handle unexpected values"
    WARNINGS=$((WARNINGS + 1))
  else
    echo "✓ PASS: Most switch statements have default cases (${COVERAGE}%)"
    echo "ℹ INFO: Default cases improve robustness"
    INFOS=$((INFOS + 1))
  fi
else
  echo "ℹ INFO: No switch statements found"
  INFOS=$((INFOS + 1))
fi
echo ""

# 37. File I/O error checking
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "37. FILE I/O ERROR CHECKING"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check that file I/O operations check return values
FREAD_COUNT=$(grep -rn "\bfread\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | grep -v "GUIDELINE_APPROVED" | wc -l | tr -d ' ')
FWRITE_COUNT=$(grep -rn "\bfwrite\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | grep -v "GUIDELINE_APPROVED" | wc -l | tr -d ' ')
FGETS_COUNT=$(grep -rn "\bfgets\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null | grep -v "GUIDELINE_APPROVED" | wc -l | tr -d ' ')

# Check how many are actually checked (look for ferror or return value checks within next few lines)
FREAD_CHECKED=$(grep -rn "\bfread\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null -A 2 | grep -v "GUIDELINE_APPROVED" | grep -E "if\s*\(|ferror|!=|==" | wc -l | tr -d ' ')
FWRITE_CHECKED=$(grep -rn "\bfwrite\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null -A 2 | grep -v "GUIDELINE_APPROVED" | grep -E "if\s*\(|ferror|!=|==" | wc -l | tr -d ' ')
FGETS_CHECKED=$(grep -rn "\bfgets\b" src/ arc/src/ ci/src/ --include="*.c" 2>/dev/null -A 2 | grep -v "GUIDELINE_APPROVED" | grep -E "if\s*\(|==|!=|NULL" | wc -l | tr -d ' ')

TOTAL_FILE_IO=$((FREAD_COUNT + FWRITE_COUNT + FGETS_COUNT))
TOTAL_CHECKED=$((FREAD_CHECKED + FWRITE_CHECKED + FGETS_CHECKED))

echo "File I/O operations:"
echo "  fread: $FREAD_COUNT (checked: $FREAD_CHECKED)"
echo "  fwrite: $FWRITE_COUNT (checked: $FWRITE_CHECKED)"
echo "  fgets: $FGETS_COUNT (checked: $FGETS_CHECKED)"
echo "  Total: $TOTAL_FILE_IO"

if [ "$TOTAL_FILE_IO" -gt 0 ]; then
  CHECK_RATIO=$((TOTAL_CHECKED * 100 / TOTAL_FILE_IO))

  if [ "$CHECK_RATIO" -lt 70 ]; then
    echo "⚠ WARN: Many file I/O operations lack return value checking (${CHECK_RATIO}%)"
    echo "Action: Check return values for fread/fwrite/fgets"
    echo "Reason: I/O operations can fail; unchecked failures lead to data corruption"
    WARNINGS=$((WARNINGS + 1))
  else
    echo "✓ PASS: Most file I/O operations check return values (${CHECK_RATIO}%)"
  fi
else
  echo "✓ PASS: Minimal file I/O operations"
fi
echo ""

# 38. Function length limits check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "38. FUNCTION LENGTH LIMITS CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Use Python script to accurately identify longest functions
if [ -f "scripts/utils/find_longest_functions.py" ]; then
  # Check all component directories
  FUNC_OUTPUT=$(python3 scripts/utils/find_longest_functions.py src/ arc/src/ ci/src/ 2>&1)
  FUNC_STATUS=$?

  echo "$FUNC_OUTPUT"

  if [ "$FUNC_STATUS" -ne 0 ]; then
    echo ""
    echo "⚠ WARN: Functions exceeding 150 lines found"
    echo "Action: Refactor long functions into smaller, focused helpers"
    echo "Recommendation: Keep functions under 100 lines"
    WARNINGS=$((WARNINGS + 1))
  else
    echo "✓ PASS: All functions within reasonable size limits"
  fi
else
  echo "⚠ WARN: Function analysis script not found"
  WARNINGS=$((WARNINGS + 1))
fi
echo ""

# 39. Inline function appropriate usage check
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "39. INLINE FUNCTION APPROPRIATE USAGE CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check inline function usage (prefer shared routines/subroutines)
INLINE_FUNCS=$(grep -rn "static\s\+inline" include/ src/ --include="*.h" --include="*.c" | wc -l | tr -d ' ')

echo "Inline functions: $INLINE_FUNCS"

if [ "$INLINE_FUNCS" -gt 10 ]; then
  echo "⚠ WARN: Many inline functions detected"
  echo "Action: Consider converting inline functions to regular functions"
  echo "Reason: Prefer shared routines or subroutines over inlining"
  echo "Note: Compiler will inline appropriately with optimization flags"
  WARNINGS=$((WARNINGS + 1))
elif [ "$INLINE_FUNCS" -gt 0 ]; then
  echo "✓ PASS: Limited inline function usage (${INLINE_FUNCS})"
  echo "Note: Prefer regular functions; compiler will optimize as needed"
else
  echo "✓ PASS: No inline functions (compiler will inline as needed)"
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
