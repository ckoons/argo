#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Comprehensive code quality analysis for Argo

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Argo Code Quality Analysis${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 1. Cppcheck - Unused Functions
echo -e "${GREEN}[1/6] Cppcheck: Unused Functions${NC}"
echo "----------------------------------------"
cppcheck --enable=unusedFunction --quiet \
         --suppress=unusedFunction:tests/* \
         --suppress=unusedFunction:ui/* \
         src/ arc/src/ 2>&1 | grep -v "Checking\|done" || echo "✓ No unused functions found"
echo ""

# 2. Cppcheck - All Issues
echo -e "${GREEN}[2/6] Cppcheck: All Issues${NC}"
echo "----------------------------------------"
cppcheck --enable=warning,style,performance,portability \
         --suppress=missingIncludeSystem \
         --suppress=unmatchedSuppression \
         --suppress=unusedFunction:tests/* \
         --inline-suppr \
         --quiet \
         src/ include/ arc/src/ arc/include/ 2>&1 | \
    grep -v "Checking\|done\|nofile" | head -50 || echo "✓ No issues found"
echo ""

# 3. Compiler Warnings (Extra Strict)
echo -e "${GREEN}[3/6] Compiler: Extra Strict Warnings${NC}"
echo "----------------------------------------"
make clean > /dev/null 2>&1
EXTRA_FLAGS="-Wunused -Wunreachable-code -Wshadow -Wconversion -Wdouble-promotion"
make CFLAGS="-Wall -Werror -Wextra -std=c11 -g $EXTRA_FLAGS" 2>&1 | \
    grep -E "warning:|error:" | head -30 || echo "✓ No warnings with strict flags"
echo ""

# 4. Find Dead Code (Custom Analysis)
echo -e "${GREEN}[4/6] Custom: Dead Code Detection${NC}"
echo "----------------------------------------"

# Find static functions that might be unused within their file
echo "Potentially unused static functions:"
for file in src/*.c src/*/*.c arc/src/*.c; do
    if [ -f "$file" ]; then
        # Get static function names
        static_funcs=$(grep -n "^static.*(" "$file" | grep -v "/\*\|//\|typedef" | \
                      sed 's/.*static[^a-z_]*\([a-z_][a-z0-9_]*\).*/\1/' | sort -u)

        for func in $static_funcs; do
            # Count calls to this function (excluding definition)
            calls=$(grep -c "\b$func\s*(" "$file" || echo "0")
            if [ "$calls" -le 1 ]; then
                echo "  $file: $func (possibly unused)"
            fi
        done
    fi
done | head -20 || echo "✓ No obviously dead static functions"
echo ""

# 5. Find Unused Variables
echo -e "${GREEN}[5/6] Custom: Unused Variables${NC}"
echo "----------------------------------------"
# This will be caught by compiler with -Wunused, but let's show it
make clean > /dev/null 2>&1
make CFLAGS="-Wall -Wextra -std=c11 -g -Wunused-variable -Wunused-parameter" 2>&1 | \
    grep "unused variable\|unused parameter" | head -20 || echo "✓ No unused variables"
echo ""

# 6. Code Complexity (Function length)
echo -e "${GREEN}[6/6] Custom: Code Complexity${NC}"
echo "----------------------------------------"
echo "Functions over 100 lines (refactoring recommended):"

for file in src/*.c src/*/*.c arc/src/*.c; do
    if [ -f "$file" ]; then
        awk '
            /^[a-z_][a-z0-9_]* [a-z_][a-z0-9_]*\(/ {
                if (func_name != "") {
                    lines = NR - func_start
                    if (lines > 100) {
                        printf "  %s:%d: %s (%d lines)\n", FILENAME, func_start, func_name, lines
                    }
                }
                match($0, /^[a-z_][a-z0-9_]* ([a-z_][a-z0-9_]*)/, arr)
                func_name = arr[1]
                func_start = NR
            }
            END {
                if (func_name != "") {
                    lines = NR - func_start
                    if (lines > 100) {
                        printf "  %s:%d: %s (%d lines)\n", FILENAME, func_start, func_name, lines
                    }
                }
            }
        ' "$file"
    fi
done | head -20 || echo "✓ No functions over 100 lines"

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Analysis Complete${NC}"
echo -e "${BLUE}========================================${NC}"
