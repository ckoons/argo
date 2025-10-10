#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Display summary table of all component sizes

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Component definitions: path:budget
COMPONENTS=(
    ".:16000"           # argo core
    "arc:5000"          # arc CLI
    "ui/argo-term:3000" # terminal UI
)

echo ""
echo "====================================================================="
echo "ARGO PROJECT SIZE SUMMARY"
echo "====================================================================="
echo ""
printf "%-20s %10s %10s %10s  %s\n" "Component" "Lines" "Budget" "Used%" "Status"
echo "---------------------------------------------------------------------"

TOTAL_LINES=0
TOTAL_BUDGET=0

for comp in "${COMPONENTS[@]}"; do
    IFS=':' read -r path budget <<< "$comp"

    # Get component name
    if [ "$path" = "." ]; then
        name="argo"
    else
        name="$(basename "$path")"
    fi

    # Check if component exists
    if [ ! -d "$PROJECT_ROOT/$path/src" ] && [ "$path" != "." ]; then
        # Component doesn't exist yet
        printf "%-20s %10s %10s %10s  %s\n" "$name" "-" "$budget" "-" "not built"
        TOTAL_BUDGET=$((TOTAL_BUDGET + budget))
        continue
    fi

    # Count component
    json=$("$SCRIPT_DIR/count_component.sh" "$path" --budget "$budget" 2>/dev/null)

    lines=$(echo "$json" | grep '"used"' | sed 's/.*: *\([0-9]*\).*/\1/')
    percent=$(echo "$json" | grep '"percentage"' | sed 's/.*: *\([0-9]*\).*/\1/')
    status=$(echo "$json" | grep '"status"' | sed 's/.*: *"\([^"]*\)".*/\1/')

    # Format status symbol
    case "$status" in
        healthy)
            status_sym="✓ Healthy"
            ;;
        warning)
            status_sym="⚠ Warning"
            ;;
        over_budget)
            status_sym="✗ Over"
            ;;
        *)
            status_sym="?"
            ;;
    esac

    printf "%-20s %10d %10d %9d%%  %s\n" "$name" "$lines" "$budget" "$percent" "$status_sym"

    TOTAL_LINES=$((TOTAL_LINES + lines))
    TOTAL_BUDGET=$((TOTAL_BUDGET + budget))
done

echo "---------------------------------------------------------------------"

if [ $TOTAL_BUDGET -gt 0 ]; then
    TOTAL_PERCENT=$((TOTAL_LINES * 100 / TOTAL_BUDGET))
    printf "%-20s %10d %10d %9d%%\n" "TOTAL" "$TOTAL_LINES" "$TOTAL_BUDGET" "$TOTAL_PERCENT"
fi

echo "====================================================================="
echo ""
