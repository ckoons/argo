#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Integration test for interactive chat workflow
# Tests user input via HTTP I/O channel and Claude Code provider

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DAEMON_PORT=9876
DAEMON_URL="http://localhost:$DAEMON_PORT"
WF_NAME="test_interactive_chat_test1"
LOG_DIR="$HOME/.argo/logs"
LOG_FILE="$LOG_DIR/$WF_NAME.log"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo "======================================================================"
echo "INTERACTIVE CHAT WORKFLOW TEST"
echo "======================================================================"
echo ""

# Check daemon
echo -e "${YELLOW}Checking daemon...${NC}"
if ! curl -s "$DAEMON_URL/api/health" > /dev/null 2>&1; then
    echo -e "${RED}Error: Daemon not running on port $DAEMON_PORT${NC}"
    echo "Start with: bin/argo-daemon --port $DAEMON_PORT &"
    exit 1
fi
echo -e "${GREEN}✓ Daemon running${NC}"
echo ""

# Clean up any existing workflow
echo -e "${YELLOW}Cleaning up existing workflow...${NC}"
curl -s -X DELETE "$DAEMON_URL/api/workflow/abandon?workflow_name=$WF_NAME" >/dev/null 2>&1 || true
sleep 1
echo -e "${GREEN}✓ Cleanup complete${NC}"
echo ""

# Start workflow
echo -e "${YELLOW}Starting workflow: $WF_NAME${NC}"
response=$(curl -s -X POST "$DAEMON_URL/api/workflow/start" \
    -H "Content-Type: application/json" \
    -d "{\"template\":\"test_interactive_chat\",\"instance\":\"test1\",\"branch\":\"main\",\"environment\":\"dev\"}" 2>&1)

if ! echo "$response" | grep -q "success"; then
    echo -e "${RED}✗ Failed to start workflow${NC}"
    echo "Response: $response"
    exit 1
fi
echo -e "${GREEN}✓ Workflow started${NC}"
echo ""

# Wait for workflow to be ready
echo -e "${YELLOW}Waiting for workflow to initialize...${NC}"
sleep 2
echo ""

# Function to send user input via HTTP I/O channel
send_input() {
    local input="$1"
    echo -e "${BLUE}Sending input: \"$input\"${NC}"

    response=$(curl -s -X POST "$DAEMON_URL/api/workflow/input/$WF_NAME" \
        -H "Content-Type: application/json" \
        -d "{\"input\":\"$input\"}" 2>&1)

    if echo "$response" | grep -qi "error"; then
        echo -e "${RED}✗ Failed to send input${NC}"
        echo "Response: $response"
        return 1
    fi

    echo -e "${GREEN}✓ Input sent${NC}"
    return 0
}

# Function to read workflow output
read_output() {
    local max_attempts=10
    local attempt=0

    while [ $attempt -lt $max_attempts ]; do
        response=$(curl -s "$DAEMON_URL/api/workflow/output/$WF_NAME" 2>&1)

        if echo "$response" | grep -q "\"output\":"; then
            # Extract output from JSON
            output=$(echo "$response" | grep -o '"output":"[^"]*"' | sed 's/"output":"//;s/"$//' | sed 's/\\n/\n/g')
            if [ -n "$output" ]; then
                echo -e "${GREEN}Output:${NC}"
                echo "$output"
                return 0
            fi
        fi

        sleep 0.5
        attempt=$((attempt + 1))
    done

    echo -e "${YELLOW}No output available yet${NC}"
    return 1
}

# Test sequence
echo "======================================================================"
echo "TEST SEQUENCE"
echo "======================================================================"
echo ""

# Step 1: Read initial output (welcome message)
echo -e "${YELLOW}Step 1: Reading welcome message...${NC}"
sleep 1
read_output
echo ""

# Step 2: Send first question
echo -e "${YELLOW}Step 2: Sending first question...${NC}"
send_input "What is 2+2?"
sleep 1
echo ""

# Step 3: Wait for Claude Code response
echo -e "${YELLOW}Step 3: Waiting for Claude response (this may take a few seconds)...${NC}"
sleep 5

