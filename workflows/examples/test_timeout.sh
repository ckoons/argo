#!/bin/bash
# Test workflow timeout - sleeps for 10 seconds
# Should be killed if timeout is set to less than 10 seconds

echo "Starting long-running workflow..."
echo "Current time: $(date)"

for i in {1..10}; do
    echo "Progress: $i/10"
    sleep 1
done

echo "Workflow completed successfully"
echo "End time: $(date)"
