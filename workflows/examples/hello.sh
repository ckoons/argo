#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Simple test workflow script

echo "=== Argo Bash Workflow Test ==="
echo "Workflow started at: $(date)"
echo ""

echo "Step 1: Environment check"
echo "  - User: $(whoami)"
echo "  - Home: $HOME"
echo "  - PWD: $(pwd)"
echo ""

echo "Step 2: Simulating work..."
for i in 1 2 3; do
    echo "  - Processing iteration $i"
    sleep 1
done
echo ""

echo "Step 3: Completion"
echo "Workflow completed successfully at: $(date)"
echo "=== End Workflow ==="

exit 0