# Check log for Claude response
if [ -f "$LOG_FILE" ]; then
    echo -e "${BLUE}Log file contents:${NC}"
    tail -20 "$LOG_FILE"
    echo ""
fi

# Step 4: Read Claude's response
echo -e "${YELLOW}Step 4: Reading Claude's response...${NC}"
read_output
echo ""

# Step 5: Read continuation prompt
echo -e "${YELLOW}Step 5: Reading continuation prompt...${NC}"
sleep 1
read_output
echo ""

# Step 6: Answer "no" to end chat
echo -e "${YELLOW}Step 6: Ending chat...${NC}"
send_input "no"
sleep 1
echo ""

# Step 7: Read goodbye message
echo -e "${YELLOW}Step 7: Reading goodbye message...${NC}"
sleep 2
read_output
echo ""

# Wait for workflow to complete
echo -e "${YELLOW}Waiting for workflow to complete...${NC}"
sleep 3

# Check log file for completion
echo ""
echo "======================================================================"
echo "LOG FILE CHECK"
echo "======================================================================"
echo ""

if [ -f "$LOG_FILE" ]; then
    echo -e "${GREEN}✓ Log file exists: $LOG_FILE${NC}"
    echo ""

    echo -e "${BLUE}Full log contents:${NC}"
    cat "$LOG_FILE"
    echo ""

    # Check for key indicators
    if grep -q "Workflow completed successfully" "$LOG_FILE"; then
        echo -e "${GREEN}✓ Workflow completed successfully${NC}"
    else
        echo -e "${YELLOW}⚠ Workflow may not have completed (check log above)${NC}"
    fi

    if grep -q "Claude says:" "$LOG_FILE" 2>/dev/null || grep -q "ci_response" "$LOG_FILE" 2>/dev/null; then
        echo -e "${GREEN}✓ Claude response found in log${NC}"
    else
        echo -e "${RED}✗ No Claude response found${NC}"
    fi

    if grep -q "HTTP I/O channel" "$LOG_FILE"; then
        echo -e "${GREEN}✓ HTTP I/O channel created${NC}"
    else
        echo -e "${RED}✗ HTTP I/O channel not created${NC}"
    fi
else
    echo -e "${RED}✗ Log file not found: $LOG_FILE${NC}"
fi

echo ""
echo "======================================================================"
echo "WORKFLOW STATUS CHECK"
echo "======================================================================"
echo ""

status=$(curl -s "$DAEMON_URL/api/workflow/status?workflow_name=$WF_NAME" 2>&1)
echo -e "${BLUE}Status API response:${NC}"
echo "$status"
echo ""

# Cleanup
echo "======================================================================"
echo "CLEANUP"
echo "======================================================================"
echo ""
echo -e "${YELLOW}Cleaning up workflow...${NC}"
curl -s -X DELETE "$DAEMON_URL/api/workflow/abandon?workflow_name=$WF_NAME" >/dev/null 2>&1 || true
echo -e "${GREEN}✓ Cleanup complete${NC}"
echo ""

echo "======================================================================"
echo "TEST COMPLETE"
echo "======================================================================"
echo ""
echo -e "${GREEN}Interactive chat test completed!${NC}"
echo ""
echo "To run this manually:"
echo "  1. Start daemon: bin/argo-daemon --port 9876 &"
echo "  2. Start workflow: curl -X POST $DAEMON_URL/api/workflow/start -H 'Content-Type: application/json' -d '{\"template\":\"test_interactive_chat\",\"instance\":\"manual\",\"branch\":\"main\",\"environment\":\"dev\"}'"
echo "  3. Send input: curl -X POST $DAEMON_URL/api/workflow/input/test_interactive_chat_manual -H 'Content-Type: application/json' -d '{\"input\":\"your question\"}'"
echo "  4. Read output: curl $DAEMON_URL/api/workflow/output/test_interactive_chat_manual"
echo "  5. View log: tail -f ~/.argo/logs/test_interactive_chat_manual.log"
echo ""
