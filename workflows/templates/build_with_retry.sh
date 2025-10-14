#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Build workflow template with retry logic

set -e  # Exit on error

echo "=== Build Workflow with Retry ==="

# Retry logic implemented in bash
retry_count=0
max_retries=3

while [ $retry_count -lt $max_retries ]; do
    echo "Build attempt $((retry_count + 1))/$max_retries..."

    if make clean && make; then
        echo "Build succeeded!"
        break
    fi

    retry_count=$((retry_count + 1))

    if [ $retry_count -lt $max_retries ]; then
        sleep_time=$((5 * (2 ** (retry_count - 1))))  # Exponential backoff
        echo "Build failed, retrying in ${sleep_time} seconds..."
        sleep $sleep_time
    else
        echo "Build failed after $max_retries attempts"
        exit 1
    fi
done

echo "Build workflow completed successfully"
