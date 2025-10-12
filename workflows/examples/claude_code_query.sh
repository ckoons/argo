#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Claude Code integration test workflow

echo "=== Claude Code Query Workflow ==="
echo "Testing Claude Code provider integration"
echo ""

# Check if claude command is available
if ! command -v claude &> /dev/null; then
    echo "WARNING: claude command not found in PATH"
    echo "This test requires Claude CLI to be installed"
    echo "Install from: https://github.com/anthropics/anthropic-sdk-python"
    exit 1
fi

echo "✓ Claude Code CLI found"
echo ""

# Simple query to Claude
echo "Sending query to Claude Code..."
echo ""

# Ask a simple question
RESPONSE=$(echo "What is 2+2? Please answer with just the number." | claude 2>&1)
EXIT_CODE=$?

echo "Claude Response:"
echo "$RESPONSE"
echo ""

if [ $EXIT_CODE -eq 0 ]; then
    echo "✓ Claude Code query successful"

    # Check if response contains "4"
    if echo "$RESPONSE" | grep -q "4"; then
        echo "✓ Claude Code response correct"
        exit 0
    else
        echo "✗ Claude Code response unexpected"
        exit 1
    fi
else
    echo "✗ Claude Code query failed with exit code: $EXIT_CODE"
    exit $EXIT_CODE
fi
