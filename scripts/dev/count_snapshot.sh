#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Save component size snapshots for historical tracking
# Usage: count_snapshot.sh [date]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
METRICS_DIR="$PROJECT_ROOT/.argo/metrics"
HISTORY_DIR="$METRICS_DIR/history"

# Create directories if needed
mkdir -p "$HISTORY_DIR"

# Use provided date or current date
DATE="${1:-$(date +%Y-%m-%d)}"
TIMESTAMP=$(date +%s)

# Component definitions
COMPONENTS=(
    ".:16000:argo"
    "arc:5000:arc"
    "ui/argo-term:3000:argo-term"
)

# Save individual component snapshots
for comp in "${COMPONENTS[@]}"; do
    IFS=':' read -r path budget name <<< "$comp"

    # Skip if doesn't exist
    if [ ! -d "$PROJECT_ROOT/$path/src" ] && [ "$path" != "." ]; then
        continue
    fi

    # Generate snapshot
    "$SCRIPT_DIR/count_component.sh" "$path" --budget "$budget" > "$HISTORY_DIR/${DATE}-${name}.json"
    echo "Saved snapshot: ${DATE}-${name}.json"
done

# Create combined snapshot with all components
echo "{" > "$HISTORY_DIR/${DATE}-combined.json"
echo "  \"date\": \"$DATE\"," >> "$HISTORY_DIR/${DATE}-combined.json"
echo "  \"timestamp\": $TIMESTAMP," >> "$HISTORY_DIR/${DATE}-combined.json"
echo "  \"components\": [" >> "$HISTORY_DIR/${DATE}-combined.json"

FIRST=1
for comp in "${COMPONENTS[@]}"; do
    IFS=':' read -r path budget name <<< "$comp"

    # Skip if doesn't exist
    if [ ! -d "$PROJECT_ROOT/$path/src" ] && [ "$path" != "." ]; then
        continue
    fi

    if [ $FIRST -eq 0 ]; then
        echo "," >> "$HISTORY_DIR/${DATE}-combined.json"
    fi
    FIRST=0

    "$SCRIPT_DIR/count_component.sh" "$path" --budget "$budget" | sed 's/^/    /' >> "$HISTORY_DIR/${DATE}-combined.json"
done

echo "" >> "$HISTORY_DIR/${DATE}-combined.json"
echo "  ]" >> "$HISTORY_DIR/${DATE}-combined.json"
echo "}" >> "$HISTORY_DIR/${DATE}-combined.json"

echo "Saved combined snapshot: ${DATE}-combined.json"

# Update latest.json symlink
ln -sf "history/${DATE}-combined.json" "$METRICS_DIR/latest.json"
echo "Updated latest.json -> history/${DATE}-combined.json"
