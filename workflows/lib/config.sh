#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# config.sh - Configuration file reading utilities
#
# Provides functions for reading project configuration from config.json
# Supports fallbacks to environment variables

# Default config file path
readonly CONFIG_FILE="${CONFIG_FILE:-.argo-project/config.json}"

#
# read_config - Read a value from config.json
#
# Usage:
#   value=$(read_config "ci.tool")
#   value=$(read_config "ci.tool" "default_value")
#
# Arguments:
#   $1 - JSON path (dot notation, e.g., "ci.tool")
#   $2 - Default value if not found (optional)
#
# Returns:
#   The value from config, or default if not found
#
read_config() {
    local key="$1"
    local default="${2:-}"

    # Check if config file exists
    if [[ ! -f "$CONFIG_FILE" ]]; then
        echo "$default"
        return 0
    fi

    # Read value using jq
    local value
    value=$(jq -r ".$key // empty" "$CONFIG_FILE" 2>/dev/null)

    # Return value or default
    if [[ -n "$value" ]] && [[ "$value" != "null" ]]; then
        echo "$value"
    else
        echo "$default"
    fi
}

#
# get_ci_tool - Get CI tool command with fallback chain
#
# Priority (highest to lowest):
#   1. CI_COMMAND environment variable
#   2. config.json ci.tool field
#   3. Default "ci" command
#
# Returns:
#   The CI tool command to use
#
get_ci_tool() {
    # Check environment variable first (highest priority)
    if [[ -n "${CI_COMMAND:-}" ]]; then
        echo "$CI_COMMAND"
        return 0
    fi

    # Check config file
    local config_tool
    config_tool=$(read_config "ci.tool")

    if [[ -n "$config_tool" ]] && [[ "$config_tool" != "null" ]]; then
        echo "$config_tool"
        return 0
    fi

    # Default fallback
    echo "ci"
}

#
# update_config - Update a value in config.json
#
# Usage:
#   update_config "ci.tool" "/path/to/ci"
#
# Arguments:
#   $1 - JSON path (dot notation)
#   $2 - New value
#
# Returns:
#   0 on success, 1 on failure
#
update_config() {
    local key="$1"
    local value="$2"

    if [[ ! -f "$CONFIG_FILE" ]]; then
        echo "ERROR: Config file not found: $CONFIG_FILE" >&2
        return 1
    fi

    # Update using jq (atomic write via temp file)
    local temp_file="${CONFIG_FILE}.tmp"
    if jq --arg v "$value" "setpath([\"${key//./\",\"}\".split(\",\")]; \$v)" \
          "$CONFIG_FILE" > "$temp_file" 2>/dev/null; then
        mv "$temp_file" "$CONFIG_FILE"
        return 0
    else
        rm -f "$temp_file"
        echo "ERROR: Failed to update config" >&2
        return 1
    fi
}
