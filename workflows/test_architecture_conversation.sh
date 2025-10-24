#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_architecture_conversation.sh - Test the architecture conversation workflow
#
# Creates a session with completed requirements and runs architecture phase

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/lib"
PHASES_DIR="$SCRIPT_DIR/phases"
TOOLS_DIR="$SCRIPT_DIR/tools"

# Load required libraries
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/context-manager.sh"
source "$LIB_DIR/logging.sh"

# Test architecture conversation
test_architecture_conversation() {
    local project_name="${1:-calculator}"

    echo ""
    echo "=========================================="
    echo "Testing Architecture Conversation Workflow"
    echo "=========================================="
    echo ""

    # Create new session
    info "Creating new session for project: $project_name"
    local session_id=$(create_session "$project_name")
    success "Created session: $session_id"

    local session_dir=$(get_session_dir "$session_id")
    local context_file=$(get_context_file "$session_id")
    info "Session directory: $session_dir"
    echo ""

    # Create initial context
    info "Setting up initial context with requirements..."
    create_initial_context "$session_id" "$project_name"

    # Populate with sample requirements (simulating completed requirements phase)
    local temp_file=$(mktemp)
    jq '.requirements.platform = "web" |
        .requirements.features = ["addition", "subtraction", "multiplication", "division"] |
        .requirements.constraints = {"number_type": "decimal", "target_users": "students"} |
        .requirements.converged = true |
        .requirements.validated = true |
        .current_phase = "architecture"' \
       "$context_file" > "$temp_file"
    mv "$temp_file" "$context_file"

    # Generate requirements.md
    "$TOOLS_DIR/generate_requirements_md" "$context_file" > /dev/null

    success "Requirements set up (simulated completed requirements phase)"
    echo ""
    echo "Requirements:"
    echo "  - Platform: web"
    echo "  - Features: addition, subtraction, multiplication, division"
    echo "  - Constraints: decimal numbers, target users = students"
    echo ""

    # Run architecture conversation
    info "Starting architecture conversation..."
    echo ""
    echo "=========================================="
    echo "ARCHITECTURE CONVERSATION"
    echo "=========================================="
    echo ""
    echo "Instructions:"
    echo "  - Discuss component breakdown, technology choices"
    echo "  - The CI will ask questions about architecture"
    echo "  - Try mentioning something not in requirements to test hopping"
    echo "  - When done, CI will summarize and ask for approval"
    echo ""

    local next_phase=$("$PHASES_DIR/architecture_conversation.sh" "$session_id")

    echo ""
    echo "=========================================="
    echo "Architecture Phase Complete"
    echo "=========================================="
    echo ""
    success "Next phase: $next_phase"

    # Show generated artifacts
    local architecture_file=$(jq -r '.architecture.file' "$context_file")
    local tasks_file=$(jq -r '.architecture.tasks_file' "$context_file")

    echo ""
    info "Generated artifacts:"
    echo "  - Context: $context_file"
    if [[ -f "$architecture_file" ]]; then
        echo "  - Architecture: $architecture_file"
        echo ""
        echo "--- architecture.md ---"
        cat "$architecture_file"
        echo ""
    fi

    if [[ -f "$tasks_file" ]]; then
        echo "  - Tasks: $tasks_file"
        echo ""
        echo "--- tasks.json (summary) ---"
        jq -r '.tasks[] | "[\(.status)] \(.name)"' "$tasks_file"
        echo ""
    fi

    echo ""
    success "Test complete! Session: $session_id"
    echo ""
    echo "To resume this session:"
    echo "  $PHASES_DIR/architecture_conversation.sh $session_id"
    echo ""
    echo "To run full workflow:"
    echo "  $SCRIPT_DIR/develop_project.sh $session_id $project_name"
    echo ""
    echo "To view context:"
    echo "  cat $context_file | jq"
    echo ""
}

# Main
main() {
    if [[ $# -gt 0 ]]; then
        test_architecture_conversation "$1"
    else
        test_architecture_conversation
    fi
}

main "$@"
