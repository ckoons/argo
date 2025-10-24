#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_requirements_conversation.sh - Test the requirements conversation workflow
#
# Creates a new session and runs the requirements gathering phase

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/lib"
PHASES_DIR="$SCRIPT_DIR/phases"

# Load required libraries
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/logging.sh"

# Test requirements conversation
test_requirements_conversation() {
    local project_name="${1:-calculator}"

    echo ""
    echo "=========================================="
    echo "Testing Requirements Conversation Workflow"
    echo "=========================================="
    echo ""

    # Create new session
    info "Creating new session for project: $project_name"
    local session_id=$(create_session "$project_name")
    success "Created session: $session_id"

    local session_dir=$(get_session_dir "$session_id")
    info "Session directory: $session_dir"
    echo ""

    # Run requirements conversation
    info "Starting requirements conversation..."
    echo ""
    echo "==========================================
"
    echo "REQUIREMENTS CONVERSATION"
    echo "=========================================="
    echo ""
    echo "Instructions:"
    echo "  - Answer the CI's questions naturally"
    echo "  - The CI will guide you through gathering requirements"
    echo "  - When done, the CI will summarize and ask for approval"
    echo ""

    local next_phase=$("$PHASES_DIR/requirements_conversation.sh" "$session_id" "$project_name")

    echo ""
    echo "=========================================="
    echo "Requirements Phase Complete"
    echo "=========================================="
    echo ""
    success "Next phase: $next_phase"

    # Show generated artifacts
    local context_file="$session_dir/context.json"
    local requirements_file=$(jq -r '.requirements.file' "$context_file")

    echo ""
    info "Generated artifacts:"
    echo "  - Context: $context_file"
    if [[ -f "$requirements_file" ]]; then
        echo "  - Requirements: $requirements_file"
        echo ""
        echo "--- requirements.md ---"
        cat "$requirements_file"
        echo ""
    fi

    echo ""
    success "Test complete! Session: $session_id"
    echo ""
    echo "To resume this session:"
    echo "  $PHASES_DIR/requirements_conversation.sh $session_id $project_name"
    echo ""
    echo "To view context:"
    echo "  cat $context_file | jq"
    echo ""
}

# Main
main() {
    if [[ $# -gt 0 ]]; then
        test_requirements_conversation "$1"
    else
        test_requirements_conversation
    fi
}

main "$@"
