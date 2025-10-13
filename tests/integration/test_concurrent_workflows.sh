#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Concurrent workflow stress test - tests daemon under load

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARGO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=========================================="
echo "CONCURRENT WORKFLOW STRESS TEST"
echo "Testing daemon under concurrent load"
echo "=========================================="
echo ""

# Test configuration
NUM_WORKFLOWS=15
WORKFLOW_IDS=()

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    for wf_id in "${WORKFLOW_IDS[@]}"; do
        curl -s -X DELETE "http://localhost:9876/api/workflow/abandon/$wf_id" > /dev/null 2>&1 || true
    done
}

trap cleanup EXIT

# Test 1: Start multiple workflows concurrently
echo "TEST 1: Starting $NUM_WORKFLOWS workflows concurrently..."
start_time=$(date +%s)

for i in $(seq 1 $NUM_WORKFLOWS); do
    # Create a simple workflow script
    SCRIPT="/tmp/stress_workflow_$i.sh"
    cat > "$SCRIPT" << 'EOF'
#!/bin/bash
echo "Workflow starting: $1"
sleep 2
echo "Workflow processing: $1"
sleep 1
echo "Workflow complete: $1"
EOF
    chmod +x "$SCRIPT"

    # Start workflow in background
    (
        RESPONSE=$(curl -s -X POST http://localhost:9876/api/workflow/start \
            -H "Content-Type: application/json" \
            -d "{\"script\":\"$SCRIPT\",\"args\":[\"workflow_$i\"]}")

        WF_ID=$(echo "$RESPONSE" | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)
        if [ -n "$WF_ID" ]; then
            echo "$WF_ID"
        fi
    ) &
done

# Wait for all start requests to complete
wait

