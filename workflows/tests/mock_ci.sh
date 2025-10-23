#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# mock_ci.sh - Mock CI tool for testing workflows
#
# Simulates AI responses without requiring actual AI providers.
# Can be configured to return specific responses based on prompt patterns.

# Mock response database
MOCK_RESPONSES_FILE="${MOCK_RESPONSES_FILE:-/tmp/mock_ci_responses.json}"

# Default responses for common prompts
init_default_responses() {
    if [[ ! -f "$MOCK_RESPONSES_FILE" ]]; then
        cat > "$MOCK_RESPONSES_FILE" <<'EOF'
{
  "responses": {
    "architecture": "# Architecture\n\n## Components\n- Component A: Handles user input\n- Component B: Processes data\n- Component C: Generates output\n\n## Dependencies\n- A depends on B\n- B depends on C",

    "requirements": "# Requirements\n\n1. User must be able to input data\n2. System must process data efficiently\n3. Output must be formatted correctly\n4. Error handling for invalid input",

    "test_suite": "# Test Suite\n\n## Unit Tests\n- test_component_a_input\n- test_component_b_processing\n- test_component_c_output\n\n## Integration Tests\n- test_end_to_end_workflow\n- test_error_handling",

    "code": "#!/bin/bash\n# © 2025 Casey Koons All rights reserved\n\nfunction main() {\n    echo \"Hello from generated code\"\n    return 0\n}\n\nmain \"$@\"",

    "fix": "#!/bin/bash\n# © 2025 Casey Koons All rights reserved\n# Fixed version\n\nfunction main() {\n    echo \"Fixed code\"\n    if [[ $# -eq 0 ]]; then\n        echo \"Error: No arguments\" >&2\n        return 1\n    fi\n    return 0\n}\n\nmain \"$@\"",

    "task_decomposition": "[\n  {\n    \"task_id\": \"argo-A\",\n    \"description\": \"Implement user authentication\",\n    \"components\": [\"auth.sh\", \"login.sh\"],\n    \"tests\": [\"test_login\", \"test_logout\"],\n    \"independent\": true\n  },\n  {\n    \"task_id\": \"argo-B\",\n    \"description\": \"Implement data processing\",\n    \"components\": [\"process.sh\", \"validate.sh\"],\n    \"tests\": [\"test_process\", \"test_validate\"],\n    \"independent\": true\n  },\n  {\n    \"task_id\": \"argo-C\",\n    \"description\": \"Implement output generation\",\n    \"components\": [\"output.sh\", \"format.sh\"],\n    \"tests\": [\"test_output\", \"test_format\"],\n    \"independent\": true\n  }\n]"
  }
}
EOF
    fi
}

# Get response for a prompt pattern
get_response() {
    local prompt="$1"

    # Initialize if needed
    init_default_responses

    # Pattern matching for common prompt types
    local response_key=""

    if echo "$prompt" | grep -qi "architecture"; then
        response_key="architecture"
    elif echo "$prompt" | grep -qi "requirements"; then
        response_key="requirements"
    elif echo "$prompt" | grep -qi "test suite"; then
        response_key="test_suite"
    elif echo "$prompt" | grep -qi "decompose.*tasks"; then
        response_key="task_decomposition"
    elif echo "$prompt" | grep -qi "fix.*code\|failed with"; then
        response_key="fix"
    elif echo "$prompt" | grep -qi "generate.*code\|implement"; then
        response_key="code"
    else
        # Default response
        echo "Mock CI response for prompt: ${prompt:0:50}..."
        return 0
    fi

    # Extract response from JSON
    local response=$(jq -r ".responses.${response_key}" "$MOCK_RESPONSES_FILE" 2>/dev/null)

    if [[ -z "$response" ]] || [[ "$response" == "null" ]]; then
        echo "Mock CI response (no specific handler for: $response_key)"
    else
        echo "$response"
    fi
}

# Set custom response for pattern
set_custom_response() {
    local pattern="$1"
    local response="$2"

    init_default_responses

    # Update JSON with custom response
    local tmp_file=$(mktemp)
    jq ".responses.${pattern} = \"${response}\"" "$MOCK_RESPONSES_FILE" > "$tmp_file"
    mv "$tmp_file" "$MOCK_RESPONSES_FILE"
}

# Clear all custom responses
clear_responses() {
    rm -f "$MOCK_RESPONSES_FILE"
    init_default_responses
}

# Simulate CI delay (for realistic testing)
simulate_delay() {
    local delay="${MOCK_CI_DELAY:-0}"
    if [[ $delay -gt 0 ]]; then
        sleep "$delay"
    fi
}

# Simulate CI failure (for error testing)
check_failure_mode() {
    if [[ "${MOCK_CI_FAIL:-false}" == "true" ]]; then
        echo "Error: Mock CI failure mode enabled" >&2
        return 1
    fi
    return 0
}

# Main execution
main() {
    # Check for failure mode
    if ! check_failure_mode; then
        exit 1
    fi

    # Simulate processing delay
    simulate_delay

    # Read prompt from args or stdin
    local prompt=""
    if [[ $# -gt 0 ]]; then
        prompt="$*"
    else
        prompt=$(cat)
    fi

    if [[ -z "$prompt" ]]; then
        echo "Error: No prompt provided" >&2
        exit 1
    fi

    # Get and output response
    get_response "$prompt"
}

# Run if executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
