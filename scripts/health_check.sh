#!/bin/bash
# © 2025 Casey Koons All rights reserved
# CI Health Monitor - Check stale heartbeats, errors, offline CIs

set -e

REGISTRY_FILE="${ARGO_ROOT:-.}/.argo/registry.json"
STALE_THRESHOLD=300  # 5 minutes

if [ ! -f "$REGISTRY_FILE" ]; then
    echo "No registry file found at $REGISTRY_FILE"
    exit 1
fi

NOW=$(date +%s)
HEALTHY=0
DEGRADED=0
OFFLINE=0
ERRORS=0

echo "=========================================="
echo "Argo CI Health Monitor"
echo "=========================================="
echo ""

# Parse registry JSON for CI status
while IFS= read -r line; do
    # Extract CI name, status, last_heartbeat, error_count
    if echo "$line" | grep -q '"name"'; then
        CI_NAME=$(echo "$line" | sed 's/.*"name": *"\([^"]*\)".*/\1/')
    fi
    if echo "$line" | grep -q '"status"'; then
        STATUS=$(echo "$line" | sed 's/.*"status": *\([0-9]*\).*/\1/')
    fi
    if echo "$line" | grep -q '"last_heartbeat"'; then
        HEARTBEAT=$(echo "$line" | sed 's/.*"last_heartbeat": *\([0-9]*\).*/\1/')

        # Calculate age
        AGE=$((NOW - HEARTBEAT))

        # Determine health status
        case $STATUS in
            0) # OFFLINE
                echo "[$CI_NAME] OFFLINE"
                ((OFFLINE++))
                ;;
            1|2) # STARTING/READY
                if [ $AGE -gt $STALE_THRESHOLD ]; then
                    echo "[$CI_NAME] DEGRADED (stale heartbeat: ${AGE}s ago)"
                    ((DEGRADED++))
                else
                    echo "[$CI_NAME] HEALTHY"
                    ((HEALTHY++))
                fi
                ;;
            3) # BUSY
                echo "[$CI_NAME] BUSY"
                ((HEALTHY++))
                ;;
            4) # ERROR
                echo "[$CI_NAME] ERROR"
                ((ERRORS++))
                ;;
            5) # SHUTDOWN
                echo "[$CI_NAME] SHUTTING DOWN"
                ((DEGRADED++))
                ;;
        esac
    fi
done < "$REGISTRY_FILE"

echo ""
echo "=========================================="
echo "Summary:"
echo "  Healthy:  $HEALTHY"
echo "  Degraded: $DEGRADED"
echo "  Offline:  $OFFLINE"
echo "  Errors:   $ERRORS"
echo "=========================================="

# Exit code reflects health
if [ $ERRORS -gt 0 ]; then
    exit 2  # Critical - errors present
elif [ $DEGRADED -gt 0 ]; then
    exit 1  # Warning - degraded CIs
else
    exit 0  # OK
fi
