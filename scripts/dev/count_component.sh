#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Count meaningful lines in a component and output JSON
# Usage: count_component.sh <component_path> [--budget <lines>]

set -e

# Default budget
DEFAULT_BUDGET=10000

# Parse arguments
COMPONENT_PATH="${1:-.}"
BUDGET=$DEFAULT_BUDGET

shift || true
while [[ $# -gt 0 ]]; do
    case "$1" in
        --budget)
            BUDGET="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# Resolve paths
if [ "$COMPONENT_PATH" = "." ]; then
    COMP_DIR="$(pwd)"
    COMP_NAME="argo"
elif [ "$COMPONENT_PATH" = "arc" ]; then
    COMP_DIR="$(pwd)/arc"
    COMP_NAME="arc"
elif [[ "$COMPONENT_PATH" =~ ^ui/ ]]; then
    COMP_DIR="$(pwd)/$COMPONENT_PATH"
    COMP_NAME="$(basename "$COMPONENT_PATH")"
else
    COMP_DIR="$COMPONENT_PATH"
    COMP_NAME="$(basename "$COMPONENT_PATH")"
fi

# Function to count meaningful lines in a file (diet-aware)
count_meaningful_lines() {
    local file=$1

    cat "$file" | \
        sed 's|/\*.*\*/||g' | \
        sed 's|//.*$||' | \
        grep -v '^ *$' | \
        grep -v '^ *[{}] *$' | \
        grep -v '^ *} *$' | \
        grep -v '^ *{ *$' | \
        grep -v 'LOG_[A-Z]*(' | \
        grep -v 'printf(' | \
        grep -v 'fprintf(' | \
        grep -v 'snprintf.*\.\.\.' | \
        grep -v '^ *; *$' | \
        wc -l | tr -d ' '
}

# Count files in a directory
count_directory() {
    local dir=$1
    local total=0
    local file_count=0

    if [ -d "$dir" ]; then
        # Process .c files (recursively using find)
        while IFS= read -r file; do
            # Skip third-party files
            basename=$(basename "$file")
            if echo "$basename" | grep -qE "(jsmn|cJSON)"; then
                continue
            fi

            meaningful=$(count_meaningful_lines "$file")
            total=$((total + meaningful))
            file_count=$((file_count + 1))
        done < <(find "$dir" -name "*.c" -type f 2>/dev/null)

        # Process .h files (recursively using find)
        while IFS= read -r file; do
            # Skip third-party files
            basename=$(basename "$file")
            if echo "$basename" | grep -qE "(jsmn|cJSON)"; then
                continue
            fi

            meaningful=$(count_meaningful_lines "$file")
            total=$((total + meaningful))
            file_count=$((file_count + 1))
        done < <(find "$dir" -name "*.h" -type f 2>/dev/null)
    fi

    echo "$total $file_count"
}

# Count component
SRC_RESULT=($(count_directory "$COMP_DIR/src"))
INC_RESULT=($(count_directory "$COMP_DIR/include"))
TEST_RESULT=($(count_directory "$COMP_DIR/tests"))

SRC_LINES=${SRC_RESULT[0]:-0}
SRC_FILES=${SRC_RESULT[1]:-0}
INC_LINES=${INC_RESULT[0]:-0}
INC_FILES=${INC_RESULT[1]:-0}
TEST_LINES=${TEST_RESULT[0]:-0}
TEST_FILES=${TEST_RESULT[1]:-0}

# Only src/ counts against budget
BUDGET_LINES=$SRC_LINES
TOTAL_FILES=$((SRC_FILES + INC_FILES))

# Calculate budget status
USED_PERCENT=0
if [ $BUDGET -gt 0 ]; then
    USED_PERCENT=$((BUDGET_LINES * 100 / BUDGET))
fi

REMAINING=$((BUDGET - BUDGET_LINES))

STATUS="healthy"
if [ $BUDGET_LINES -gt $BUDGET ]; then
    STATUS="over_budget"
elif [ $USED_PERCENT -gt 80 ]; then
    STATUS="warning"
fi

# Output JSON
cat <<EOF
{
  "component": "$COMP_NAME",
  "timestamp": $(date +%s),
  "path": "$COMPONENT_PATH",
  "counts": {
    "budget_lines": $BUDGET_LINES,
    "source_files": $TOTAL_FILES,
    "test_lines": $TEST_LINES,
    "test_files": $TEST_FILES
  },
  "budget": {
    "limit": $BUDGET,
    "used": $BUDGET_LINES,
    "remaining": $REMAINING,
    "percentage": $USED_PERCENT,
    "status": "$STATUS"
  },
  "breakdown": {
    "src": {
      "lines": $SRC_LINES,
      "files": $SRC_FILES,
      "counts_against_budget": true
    },
    "include": {
      "lines": $INC_LINES,
      "files": $INC_FILES,
      "counts_against_budget": false
    },
    "tests": {
      "lines": $TEST_LINES,
      "files": $TEST_FILES,
      "counts_against_budget": false
    }
  }
}
EOF
