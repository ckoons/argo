#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# requirements_conversation.sh - Requirements gathering phase workflow
#
# CI-driven conversation to gather and converge on project requirements

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/../lib"
TOOLS_DIR="$SCRIPT_DIR/../tools"

# Load required libraries
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/context-manager.sh"
source "$LIB_DIR/ci_onboarding.sh"
source "$LIB_DIR/logging.sh"

# Requirements conversation workflow
# Args: session_id, project_name
# Returns: next_phase
requirements_conversation() {
    local session_id="$1"
    local project_name="${2:-unnamed_project}"

    local context_file=$(get_context_file "$session_id")

    # Create initial context if doesn't exist
    if [[ ! -f "$context_file" ]]; then
        info "Creating initial context for $session_id"
        context_file=$(create_initial_context "$session_id" "$project_name")

        # Onboard CI with workflow primer
        info "Onboarding CI with workflow system..."
        local primer=$(get_workflow_primer)

        # CI creates initial understanding
        local init_response=$(ci <<EOF
$(get_workflow_primer)

Project: $project_name

Initialize your understanding of this workflow. You are now ready to help the user define requirements.

Respond with a greeting and first question in JSON format:
{
  "message": "your greeting and first question",
  "next_phase": "requirements"
}
EOF
)

        # Display CI's greeting
        local greeting=$(echo "$init_response" | jq -r '.message')
        echo ""
        echo "CI: $greeting"
        echo ""
    else
        info "Resuming requirements conversation for $session_id"
        echo ""
        echo "CI: Welcome back! Let's continue refining the requirements."
        echo ""
    fi

    # Main conversation loop
    local converged=false
    while [[ "$converged" == "false" ]]; do
        # Get user input
        echo -n "You: "
        read -r user_input

        # Skip empty input
        if [[ -z "$user_input" ]]; then
            continue
        fi

        # Load current context
        local context_json=$(load_context "$session_id")

        # Call CI with full context and user input
        local ci_response=$(ci <<EOF
Context:
$context_json

User input: $user_input

Tasks:
1. Respond to the user conversationally
2. Update context with any new requirements information
3. Decide if we should stay in requirements or move to another phase
4. Determine if requirements are converged (ready to proceed)

Respond in JSON format as specified in the workflow primer.
EOF
)

        # Parse CI response
        local message=$(echo "$ci_response" | jq -r '.message')
        local context_updates=$(echo "$ci_response" | jq -r '.context_updates // {}')
        local next_phase=$(echo "$ci_response" | jq -r '.next_phase')
        local converged_response=$(echo "$ci_response" | jq -r '.convergence.ready')
        local convergence_summary=$(echo "$ci_response" | jq -r '.convergence.summary // ""')

        # Display CI's message
        echo ""
        echo "CI: $message"
        echo ""

        # Apply context updates
        if [[ "$context_updates" != "{}" ]] && [[ "$context_updates" != "null" ]]; then
            update_context "$session_id" "$context_updates"
        fi

        # Add conversation message to context
        add_conversation_message "$session_id" "user" "$user_input"
        add_conversation_message "$session_id" "assistant" "$message"

        # Check if CI decided to hop to different phase
        if [[ "$next_phase" != "requirements" ]]; then
            warning "CI recommends moving to $next_phase phase"
            update_current_phase "$session_id" "$next_phase"
            echo "$next_phase"
            return 0
        fi

        # Check if converged
        if [[ "$converged_response" == "true" ]]; then
            info "CI believes requirements are converged"

            # Run DC validation
            info "Running validation..."
            if "$TOOLS_DIR/validate_requirements" "$context_file"; then
                success "Validation passed"

                # Generate requirements.md
                info "Generating requirements.md..."
                "$TOOLS_DIR/generate_requirements_md" "$context_file"

                # Show summary and ask user approval
                echo ""
                echo "=========================================="
                echo "Requirements Summary"
                echo "=========================================="
                if [[ -n "$convergence_summary" ]]; then
                    echo "$convergence_summary"
                else
                    # Show requirements from context
                    local platform=$(get_context_value "$session_id" "requirements.platform")
                    local features=$(jq -r '.requirements.features[]' <<< "$context_json" 2>/dev/null)
                    echo "Platform: $platform"
                    echo "Features:"
                    while IFS= read -r feature; do
                        echo "  - $feature"
                    done <<< "$features"
                fi
                echo "=========================================="
                echo ""

                # Ask user for approval
                echo -n "Does this capture your vision? (yes/no/change): "
                read -r approval

                case "$approval" in
                    yes|y|Y)
                        # Mark requirements as converged
                        update_context "$session_id" '{"requirements":{"converged":true}}'
                        add_decision "$session_id" "Requirements converged and approved by user"

                        # Move to next phase
                        local next_phase=$(echo "$ci_response" | jq -r '.next_phase // "architecture"')
                        update_current_phase "$session_id" "$next_phase"

                        success "Requirements complete! Moving to $next_phase phase"
                        echo "$next_phase"
                        return 0
                        ;;
                    no|n|N|change|c|C)
                        echo ""
                        echo "CI: Let's refine the requirements. What would you like to change?"
                        echo ""
                        # Continue loop
                        ;;
                    *)
                        echo "Please answer yes, no, or change"
                        ;;
                esac
            else
                # Validation failed
                error "Validation failed"
                local errors=$(jq -r '.requirements.validation_errors[]' "$context_file")
                echo "CI: I found some issues we need to address:"
                echo "$errors"
                echo ""
                # Continue conversation to fix errors
            fi
        fi
    done
}

# Main entry point
main() {
    if [[ $# -lt 1 ]]; then
        echo "Usage: $0 <session_id> [project_name]"
        exit 1
    fi

    requirements_conversation "$@"
}

# Run if executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
