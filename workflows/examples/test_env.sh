#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Test workflow for environment variable passing

echo "=== Workflow Environment Variables Test ==="
echo ""

echo "Custom Environment Variables:"
echo "  API_KEY=${API_KEY:-NOT SET}"
echo "  DEBUG=${DEBUG:-NOT SET}"
echo "  STAGE=${STAGE:-NOT SET}"
echo "  LOG_LEVEL=${LOG_LEVEL:-NOT SET}"
echo ""

echo "Standard Environment Variables:"
echo "  HOME=$HOME"
echo "  USER=$USER"
echo "  PWD=$PWD"
