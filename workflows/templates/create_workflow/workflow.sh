#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# create_workflow - Meta-workflow that creates new workflow templates
#
# This workflow guides users through creating a new workflow template
# by asking questions, understanding requirements, and generating a
# complete directory-based workflow template.

set -e

# Template directory
TEMPLATES_DIR="${HOME}/.argo/workflows/templates"

# Workflow state
WORKFLOW_STATE="${HOME}/.argo/workflows/state/${WORKFLOW_ID:-create_workflow}/state.json"

# Ensure state directory exists
mkdir -p "$(dirname "$WORKFLOW_STATE")"

# Logging function
log() {
    echo "[create_workflow] $*"
}

# Ask user a question and get response
ask() {
    local question="$1"
    local varname="$2"

    echo "$question"
    read -r response
    eval "$varname=\"$response\""
}

# User onboarding - explain the process
user_onboarding() {
    log "Welcome to the Argo Workflow Creator!"
    echo ""
    echo "I'll help you create a new workflow template by asking a few questions."
    echo "We'll work together to define what you want, how to test it, and"
    echo "what success looks like."
    echo ""
    echo "This process has three phases:"
    echo "  1. Requirements gathering - Tell me what you want"
    echo "  2. Design dialog - We'll refine the approach together"
    echo "  3. Build phase - I'll create the template"
    echo ""
    read -p "Press Enter to continue..."
    echo ""
}

# Gather requirements from user
gather_requirements() {
    log "Phase 1: Requirements Gathering"
    echo ""

    # Question 1: What do you want?
    ask "1. What do you want this workflow to do?" WORKFLOW_PURPOSE

    # Question 2: How will you test it?
    ask "2. How will you test if this workflow works correctly?" TEST_METHOD

    # Question 3: What is success?
    ask "3. What does success look like? (What should the workflow produce?)" SUCCESS_CRITERIA

    # Optional: Additional context
    echo ""
    echo "4. Any additional context or requirements? (Press Enter to skip)"
    read -r ADDITIONAL_CONTEXT

    # Save requirements to state
    # Note: Using jq would be safer, but keep it simple with printf for JSON escaping
    printf '{
  "phase": "requirements_gathered",
  "purpose": %s,
  "test_method": %s,
  "success_criteria": %s,
  "additional_context": %s
}\n' \
        "$(printf '%s' "$WORKFLOW_PURPOSE" | jq -Rs .)" \
        "$(printf '%s' "$TEST_METHOD" | jq -Rs .)" \
        "$(printf '%s' "$SUCCESS_CRITERIA" | jq -Rs .)" \
        "$(printf '%s' "$ADDITIONAL_CONTEXT" | jq -Rs .)" \
        > "$WORKFLOW_STATE"

    log "Requirements captured!"
}

# Design dialog - refine with AI
design_dialog() {
    log "Phase 2: Design Dialog"
    echo ""
    echo "Now I'll analyze your requirements and propose an implementation approach."
    echo ""

    # This is where we'd call the AI to interpret requirements
    # For now, summarize what we have

    echo "Summary of your workflow:"
    echo "  Purpose: $WORKFLOW_PURPOSE"
    echo "  Testing: $TEST_METHOD"
    echo "  Success: $SUCCESS_CRITERIA"
    echo ""

    # In production, AI would propose design here
    # For prototype, ask for confirmation

    ask "Does this look correct? (yes/no)" CONFIRM

    if [[ "$CONFIRM" != "yes" ]]; then
        log "Let's refine the requirements..."
        gather_requirements
        design_dialog
        return
    fi

    # Update state
    printf '{
  "phase": "design_approved",
  "purpose": %s,
  "test_method": %s,
  "success_criteria": %s,
  "additional_context": %s,
  "approved": true
}\n' \
        "$(printf '%s' "$WORKFLOW_PURPOSE" | jq -Rs .)" \
        "$(printf '%s' "$TEST_METHOD" | jq -Rs .)" \
        "$(printf '%s' "$SUCCESS_CRITERIA" | jq -Rs .)" \
        "$(printf '%s' "$ADDITIONAL_CONTEXT" | jq -Rs .)" \
        > "$WORKFLOW_STATE"
}

