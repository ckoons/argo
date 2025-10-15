#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Basic test for create_workflow

set -e

echo "Running basic test for create_workflow..."

# Test 1: Verify workflow script exists
if [[ ! -f "../workflow.sh" ]]; then
    echo "ERROR: workflow.sh not found"
    exit 1
fi
echo "✓ Workflow script exists"

# Test 2: Verify workflow script is executable
if [[ ! -x "../workflow.sh" ]]; then
    echo "ERROR: workflow.sh is not executable"
    exit 1
fi
echo "✓ Workflow script is executable"

# Test 3: Verify metadata exists
if [[ ! -f "../metadata.yaml" ]]; then
    echo "ERROR: metadata.yaml not found"
    exit 1
fi
echo "✓ Metadata file exists"

# Test 4: Verify README exists
if [[ ! -f "../README.md" ]]; then
    echo "ERROR: README.md not found"
    exit 1
fi
echo "✓ README exists"

# Test 5: Verify prompts directory exists
if [[ ! -d "../prompts" ]]; then
    echo "ERROR: prompts directory not found"
    exit 1
fi
echo "✓ Prompts directory exists"

# Test 6: Verify all required prompt files exist
REQUIRED_PROMPTS=(
    "user_onboarding.txt"
    "requirements_questions.txt"
    "design_summary.txt"
    "ci_onboarding.txt"
)

for prompt in "${REQUIRED_PROMPTS[@]}"; do
    if [[ ! -f "../prompts/$prompt" ]]; then
        echo "ERROR: Prompt file missing: $prompt"
        exit 1
    fi
done
echo "✓ All prompt files exist"

# Test 7: Verify metadata has required fields
if ! grep -q "name:" "../metadata.yaml"; then
    echo "ERROR: metadata.yaml missing 'name' field"
    exit 1
fi
echo "✓ Metadata is valid"

echo "All basic tests passed!"
exit 0
