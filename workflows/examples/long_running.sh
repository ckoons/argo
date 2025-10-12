#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Long-running test workflow for abandon testing

echo "Starting long workflow..."
for i in {1..30}; do
    echo "Iteration $i of 30"
    sleep 1
done
echo "Completed"
