#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Test workflow that fails

echo "Starting workflow that will fail..."
echo "Step 1: OK"
echo "Step 2: ERROR - simulating failure"
exit 42
