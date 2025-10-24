#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# build_phase.sh - Build phase workflow
#
# Executes tasks from tasks.json with dependency resolution

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/../lib"
TOOLS_DIR="$SCRIPT_DIR/../tools"

# Load required libraries
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/context-manager.sh"
source "$LIB_DIR/logging.sh"

# Build phase workflow
# Args: session_id
# Returns: next_phase
build_phase() {
    local session_id="$1"

    local context_file=$(get_context_file "$session_id")
    local session_dir=$(get_session_dir "$session_id")

    if [[ ! -f "$context_file" ]]; then
        error "Context file not found: $context_file"
        return 1
    fi

    # Check if architecture is converged
    local architecture_converged=$(get_context_value "$session_id" "architecture.converged")
    if [[ "$architecture_converged" != "true" ]]; then
        warning "Architecture not converged yet"
        echo "architecture"
        return 0
    fi

    # Get tasks file
    local tasks_file=$(get_context_value "$session_id" "architecture.tasks_file")
    if [[ ! -f "$tasks_file" ]]; then
        error "tasks.json not found: $tasks_file"
        error "Run architecture phase first"
        echo "architecture"
        return 0
    fi

    info "Starting build phase for $session_id"
    info "Tasks file: $tasks_file"
    echo ""

    # Load tasks
    local tasks_json=$(cat "$tasks_file")
    local task_count=$(echo "$tasks_json" | jq '.tasks | length')

    echo "Tasks to execute: $task_count"
    echo ""

    # Show task list
    echo "Task breakdown:"
    echo "$tasks_json" | jq -r '.tasks[] | "  [\(.status)] \(.id) - \(.name)"'
    echo ""

    # Ask user to proceed
    echo -n "Start build execution? (yes/no): "
    read -r proceed

    if [[ ! "$proceed" =~ ^[Yy] ]]; then
        echo ""
        echo "Build cancelled. Returning to architecture phase."
        echo "architecture"
        return 0
    fi

    echo ""
    info "Executing tasks..."
    echo ""

    # Get tasks in dependency order
    local ordered_tasks=$("$TOOLS_DIR/resolve_task_dependencies" "$tasks_file")
    if [[ $? -ne 0 ]]; then
        error "Failed to resolve task dependencies"
        return 1
    fi

    # Execute tasks in order
    local completed_count=0
    local failed_count=0

    while IFS= read -r task_id; do
        if [[ -z "$task_id" ]]; then
            continue
        fi

        # Get task details
        local task=$(echo "$tasks_json" | jq -r ".tasks[] | select(.id == \"$task_id\")")
        local task_name=$(echo "$task" | jq -r '.name')
        local task_type=$(echo "$task" | jq -r '.type')

        echo ""
        info "Executing task: $task_id"
        echo "  Name: $task_name"
        echo "  Type: $task_type"
        echo ""

        # Execute the task (simplified for Milestone 3)
        execute_task "$session_id" "$task_id" "$task_name" "$task_type"
        local result=$?

        if [[ $result -eq 0 ]]; then
            success "Task $task_id completed"
            completed_count=$((completed_count + 1))

            # Update task status in tasks.json
            local temp_file=$(mktemp)
            jq --arg id "$task_id" \
               '(.tasks[] | select(.id == $id).status) = "completed"' \
               "$tasks_file" > "$temp_file"
            mv "$temp_file" "$tasks_file"
        else
            error "Task $task_id failed"
            failed_count=$((failed_count + 1))

            # Update task status in tasks.json
            local temp_file=$(mktemp)
            jq --arg id "$task_id" \
               '(.tasks[] | select(.id == $id).status) = "failed"' \
               "$tasks_file" > "$temp_file"
            mv "$temp_file" "$tasks_file"

            # For now, stop on first failure
            break
        fi
    done <<< "$ordered_tasks"

    echo ""
    echo "=========================================="
    echo "Build Summary"
    echo "=========================================="
    echo "Total tasks: $task_count"
    echo "Completed: $completed_count"
    echo "Failed: $failed_count"
    echo "=========================================="
    echo ""

    # Update context with build results
    local build_status="completed"
    if [[ $failed_count -gt 0 ]]; then
        build_status="failed"
    fi

    update_context "$session_id" "{\"build\":{\"status\":\"$build_status\",\"completed_tasks\":$completed_count,\"failed_tasks\":$failed_count}}"

    if [[ $build_status == "completed" ]]; then
        success "Build phase complete!"
        echo "review"
        return 0
    else
        error "Build phase had failures"
        echo "architecture"
        return 0
    fi
}

# Execute a single task (simplified implementation for Milestone 3)
# Args: session_id, task_id, task_name, task_type
# Returns: 0 on success, 1 on failure
execute_task() {
    local session_id="$1"
    local task_id="$2"
    local task_name="$3"
    local task_type="$4"

    # For Milestone 3, simulate task execution
    # In a full implementation, this would:
    # 1. Initialize builder state
    # 2. Fork builder process
    # 3. Monitor progress
    # 4. Handle failures

    case "$task_type" in
        setup)
            info "Setting up project structure..."
            sleep 1
            return 0
            ;;

        implementation)
            info "Implementing component..."
            sleep 2
            # Simulate 90% success rate
            if [[ $((RANDOM % 10)) -lt 9 ]]; then
                return 0
            else
                return 1
            fi
            ;;

        test)
            info "Running tests..."
            sleep 1
            # Simulate 95% success rate
            if [[ $((RANDOM % 20)) -lt 19 ]]; then
                return 0
            else
                return 1
            fi
            ;;

        integration)
            info "Running integration tests..."
            sleep 2
            return 0
            ;;

        documentation)
            info "Generating documentation..."
            sleep 1
            return 0
            ;;

        *)
            warning "Unknown task type: $task_type"
            return 0
            ;;
    esac
}

# Main entry point
main() {
    if [[ $# -lt 1 ]]; then
        echo "Usage: $0 <session_id>"
        exit 1
    fi

    build_phase "$@"
}

# Run if executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
