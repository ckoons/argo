#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# failure-handler.sh - Phase 5.1: Builder failure handling
#
# Handles builder failures through:
# - Test analysis and refactoring
# - Builder respawn logic
# - User escalation

# Configuration
ARGO_MAX_RESPAWN_ATTEMPTS="${ARGO_MAX_RESPAWN_ATTEMPTS:-2}"

# Analyze why a builder failed
# Args: session_id, builder_id, design_dir
# Returns: JSON analysis
analyze_builder_failure() {
    local session_id="$1"
    local builder_id="$2"
    local design_dir="$3"

    local builder_file="$(get_session_dir "$session_id")/builders/$builder_id.json"

    if [[ ! -f "$builder_file" ]]; then
        echo '{"error":"Builder state file not found"}'
        return 1
    fi

    # Extract failure information
    local status=$(jq -r '.status' "$builder_file")
    local tests_failed=$(jq -r '.tests_failed_count' "$builder_file")
    local failed_tests=$(jq -r '.failed_tests | join(", ")' "$builder_file")
    local task_description=$(jq -r '.task_description' "$builder_file")

    # Get task details
    local task=$(jq -r ".[] | select(.task_id == \"$builder_id\")" "$design_dir/tasks.json")
    local components=$(echo "$task" | jq -r '.components | join(", ")')
    local expected_tests=$(echo "$task" | jq -r '.tests | join(", ")')

    # Create analysis context
    cat <<EOF
{
  "builder_id": "$builder_id",
  "status": "$status",
  "task_description": "$task_description",
  "components": "$components",
  "expected_tests": "$expected_tests",
  "tests_failed": $tests_failed,
  "failed_test_names": "$failed_tests"
}
EOF
}

# Use AI to analyze test failures and suggest fixes
# Args: session_id, builder_id, design_dir
# Returns: 0 if tests can be fixed, 1 if code needs refactoring
analyze_test_failures() {
    local session_id="$1"
    local builder_id="$2"
    local design_dir="$3"

    local failure_analysis=$(analyze_builder_failure "$session_id" "$builder_id" "$design_dir")

    # Ask AI to analyze the failure
    local analysis=$(ci <<EOF
You are analyzing a build failure in a parallel development workflow.

Failure Details:
$failure_analysis

Requirements:
$(cat "$design_dir/requirements.md")

Architecture:
$(cat "$design_dir/architecture.md")

Analyze this failure and determine:
1. Are the tests themselves problematic (too strict, wrong expectations)?
2. Or is the generated code incorrect?
3. What specific changes would fix this?

Respond in JSON format:
{
  "problem_type": "bad_tests" or "bad_code",
  "explanation": "Why this failed",
  "recommended_action": "refactor_tests" or "regenerate_code" or "manual_review",
  "specific_fixes": ["list", "of", "specific", "changes"]
}
EOF
)

    echo "$analysis"
}

# Refactor tests using AI
# Args: session_id, builder_id, design_dir
# Returns: 0 on success, 1 on failure
refactor_builder_tests() {
    local session_id="$1"
    local builder_id="$2"
    local design_dir="$3"

    info "Analyzing test failures for $(label $builder_id)..."

    local analysis=$(analyze_test_failures "$session_id" "$builder_id" "$design_dir")
    local problem_type=$(echo "$analysis" | jq -r '.problem_type')
    local recommended_action=$(echo "$analysis" | jq -r '.recommended_action')

    echo ""
    box_header "Failure Analysis: $builder_id" "$WARNING"
    echo "$analysis" | jq -r '.explanation'
    echo ""
    echo -e "${BOLD}Recommended action:${NC} $recommended_action"
    box_footer

    if [[ "$recommended_action" == "refactor_tests" ]]; then
        # AI suggests tests need refactoring
        info "Refactoring tests..."

        local task=$(jq -r ".[] | select(.task_id == \"$builder_id\")" "$design_dir/tasks.json")
        local original_tests=$(echo "$task" | jq -r '.tests')
        local specific_fixes=$(echo "$analysis" | jq -r '.specific_fixes | join("\n")')

        # Ask AI to create better tests
        local new_tests=$(ci <<EOF
The original tests for this task are too strict or have wrong expectations.

Original tests:
$original_tests

Specific issues:
$specific_fixes

Requirements:
$(cat "$design_dir/requirements.md")

Task: $(echo "$task" | jq -r '.description')
Components: $(echo "$task" | jq -r '.components | join(", ")')

Create better, more realistic tests that:
1. Test the actual requirements (not overly strict edge cases)
2. Focus on success criteria
3. Allow reasonable implementation flexibility

Output as JSON array of test names:
["test_name_1", "test_name_2", "test_name_3"]
EOF
)

        # Update task with new tests
        local temp_file=$(mktemp)
        jq --argjson tests "$new_tests" \
           "(.[] | select(.task_id == \"$builder_id\").tests) = \$tests" \
           "$design_dir/tasks.json" > "$temp_file"
        mv "$temp_file" "$design_dir/tasks.json"

        success "Tests refactored for $builder_id"
        return 0

    elif [[ "$recommended_action" == "regenerate_code" ]]; then
        # AI suggests code is wrong, need to respawn
        info "Code needs regeneration"
        return 0

    else
        # manual_review or unknown
        warning "Manual review recommended"
        return 1
    fi
}

