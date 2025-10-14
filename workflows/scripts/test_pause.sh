#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Test workflow for pause/resume functionality

set -e

echo "Starting test workflow..."
echo "Step 1: Initial setup"
sleep 2

echo "Step 2: Processing (this can be paused)"
for i in {1..20}; do
    echo "  Processing item $i/20..."
    sleep 1
done

echo "Step 3: Cleanup"
sleep 1

echo "Test workflow completed successfully!"
