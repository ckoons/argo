#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Test workflow for argument passing

echo "=== Workflow Arguments Test ==="
echo "Number of arguments: $#"
echo ""

if [ $# -eq 0 ]; then
    echo "No arguments provided"
    exit 0
fi

echo "Arguments received:"
for i in $(seq 1 $#); do
    echo "  Arg $i: ${!i}"
done

echo ""
echo "All arguments: $@"
echo "Script name: $0"
