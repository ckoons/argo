#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# logging.sh - Logging functions for workflow scripts
#
# Provides consistent logging across all workflow phases

# Source arc-colors for the actual implementations
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/arc-colors.sh"

# Export logging functions
export -f info
export -f error
export -f success
export -f warning
export -f log_phase
export -f step
