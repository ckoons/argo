#!/bin/bash
# © 2025 Casey Koons All rights reserved
# CI Health Monitor - Check stale heartbeats, errors, offline CIs

set -e

REGISTRY_FILE="${ARGO_ROOT:-.}/.argo/registry.json"
STALE_THRESHOLD=300  # 5 minutes
JSON_OUTPUT=0

# Parse command line arguments
if [ "$1" = "--json" ]; then
    JSON_OUTPUT=1
fi

if [ ! -f "$REGISTRY_FILE" ]; then
    echo "No registry file found at $REGISTRY_FILE"
    exit 1
fi

NOW=$(date +%s)
HEALTHY=0
DEGRADED=0
OFFLINE=0
ERRORS=0

# Arrays to store CI details for JSON output
declare -a CI_NAMES
declare -a CI_STATUSES
declare -a CI_HEARTBEAT_AGES

if [ $JSON_OUTPUT -eq 0 ]; then
    echo "=========================================="
    echo "Argo CI Health Monitor"
    echo "=========================================="
    echo ""
fi

# Parse registry JSON for CI status
CI_INDEX=0
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
        HEALTH_STATUS=""
        case $STATUS in
            0) # OFFLINE
                HEALTH_STATUS="offline"
                ((OFFLINE++))
                ;;
            1|2) # STARTING/READY
                if [ $AGE -gt $STALE_THRESHOLD ]; then
                    HEALTH_STATUS="degraded"
                    ((DEGRADED++))
                else
                    HEALTH_STATUS="healthy"
                    ((HEALTHY++))
                fi
                ;;
            3) # BUSY
                HEALTH_STATUS="healthy"
                ((HEALTHY++))
                ;;
            4) # ERROR
                HEALTH_STATUS="error"
                ((ERRORS++))
                ;;
            5) # SHUTDOWN
                HEALTH_STATUS="degraded"
                ((DEGRADED++))
                ;;
        esac

        # Store CI details
        CI_NAMES[$CI_INDEX]="$CI_NAME"
        CI_STATUSES[$CI_INDEX]="$HEALTH_STATUS"
        CI_HEARTBEAT_AGES[$CI_INDEX]="$AGE"
        ((CI_INDEX++))

        # Text output
        if [ $JSON_OUTPUT -eq 0 ]; then
            case $HEALTH_STATUS in
                offline)
                    echo "[$CI_NAME] OFFLINE"
                    ;;
                degraded)
                    echo "[$CI_NAME] DEGRADED (stale heartbeat: ${AGE}s ago)"
                    ;;
                healthy)
                    if [ $STATUS -eq 3 ]; then
                        echo "[$CI_NAME] BUSY"
                    else
                        echo "[$CI_NAME] HEALTHY"
                    fi
                    ;;
                error)
                    echo "[$CI_NAME] ERROR"
                    ;;
            esac
        fi
    fi
done < "$REGISTRY_FILE"

# Output results
if [ $JSON_OUTPUT -eq 1 ]; then
    # JSON output
    echo "{"
    echo "  \"timestamp\": $NOW,"
    if [ $ERRORS -gt 0 ]; then
        echo "  \"overall_status\": \"critical\","
    elif [ $DEGRADED -gt 0 ]; then
        echo "  \"overall_status\": \"warning\","
    else
        echo "  \"overall_status\": \"ok\","
    fi
    echo "  \"summary\": {"
    echo "    \"healthy\": $HEALTHY,"
    echo "    \"degraded\": $DEGRADED,"
    echo "    \"offline\": $OFFLINE,"
    echo "    \"errors\": $ERRORS"
    echo "  },"
    echo "  \"cis\": ["
    for i in "${!CI_NAMES[@]}"; do
        echo "    {"
        echo "      \"name\": \"${CI_NAMES[$i]}\","
        echo "      \"status\": \"${CI_STATUSES[$i]}\","
        echo "      \"heartbeat_age_seconds\": ${CI_HEARTBEAT_AGES[$i]}"
        if [ $i -eq $((${#CI_NAMES[@]} - 1)) ]; then
            echo "    }"
        else
            echo "    },"
        fi
    done
    echo "  ]"
    echo "}"
else
    # Text output
    echo ""
    echo "=========================================="
    echo "Summary:"
    echo "  Healthy:  $HEALTHY"
    echo "  Degraded: $DEGRADED"
    echo "  Offline:  $OFFLINE"
    echo "  Errors:   $ERRORS"
    echo "=========================================="
fi

# Exit code reflects health
if [ $ERRORS -gt 0 ]; then
    exit 2  # Critical - errors present
elif [ $DEGRADED -gt 0 ]; then
    exit 1  # Warning - degraded CIs
else
    exit 0  # OK
fi
