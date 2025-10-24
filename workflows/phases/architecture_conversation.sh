#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# architecture_conversation.sh - Architecture design phase workflow
#
# CI-driven conversation to design system architecture based on requirements

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/../lib"
TOOLS_DIR="$SCRIPT_DIR/../tools"

# Load required libraries
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/context-manager.sh"
source "$LIB_DIR/ci_onboarding.sh"
source "$LIB_DIR/logging.sh"

# Architecture conversation workflow
# Args: session_id
# Returns: next_phase
architecture_conversation() {
    local session_id="$1"

    local context_file=$(get_context_file "$session_id")

    if [[ ! -f "$context_file" ]]; then
        error "Context file not found: $context_file"
        error "Run requirements phase first"
        return 1
    fi

    # Check if requirements are converged
    local requirements_converged=$(get_context_value "$session_id" "requirements.converged")
    if [[ "$requirements_converged" != "true" ]]; then
        warning "Requirements not converged yet"
        echo "requirements"
        return 0
    fi

    info "Starting architecture conversation for $session_id"

    # Load requirements for context
    local requirements_file=$(get_context_value "$session_id" "requirements.file")
    local requirements_content=""
    if [[ -f "$requirements_file" ]]; then
        requirements_content=$(cat "$requirements_file")
    fi

    # Initial architecture greeting
    echo ""
    echo "CI: Great! Now let's design the architecture to meet these requirements."
    echo ""

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

Requirements:
$requirements_content

User input: $user_input

You are in the ARCHITECTURE phase. Your tasks:
1. Discuss component breakdown, interfaces, technology choices
2. WATCH for statements that reveal REQUIREMENT gaps
   - Example: User mentions performance needs not in requirements
   - Example: User mentions platform detail not specified
3. If gap detected, suggest hopping back to requirements
4. Update context with architectural decisions
5. Determine if architecture is converged (ready to build)

Respond in JSON format as specified in the workflow primer.
EOF
)

        # Parse CI response
        local message=$(echo "$ci_response" | jq -r '.message')
        local context_updates=$(echo "$ci_response" | jq -r '.context_updates // {}')
        local next_phase=$(echo "$ci_response" | jq -r '.next_phase')
        local converged_response=$(echo "$ci_response" | jq -r '.convergence.ready')
        local convergence_summary=$(echo "$ci_response" | jq -r '.convergence.summary // ""')
        local hop_reason=$(echo "$ci_response" | jq -r '.metadata.hop_reason // ""')

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

        # Check if CI decided to hop back to requirements
        if [[ "$next_phase" == "requirements" ]]; then
            warning "CI detected requirement gap: $hop_reason"
            echo ""
            echo -n "Hop back to requirements to clarify? (yes/no): "
            read -r hop_approval

            if [[ "$hop_approval" =~ ^[Yy] ]]; then
                add_decision "$session_id" "Hopped back to requirements: $hop_reason"
                update_current_phase "$session_id" "requirements"
                echo "requirements"
                return 0
            else
                echo ""
                echo "CI: Okay, let's continue with architecture using current understanding."
                echo ""
                continue
            fi
        fi

        # Check if converged
        if [[ "$converged_response" == "true" ]]; then
            info "CI believes architecture is converged"

            # Run DC validation
            info "Running validation..."
            if "$TOOLS_DIR/validate_architecture" "$context_file"; then
                success "Validation passed"

                # Generate architecture.md
                info "Generating architecture.md..."
                "$TOOLS_DIR/generate_architecture_md" "$context_file"

                # Generate tasks.json
                info "Generating tasks.json..."
                "$TOOLS_DIR/generate_tasks" "$context_file"

                # Show summary and ask user approval
                echo ""
                echo "=========================================="
                echo "Architecture Summary"
                echo "=========================================="
                if [[ -n "$convergence_summary" ]]; then
                    echo "$convergence_summary"
                else
                    # Show architecture from context
                    local components=$(jq -r '.architecture.components[]' <<< "$context_json" 2>/dev/null)
                    echo "Components:"
                    while IFS= read -r component; do
                        echo "  - $component"
                    done <<< "$components"
                fi
                echo "=========================================="
                echo ""

                # Ask user for approval to build
                echo -n "Should we build this? (yes/no/change): "
                read -r approval

                case "$approval" in
                    yes|y|Y)
                        # Mark architecture as converged
                        update_context "$session_id" '{"architecture":{"converged":true}}'
                        add_decision "$session_id" "Architecture converged and approved by user"

                        # Move to build phase
                        update_current_phase "$session_id" "build"

                        success "Architecture complete! Moving to build phase"
                        echo "build"
                        return 0
                        ;;
                    no|n|N|change|c|C)
                        echo ""
                        echo "CI: Let's refine the architecture. What would you like to change?"
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
                local errors=$(jq -r '.architecture.validation_errors[]' "$context_file")
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
        echo "Usage: $0 <session_id>"
        exit 1
    fi

    architecture_conversation "$@"
}

# Run if executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
