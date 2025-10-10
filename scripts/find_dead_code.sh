#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Custom dead code detection for Argo

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "=========================================="
echo "Argo Dead Code Detection"
echo "=========================================="
echo ""

# 1. Find functions defined but never called
echo "[1/4] Finding unused functions..."
echo "----------------------------------------"

# Get all function definitions (exclude static for now)
grep -rh "^[a-z_][a-z0-9_]* [a-z_][a-z0-9_]*(" src/ arc/src/ 2>/dev/null | \
    sed 's/(.*//' | awk '{print $NF}' | sort -u > /tmp/argo_defined.txt

# Get all function calls
grep -roh "[a-z_][a-z0-9_]*(" src/ arc/src/ 2>/dev/null | \
    sed 's/(//' | sort -u > /tmp/argo_called.txt

# Find difference (defined but not called)
unused=$(comm -23 /tmp/argo_defined.txt /tmp/argo_called.txt | wc -l | tr -d ' ')
echo "  Found $unused potentially unused non-static functions"
echo "  (See full report with: cppcheck --enable=unusedFunction)"
echo ""

# 2. Find #ifdef blocks that are never enabled
echo "[2/4] Finding potentially dead #ifdef blocks..."
echo "----------------------------------------"

# Find all #ifdef/#ifndef macros
ifdefs=$(grep -rh "#ifdef\|#ifndef" src/ arc/src/ 2>/dev/null | \
    sed 's/.*#ifdef \(.*\)/\1/' | sed 's/.*#ifndef \(.*\)/\1/' | \
    grep -v "^_\|^__" | sort -u)

dead_ifdefs=0
for macro in $ifdefs; do
    # Check if macro is ever defined
    if ! grep -rq "#define $macro\|CFLAGS.*-D$macro" src/ arc/src/ Makefile* 2>/dev/null; then
        echo "  Potentially unused: #ifdef $macro"
        dead_ifdefs=$((dead_ifdefs + 1))
    fi
done

if [ $dead_ifdefs -eq 0 ]; then
    echo "  ✓ No dead #ifdef blocks found"
fi
echo ""

# 3. Find included headers that might not be needed
echo "[3/4] Finding potentially unused includes..."
echo "----------------------------------------"

# Sample a few files and check includes
checked=0
unused_includes=0

for file in $(ls src/foundation/*.c src/daemon/*.c 2>/dev/null | head -10); do
    if [ -f "$file" ]; then
        # Get includes from this file
        includes=$(grep "^#include \"" "$file" | sed 's/.*"\(.*\)".*/\1/')

        for inc in $includes; do
            # Check if any symbols from this header are used
            # This is a simple heuristic - check if header basename appears in code
            basename=$(basename "$inc" .h)

            if ! grep -q "$basename" "$file"; then
                echo "  $file: possibly unused #include \"$inc\""
                unused_includes=$((unused_includes + 1))
            fi
        done
        checked=$((checked + 1))
    fi
done

if [ $unused_includes -eq 0 ]; then
    echo "  ✓ No obviously unused includes in sampled files"
else
    echo "  (Sampled $checked files - run full analysis with: include-what-you-use)"
fi
echo ""

# 4. Find variables declared but never used (compile-time check)
echo "[4/4] Finding unused variables (requires recompile)..."
echo "----------------------------------------"

# Quick compile check for unused variables
make clean > /dev/null 2>&1
make CFLAGS="-Wall -Wextra -Wunused-variable -Wunused-parameter -std=c11 -g" 2>&1 | \
    grep -E "unused variable|unused parameter" | head -10 || echo "  ✓ No unused variables detected"

echo ""
echo "=========================================="
echo "Dead Code Detection Complete"
echo "=========================================="
echo ""
echo "Summary:"
echo "  - Unused functions: $unused (see CODE_ANALYSIS_REPORT.md for details)"
echo "  - Unused #ifdef blocks: $dead_ifdefs"
echo "  - Potentially unused includes: $unused_includes (sampled)"
echo ""
echo "For detailed analysis:"
echo "  - Run: ./scripts/analyze_code.sh"
echo "  - Review: CODE_ANALYSIS_REPORT.md"
echo "  - Coverage: ./scripts/setup_coverage.sh"
echo ""