# Respawn a failed builder with updated tests
# Args: session_id, builder_id, design_dir, build_dir
# Returns: 0 on success, 1 on failure
respawn_builder() {
    local session_id="$1"
    local builder_id="$2"
    local design_dir="$3"
    local build_dir="$4"

    local builder_file="$(get_session_dir "$session_id")/builders/$builder_id.json"

    # Check respawn count
    local respawn_count=$(jq -r '.respawn_count // 0' "$builder_file")

    if [[ $respawn_count -ge $ARGO_MAX_RESPAWN_ATTEMPTS ]]; then
        warning "Max respawn attempts reached for $builder_id"
        return 1
    fi

    # Increment respawn count
    local temp_file=$(mktemp)
    jq --arg count "$((respawn_count + 1))" \
       '.respawn_count = ($count | tonumber) | .status = "respawning"' \
       "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"

    info "Respawning $(label $builder_id) (attempt $((respawn_count + 2)))"

    # Get builder branch
    local builder_branch=$(jq -r '.feature_branch' "$builder_file")

    # Reset builder branch to clean state
    if [[ -n "$builder_branch" ]] && [[ "$builder_branch" != "null" ]]; then
        # Delete and recreate branch
        local feature_branch=$(dirname "$builder_branch")
        git_delete_branch "$builder_branch" 2>/dev/null
        git_create_branch "$builder_branch" "$feature_branch"
    fi

    # Respawn builder process
    local lib_dir="$(dirname "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")/lib"
    "$lib_dir/builder_ci_loop.sh" "$session_id" "$builder_id" "$design_dir" "$build_dir" &
    local new_pid=$!

    success "Respawned $builder_id (PID: $new_pid)"
    echo "$new_pid"
    return 0
}

# Handle a failed builder (Phase 5.1 main entry point)
# Args: session_id, builder_id, design_dir, build_dir
# Returns: 0 if handled, 1 if escalation needed
handle_builder_failure() {
    local session_id="$1"
    local builder_id="$2"
    local design_dir="$3"
    local build_dir="$4"

    warning "Builder $(label $builder_id) failed"

    # Step 1: Analyze failure
    refactor_builder_tests "$session_id" "$builder_id" "$design_dir"
    local refactor_result=$?

    if [[ $refactor_result -ne 0 ]]; then
        # Manual review needed
        escalate_to_user "$session_id" "$builder_id" "$design_dir"
        return 1
    fi

    # Step 2: Respawn builder with improved tests
    local new_pid=$(respawn_builder "$session_id" "$builder_id" "$design_dir" "$build_dir")
    if [[ $? -ne 0 ]]; then
        # Respawn failed
        escalate_to_user "$session_id" "$builder_id" "$design_dir"
        return 1
    fi

    return 0
}

# Escalate to user when automatic recovery fails
# Args: session_id, builder_id, design_dir
escalate_to_user() {
    local session_id="$1"
    local builder_id="$2"
    local design_dir="$3"

    echo ""
    box_header "User Escalation Required: $builder_id" "$ALERT"

    local builder_file="$(get_session_dir "$session_id")/builders/$builder_id.json"
    local task_description=$(jq -r '.task_description' "$builder_file")
    local tests_failed=$(jq -r '.tests_failed_count' "$builder_file")

    echo "Builder $(label $builder_id) cannot recover automatically."
    echo ""
    echo "Task: $task_description"
    echo "Failed tests: $tests_failed"
    echo ""
    echo "Options:"
    echo "  1. Review builder logs: $(get_session_dir "$session_id")/builders/$builder_id.json"
    echo "  2. Check task definition: $design_dir/tasks.json"
    echo "  3. Manual intervention required"

    box_footer

    # Mark builder as needs_assistance
    local temp_file=$(mktemp)
    jq '.needs_assistance = true | .status = "needs_assistance"' \
       "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"
}

# Export functions
export -f analyze_builder_failure
export -f analyze_test_failures
export -f refactor_builder_tests
export -f respawn_builder
export -f handle_builder_failure
export -f escalate_to_user
