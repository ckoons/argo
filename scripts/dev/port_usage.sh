#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Port Usage Report - Show which ports are allocated to which CIs

set -e

REGISTRY_FILE="${ARGO_ROOT:-.}/.argo/registry.json"

if [ ! -f "$REGISTRY_FILE" ]; then
    echo "No registry file found at $REGISTRY_FILE"
    exit 1
fi

echo "=========================================="
echo "Argo Port Usage Report"
echo "=========================================="
echo ""
echo "Port    CI Name            Role          Status"
echo "-------------------------------------------"

# Extract and sort by port
grep -E '"port"|"name"|"role"|"status"' "$REGISTRY_FILE" | \
paste - - - - | \
sed 's/"port": *\([0-9]*\).*"name": *"\([^"]*\)".*"role": *"\([^"]*\)".*"status": *\([0-9]*\)/\1 \2 \3 \4/' | \
sort -n | \
while read port name role status; do
    case $status in
        0) status_str="OFFLINE" ;;
        1) status_str="STARTING" ;;
        2) status_str="READY" ;;
        3) status_str="BUSY" ;;
        4) status_str="ERROR" ;;
        5) status_str="SHUTDOWN" ;;
        *) status_str="UNKNOWN" ;;
    esac
    printf "%-7s %-18s %-13s %s\n" "$port" "$name" "$role" "$status_str"
done

echo ""
echo "Port Ranges:"
echo "  Builder:       5000-5009"
echo "  Coordinator:   5010-5019"
echo "  Requirements:  5020-5029"
echo "  Analysis:      5030-5039"
echo "  Reserved:      5040-5049"
echo "=========================================="