# Build the workflow template
build_workflow() {
    log "Phase 3: Build Phase"
    echo ""

    # Get template name
    ask "What should we name this workflow template?" TEMPLATE_NAME

    # Sanitize template name (lowercase, underscores)
    TEMPLATE_NAME=$(echo "$TEMPLATE_NAME" | tr '[:upper:]' '[:lower:]' | tr ' ' '_' | tr -cd '[:alnum:]_')

    TEMPLATE_DIR="$TEMPLATES_DIR/$TEMPLATE_NAME"

    if [[ -d "$TEMPLATE_DIR" ]]; then
        log "Template '$TEMPLATE_NAME' already exists!"
        ask "Overwrite? (yes/no)" OVERWRITE
        if [[ "$OVERWRITE" != "yes" ]]; then
            log "Aborting."
            exit 1
        fi
        rm -rf "$TEMPLATE_DIR"
    fi

    log "Creating template directory: $TEMPLATE_DIR"

    # Create directory structure
    mkdir -p "$TEMPLATE_DIR"
    mkdir -p "$TEMPLATE_DIR/tests"
    mkdir -p "$TEMPLATE_DIR/config"

    # Create workflow.sh
    log "Creating workflow.sh..."
    cat > "$TEMPLATE_DIR/workflow.sh" <<'WORKFLOW_EOF'
#!/bin/bash
# Workflow: {TEMPLATE_NAME}
# Purpose: {PURPOSE}

set -e

log() {
    echo "[{TEMPLATE_NAME}] $*"
}

log "Starting workflow..."

# TODO: Implement workflow logic here

log "Workflow complete!"
WORKFLOW_EOF

    # Replace placeholders
    sed -i '' "s/{TEMPLATE_NAME}/$TEMPLATE_NAME/g" "$TEMPLATE_DIR/workflow.sh"
    sed -i '' "s/{PURPOSE}/$WORKFLOW_PURPOSE/g" "$TEMPLATE_DIR/workflow.sh"

    chmod +x "$TEMPLATE_DIR/workflow.sh"

    # Create metadata.yaml
    log "Creating metadata.yaml..."
    cat > "$TEMPLATE_DIR/metadata.yaml" <<METADATA_EOF
name: $TEMPLATE_NAME
description: $WORKFLOW_PURPOSE
version: 1.0.0
author: $(whoami)
created: $(date -u +"%Y-%m-%dT%H:%M:%SZ")

parameters: []

requirements:
  - bash

environments:
  - dev
  - test
  - prod
METADATA_EOF

    # Create README.md
    log "Creating README.md..."
    cat > "$TEMPLATE_DIR/README.md" <<README_EOF
# $TEMPLATE_NAME

## Description

$WORKFLOW_PURPOSE

## Success Criteria

$SUCCESS_CRITERIA

## Testing

$TEST_METHOD

## Usage

\`\`\`bash
arc workflow start $TEMPLATE_NAME my_instance
\`\`\`

## Parameters

None currently defined.

## Author

Created by $(whoami) on $(date)

---
Generated by create_workflow meta-workflow
README_EOF

    # Create test file
    log "Creating tests/test_basic.sh..."
    cat > "$TEMPLATE_DIR/tests/test_basic.sh" <<TEST_EOF
#!/bin/bash
# Basic test for $TEMPLATE_NAME workflow

set -e

echo "Running basic test for $TEMPLATE_NAME..."

# TODO: Implement test based on: $TEST_METHOD

echo "Test passed!"
exit 0
TEST_EOF

    chmod +x "$TEMPLATE_DIR/tests/test_basic.sh"

    # Create config file
    log "Creating config/defaults.env..."
    cat > "$TEMPLATE_DIR/config/defaults.env" <<CONFIG_EOF
# Default configuration for $TEMPLATE_NAME

# Add configuration variables here
CONFIG_EOF

    log "Template created successfully!"
    echo ""
    echo "Template location: $TEMPLATE_DIR"
    echo ""
    echo "Next steps:"
    echo "  1. Edit $TEMPLATE_DIR/workflow.sh to implement logic"
    echo "  2. Edit $TEMPLATE_DIR/tests/test_basic.sh to implement tests"
    echo "  3. Test with: arc workflow test $TEMPLATE_NAME"
    echo "  4. Use with: arc workflow start $TEMPLATE_NAME instance_name"
    echo ""

    # Update state
    printf '{
  "phase": "completed",
  "template_name": %s,
  "template_dir": %s,
  "created": %s
}\n' \
        "$(printf '%s' "$TEMPLATE_NAME" | jq -Rs .)" \
        "$(printf '%s' "$TEMPLATE_DIR" | jq -Rs .)" \
        "$(date -u +"%Y-%m-%dT%H:%M:%SZ" | jq -Rs .)" \
        > "$WORKFLOW_STATE"
}

# Main workflow execution
main() {
    # Ensure templates directory exists
    mkdir -p "$TEMPLATES_DIR"

    # Run phases
    user_onboarding
    gather_requirements
    design_dialog
    build_workflow

    log "Workflow creation complete!"
}

# Run main
main