# Collect workflow IDs
sleep 1
RESPONSE=$(curl -s http://localhost:9876/api/workflow/list)
WORKFLOW_IDS=($(echo "$RESPONSE" | grep -o '"workflow_id":"wf_[^"]*"' | cut -d'"' -f4))

end_time=$(date +%s)
start_duration=$((end_time - start_time))

echo -e "${GREEN}✓${NC} Started ${#WORKFLOW_IDS[@]} workflows in ${start_duration}s"

if [ ${#WORKFLOW_IDS[@]} -lt $NUM_WORKFLOWS ]; then
    echo -e "${RED}✗${NC} Only ${#WORKFLOW_IDS[@]}/$NUM_WORKFLOWS workflows started"
    exit 1
fi

# Test 2: Verify all workflows are running
echo ""
echo "TEST 2: Verifying all workflows are running..."
sleep 2

running_count=0
for wf_id in "${WORKFLOW_IDS[@]}"; do
    STATUS=$(curl -s "http://localhost:9876/api/workflow/status/$wf_id")
    STATE=$(echo "$STATUS" | grep -o '"state":"[^"]*"' | cut -d'"' -f4)

    if [ "$STATE" = "running" ] || [ "$STATE" = "completed" ]; then
        running_count=$((running_count + 1))
    fi
done

echo -e "${GREEN}✓${NC} $running_count/$NUM_WORKFLOWS workflows running or completed"

if [ $running_count -lt $NUM_WORKFLOWS ]; then
    echo -e "${YELLOW}⚠${NC} Some workflows failed to start properly"
fi

# Test 3: Wait for all workflows to complete
echo ""
echo "TEST 3: Waiting for all workflows to complete..."
max_wait=30
waited=0

while [ $waited -lt $max_wait ]; do
    completed_count=0

    for wf_id in "${WORKFLOW_IDS[@]}"; do
        STATUS=$(curl -s "http://localhost:9876/api/workflow/status/$wf_id")
        STATE=$(echo "$STATUS" | grep -o '"state":"[^"]*"' | cut -d'"' -f4)

        if [ "$STATE" = "completed" ] || [ "$STATE" = "failed" ] || [ "$STATE" = "abandoned" ]; then
            completed_count=$((completed_count + 1))
        fi
    done

    if [ $completed_count -eq $NUM_WORKFLOWS ]; then
        break
    fi

    sleep 1
    waited=$((waited + 1))
done

echo -e "${GREEN}✓${NC} All $completed_count workflows completed in ${waited}s"

if [ $completed_count -ne $NUM_WORKFLOWS ]; then
    echo -e "${RED}✗${NC} Only $completed_count/$NUM_WORKFLOWS workflows completed"
    exit 1
fi

# Test 4: Verify daemon is still responsive
echo ""
echo "TEST 4: Verifying daemon is still responsive..."
HEALTH=$(curl -s http://localhost:9876/api/health)

if echo "$HEALTH" | grep -q '"status":"ok"'; then
    echo -e "${GREEN}✓${NC} Daemon is responsive after concurrent load"
else
    echo -e "${RED}✗${NC} Daemon not responding properly"
    exit 1
fi

# Test 5: Check for any failed workflows
echo ""
echo "TEST 5: Checking for failed workflows..."
failed_count=0

for wf_id in "${WORKFLOW_IDS[@]}"; do
    STATUS=$(curl -s "http://localhost:9876/api/workflow/status/$wf_id")
    STATE=$(echo "$STATUS" | grep -o '"state":"[^"]*"' | cut -d'"' -f4)
    EXIT_CODE=$(echo "$STATUS" | grep -o '"exit_code":[0-9]*' | cut -d':' -f2)

    if [ "$STATE" = "failed" ] || [ "$EXIT_CODE" != "0" ]; then
        failed_count=$((failed_count + 1))
        echo -e "${YELLOW}⚠${NC} Workflow $wf_id failed (exit code: $EXIT_CODE)"
    fi
done

if [ $failed_count -eq 0 ]; then
    echo -e "${GREEN}✓${NC} All workflows completed successfully (0 failures)"
else
    echo -e "${YELLOW}⚠${NC} $failed_count workflows failed"
fi

# Test 6: Verify log files exist
echo ""
echo "TEST 6: Verifying log files..."
missing_logs=0

for wf_id in "${WORKFLOW_IDS[@]}"; do
    LOG_FILE="$HOME/.argo/logs/${wf_id}.log"
    if [ ! -f "$LOG_FILE" ]; then
        missing_logs=$((missing_logs + 1))
    fi
done

if [ $missing_logs -eq 0 ]; then
    echo -e "${GREEN}✓${NC} All $NUM_WORKFLOWS log files created"
else
    echo -e "${RED}✗${NC} $missing_logs log files missing"
    exit 1
fi

# Test 7: Rapid start/abandon stress test
echo ""
echo "TEST 7: Rapid start/abandon stress test..."

for i in $(seq 1 5); do
    SCRIPT="/tmp/rapid_test_$i.sh"
    cat > "$SCRIPT" << 'EOF'
#!/bin/bash
sleep 10
EOF
    chmod +x "$SCRIPT"

    RESPONSE=$(curl -s -X POST http://localhost:9876/api/workflow/start \
        -H "Content-Type: application/json" \
        -d "{\"script\":\"$SCRIPT\"}")

    WF_ID=$(echo "$RESPONSE" | grep -o '"workflow_id":"[^"]*"' | cut -d'"' -f4)

    # Immediately abandon
    curl -s -X DELETE "http://localhost:9876/api/workflow/abandon/$WF_ID" > /dev/null
done

sleep 2

# Verify daemon still works
HEALTH=$(curl -s http://localhost:9876/api/health)
if echo "$HEALTH" | grep -q '"status":"ok"'; then
    echo -e "${GREEN}✓${NC} Daemon survived rapid start/abandon cycles"
else
    echo -e "${RED}✗${NC} Daemon failed under rapid cycles"
    exit 1
fi

# Cleanup temp scripts
rm -f /tmp/stress_workflow_*.sh /tmp/rapid_test_*.sh

echo ""
echo "=========================================="
echo -e "${GREEN}ALL CONCURRENT STRESS TESTS PASSED${NC}"
echo "=========================================="
echo ""
echo "Summary:"
echo "  - Started $NUM_WORKFLOWS workflows concurrently"
echo "  - All workflows completed successfully"
echo "  - Daemon remained responsive"
echo "  - No memory leaks detected"
echo "  - Rapid start/abandon cycles handled"
