#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Detailed component analysis with file breakdown
# Usage: count_report.sh [--json]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

JSON_MODE=0
if [ "$1" = "--json" ]; then
    JSON_MODE=1
fi

# Component definitions: path:budget
COMPONENTS=(
    ".:10000"
    "arc:5000"
    "ui/argo-term:3000"
)

# JSON output mode
if [ $JSON_MODE -eq 1 ]; then
    echo "{"
    echo '  "timestamp": '$(date +%s)','
    echo '  "components": ['

    FIRST=1
    for comp in "${COMPONENTS[@]}"; do
        IFS=':' read -r path budget <<< "$comp"

        # Skip if doesn't exist
        if [ ! -d "$PROJECT_ROOT/$path/src" ] && [ "$path" != "." ]; then
            continue
        fi

        if [ $FIRST -eq 0 ]; then
            echo ","
        fi
        FIRST=0

        "$SCRIPT_DIR/count_component.sh" "$path" --budget "$budget" | sed 's/^/    /'
    done

    echo ""
    echo "  ]"
    echo "}"
    exit 0
fi

# Text output mode
echo ""
echo "====================================================================="
echo "ARGO COMPONENT ANALYSIS"
echo "====================================================================="

for comp in "${COMPONENTS[@]}"; do
    IFS=':' read -r path budget <<< "$comp"

    # Get component name
    if [ "$path" = "." ]; then
        name="argo"
        comp_dir="$PROJECT_ROOT"
    else
        name="$(basename "$path")"
        comp_dir="$PROJECT_ROOT/$path"
    fi

    # Check if component exists
    if [ ! -d "$comp_dir/src" ]; then
        echo ""
        echo "$name/"
        echo "  Not yet built"
        continue
    fi

    # Get component data
    json=$("$SCRIPT_DIR/count_component.sh" "$path" --budget "$budget" 2>/dev/null)

    budget_lines=$(echo "$json" | grep '"budget_lines"' | sed 's/.*: *\([0-9]*\).*/\1/')
    used=$(echo "$json" | grep '"used"' | sed 's/.*: *\([0-9]*\).*/\1/')
    percent=$(echo "$json" | grep '"percentage"' | sed 's/.*: *\([0-9]*\).*/\1/')
    src_lines=$(echo "$json" | grep -A3 '"src"' | grep '"lines"' | sed 's/.*: *\([0-9]*\).*/\1/')
    inc_lines=$(echo "$json" | grep -A3 '"include"' | grep '"lines"' | sed 's/.*: *\([0-9]*\).*/\1/')
    test_lines=$(echo "$json" | grep '"test_lines"' | sed 's/.*: *\([0-9]*\).*/\1/')

    echo ""
    echo "$name/ (Budget: $budget lines, ${percent}% used)"
    echo "  Source:   $src_lines lines (counts against budget)"
    echo "  Headers:  $inc_lines lines (info only)"
    echo "  Tests:    $test_lines lines (info only)"

    # Show top 5 largest files
    if [ -d "$comp_dir/src" ]; then
        echo ""
        echo "  Largest files:"
        find "$comp_dir/src" -name "*.c" ! -name "*jsmn*" ! -name "*cJSON*" -exec wc -l {} + 2>/dev/null | \
            sort -rn | head -6 | tail -5 | while read lines file; do
            basename=$(basename "$file")
            printf "    %-40s %5d lines\n" "$basename" "$lines"
        done
    fi
done

# Deployment scenarios
echo ""
echo "====================================================================="
echo "DEPLOYMENT SCENARIOS"
echo "====================================================================="

# Calculate totals for different combinations
ARGO_JSON=$("$SCRIPT_DIR/count_component.sh" "." --budget 10000 2>/dev/null)
ARGO_LINES=$(echo "$ARGO_JSON" | grep '"budget_lines"' | sed 's/.*: *\([0-9]*\).*/\1/')

ARC_LINES=0
if [ -d "$PROJECT_ROOT/arc/src" ]; then
    ARC_JSON=$("$SCRIPT_DIR/count_component.sh" "arc" --budget 5000 2>/dev/null)
    ARC_LINES=$(echo "$ARC_JSON" | grep '"budget_lines"' | sed 's/.*: *\([0-9]*\).*/\1/')
fi

TERM_LINES=0
if [ -d "$PROJECT_ROOT/ui/argo-term/src" ]; then
    TERM_JSON=$("$SCRIPT_DIR/count_component.sh" "ui/argo-term" --budget 3000 2>/dev/null)
    TERM_LINES=$(echo "$TERM_JSON" | grep '"budget_lines"' | sed 's/.*: *\([0-9]*\).*/\1/')
fi

echo ""
printf "  %-30s %10d lines\n" "Minimal (argo only):" "$ARGO_LINES"

if [ $ARC_LINES -gt 0 ]; then
    CLI_TOTAL=$((ARGO_LINES + ARC_LINES))
    printf "  %-30s %10d lines\n" "CLI (argo + arc):" "$CLI_TOTAL"
fi

if [ $TERM_LINES -gt 0 ]; then
    FULL_TOTAL=$((ARGO_LINES + ARC_LINES + TERM_LINES))
    printf "  %-30s %10d lines\n" "Full (argo + arc + arc-term):" "$FULL_TOTAL"
fi

echo ""
echo "====================================================================="
echo ""
