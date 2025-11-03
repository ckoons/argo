#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# design_program.sh - Initial requirements gathering phase
#
# Queries CI for high-level design based on user requirements
# Transitions from init -> design
# Hands off to CI for design approval

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source library modules
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"

# CI tool command (allow override for testing)
CI_TOOL="${CI_COMMAND:-ci}"

#
# Main function
#
main() {
    local project_path="$1"

    # Validate argument
    if [[ -z "$project_path" ]]; then
        echo "ERROR: Project path required" >&2
        return 1
    fi

    # Validate project directory
    if [[ ! -d "$project_path/.argo-project" ]]; then
        echo "ERROR: .argo-project not found in $project_path" >&2
        return 1
    fi

    cd "$project_path"

    echo "=== design_program phase ==="

    # Read user query from state
    local user_query=$(read_state "user_query")
    if [[ -z "$user_query" ]] || [[ "$user_query" == "null" ]]; then
        echo "ERROR: No user query in state" >&2
        update_state "error_recovery.last_error" "Missing user query"
        return 1
    fi

    echo "User query: $user_query"

    # Build prompt for CI
    local ci_prompt="Based on this requirement, provide a high-level software design:

Requirement: $user_query

Please provide:
1. A brief summary of the approach
2. Main components to be built
3. Key technologies or patterns to use

Keep the design high-level and focused on component decomposition."

    echo "Querying CI for design..."

    # Query CI for initial design
    local design_response=""
    if command -v "$CI_TOOL" >/dev/null 2>&1; then
        design_response=$("$CI_TOOL" "$ci_prompt" 2>&1)
        local ci_result=$?

        if [[ $ci_result -ne 0 ]]; then
            echo "ERROR: CI query failed with exit code $ci_result" >&2
            update_state "error_recovery.last_error" "CI query failed"
            return 1
        fi
    else
        echo "ERROR: CI tool not found: $CI_TOOL" >&2
        update_state "error_recovery.last_error" "CI tool not available"
        return 1
    fi

    echo "Design received from CI"

    # Create timestamp
    local timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

    # Save design to state
    local design_json=$(jq -n \
        --arg summary "$design_response" \
        --arg created "$timestamp" \
        '{
            summary: $summary,
            components: [],
            created: $created,
            approved: false
        }')

    local temp_file=".argo-project/state.json.tmp"
    jq --argjson design "$design_json" '.design = $design' .argo-project/state.json > "$temp_file"

    if [[ $? -eq 0 ]]; then
        mv "$temp_file" .argo-project/state.json
    else
        echo "ERROR: Failed to save design to state" >&2
        rm -f "$temp_file"
        return 1
    fi

    echo "Design saved to state"

    # Log phase transition
    log_state_transition "init" "design"

    # Transition to design phase
    update_state "phase" "design"
    update_state "action_owner" "ci"
    update_state "action_needed" "approve_design"

    echo "design_program phase complete. Awaiting design approval..."
    echo ""

    return 0
}

# Run main function
main "$@"
exit $?
