#!/bin/bash
# Test workflow retry - fails the first 2 times, succeeds on 3rd attempt

RETRY_FILE="/tmp/argo_retry_test_count"

# Initialize or increment retry counter
if [ ! -f "$RETRY_FILE" ]; then
    echo "0" > "$RETRY_FILE"
fi

ATTEMPT=$(cat "$RETRY_FILE")
ATTEMPT=$((ATTEMPT + 1))
echo "$ATTEMPT" > "$RETRY_FILE"

echo "=== Retry Test Workflow ==="
echo "Attempt number: $ATTEMPT"
echo "Timestamp: $(date)"

if [ "$ATTEMPT" -lt 3 ]; then
    echo "FAIL: Attempt $ATTEMPT - Simulating failure"
    exit 1
else
    echo "SUCCESS: Attempt $ATTEMPT - Workflow succeeded!"
    rm -f "$RETRY_FILE"  # Clean up for next test
    exit 0
fi
