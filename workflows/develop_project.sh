#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# develop_project.sh - Meta-workflow orchestrator
#
# Orchestrates the full development workflow: requirements → architecture → build → review
# Demonstrates phase hopping and conversational workflow navigation

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/lib"
PHASES_DIR="$SCRIPT_DIR/phases"

# Load required libraries
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/context-manager.sh"
source "$LIB_DIR/logging.sh"

# Meta-workflow orchestrator
# Args: session_id, project_name
develop_project() {
    local session_id="$1"
    local project_name="${2:-unnamed_project}"

    echo ""
    echo "=========================================="
    echo "Argo Development Workflow"
    echo "=========================================="
    echo ""
    echo "Project: $project_name"
    echo "Session: $session_id"
    echo ""

    # Get or create session
    local session_dir=$(get_session_dir "$session_id")
    local context_file=$(get_context_file "$session_id")

    if [[ ! -f "$context_file" ]]; then
        info "Creating new session for $project_name"
        create_initial_context "$session_id" "$project_name"
        local current_phase="requirements"
    else
        info "Resuming session $session_id"
        local current_phase=$(get_context_value "$session_id" "current_phase")
        if [[ -z "$current_phase" ]] || [[ "$current_phase" == "null" ]]; then
            current_phase="requirements"
        fi
    fi

    echo ""
    info "Session directory: $session_dir"
    info "Starting phase: $current_phase"
    echo ""

    # Main phase loop - navigate through workflow
    while true; do
        case "$current_phase" in
            requirements)
                echo ""
                echo "=========================================="
                echo "PHASE: Requirements Gathering"
                echo "=========================================="
                echo ""

                local next_phase=$("$PHASES_DIR/requirements_conversation.sh" "$session_id" "$project_name")

                if [[ $? -ne 0 ]]; then
                    error "Requirements phase failed"
                    return 1
                fi

                current_phase="$next_phase"
                ;;

            architecture)
                echo ""
                echo "=========================================="
                echo "PHASE: Architecture Design"
                echo "=========================================="
                echo ""

                local next_phase=$("$PHASES_DIR/architecture_conversation.sh" "$session_id")

                if [[ $? -ne 0 ]]; then
                    error "Architecture phase failed"
                    return 1
                fi

                current_phase="$next_phase"
                ;;

            build)
                echo ""
                echo "=========================================="
                echo "PHASE: Build"
                echo "=========================================="
                echo ""

                local next_phase=$("$PHASES_DIR/build_phase.sh" "$session_id")

                if [[ $? -ne 0 ]]; then
                    error "Build phase failed"
                    return 1
                fi

                current_phase="$next_phase"
                ;;

            review)
                echo ""
                echo "=========================================="
                echo "PHASE: Review & Iteration"
                echo "=========================================="
                echo ""

                info "Review phase - gather feedback and iterate"
                echo ""
                echo "What would you like to do?"
                echo "  1. Refine requirements"
                echo "  2. Refine architecture"
                echo "  3. Rebuild"
                echo "  4. Complete"
                echo ""
                echo -n "Choice (1-4): "
                read -r choice

                case "$choice" in
                    1)
                        current_phase="requirements"
                        ;;
                    2)
                        current_phase="architecture"
                        ;;
                    3)
                        current_phase="build"
                        ;;
                    4)
                        success "Project development complete!"
                        echo ""
                        echo "Generated artifacts:"
                        echo "  - Requirements: $(get_context_value "$session_id" "requirements.file")"
                        echo "  - Architecture: $(get_context_value "$session_id" "architecture.file")"
                        echo "  - Tasks: $(get_context_value "$session_id" "architecture.tasks_file")"
                        echo ""
                        echo "Session: $session_id"
                        echo "Context: $context_file"
                        echo ""
                        return 0
                        ;;
                    *)
                        echo "Invalid choice"
                        ;;
                esac
                ;;

            *)
                error "Unknown phase: $current_phase"
                return 1
                ;;
        esac

        # Show phase transition
        if [[ -n "$current_phase" ]]; then
            echo ""
            info "→ Transitioning to $current_phase phase"
            sleep 1
        fi
    done
}

# Main entry point
main() {
    if [[ $# -lt 1 ]]; then
        echo "Usage: $0 <session_id> [project_name]"
        echo ""
        echo "Examples:"
        echo "  $0 calculator calculator"
        echo "  $0 proj-A myproject"
        echo ""
        echo "This script orchestrates the full development workflow:"
        echo "  - Requirements gathering (conversational)"
        echo "  - Architecture design (conversational, can hop back to requirements)"
        echo "  - Build execution (automated)"
        echo "  - Review & iteration (feedback-driven)"
        echo ""
        exit 1
    fi

    develop_project "$@"
}

# Run if executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
