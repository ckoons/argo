#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# create_template - Meta-workflow that creates new workflow templates
#
# This workflow guides users through creating a new workflow template
# by asking questions, understanding requirements, and generating a
# complete directory-based workflow template.

set -e

# Template directory
TEMPLATES_DIR="${HOME}/.argo/workflows/templates"

# Workflow state
WORKFLOW_STATE="${HOME}/.argo/workflows/state/${WORKFLOW_ID:-create_template}/state.json"

# Ensure state directory exists
mkdir -p "$(dirname "$WORKFLOW_STATE")"

# Logging function
log() {
    echo "[create_template] $*"
}

# Ask user a question and get response
ask() {
    local question="$1"
    local varname="$2"
    local response

    echo "$question"
    read -r response

    # Use printf with %s to safely assign (avoids eval quoting issues)
    printf -v "$varname" '%s' "$response"
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
    echo "Press Enter to start..."
    read -r
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
    echo "Analyzing your requirements with AI..."
    echo ""

    # Call ci to analyze requirements and propose design
    DESIGN_PROPOSAL=$(ci <<EOF
You are helping design a new Argo workflow template. Based on these requirements, propose a detailed implementation approach:

**Workflow Requirements:**
- Purpose: $WORKFLOW_PURPOSE
- Testing Method: $TEST_METHOD
- Success Criteria: $SUCCESS_CRITERIA
- Additional Context: $ADDITIONAL_CONTEXT

**Please provide:**
1. Recommended workflow structure (what the workflow.sh should do step-by-step)
2. Key implementation steps
3. Suggested test approach (what tests/test_basic.sh should verify)
4. Any configuration variables needed (for config/defaults.env)
5. Any missing requirements or concerns to clarify with the user

Be concise but specific. Focus on actionable implementation guidance.
EOF
)

    if [ $? -ne 0 ]; then
        log "Warning: AI analysis failed, proceeding with manual review"
        DESIGN_PROPOSAL="AI unavailable - manual design review required"
    fi

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "AI Design Proposal:"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "$DESIGN_PROPOSAL"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""

    ask "Does this approach work for you? (yes/no/refine)" CONFIRM

    if [[ "$CONFIRM" == "no" ]]; then
        log "Let's start over with different requirements..."
        gather_requirements
        design_dialog
        return
    elif [[ "$CONFIRM" == "refine" ]]; then
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
    echo "Using AI to generate template implementation..."
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

    # Generate workflow.sh using AI
    log "Generating workflow.sh with AI..."
    WORKFLOW_SCRIPT=$(ci <<EOF
Generate a bash workflow script for this template:

**Template Name:** $TEMPLATE_NAME
**Purpose:** $WORKFLOW_PURPOSE
**Success Criteria:** $SUCCESS_CRITERIA
**Design Guidance:** $DESIGN_PROPOSAL

Generate a complete, executable bash script that:
1. Starts with #!/bin/bash and set -e
2. Includes a log() function: log() { echo "[$TEMPLATE_NAME] \$*"; }
3. Implements the workflow logic based on the purpose and design
4. Has clear comments explaining each step
5. Exits with appropriate status codes
6. Uses best practices (quotes, error handling, etc.)

Output ONLY the bash script, no explanations before or after.
EOF
)

    if [ $? -ne 0 ] || [ -z "$WORKFLOW_SCRIPT" ]; then
        log "Warning: AI generation failed, using template..."
        cat > "$TEMPLATE_DIR/workflow.sh" <<WORKFLOW_EOF
#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Workflow: $TEMPLATE_NAME
# Purpose: $WORKFLOW_PURPOSE

set -e

log() {
    echo "[$TEMPLATE_NAME] \$*"
}

log "Starting workflow..."

# TODO: Implement workflow logic based on:
# $WORKFLOW_PURPOSE
#
# Design guidance:
# $DESIGN_PROPOSAL

log "Workflow complete!"
WORKFLOW_EOF
    else
        # Strip markdown code blocks if present
        WORKFLOW_SCRIPT=$(echo "$WORKFLOW_SCRIPT" | sed '/^```/d')
        echo "$WORKFLOW_SCRIPT" > "$TEMPLATE_DIR/workflow.sh"
    fi

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
Generated by create_template meta-workflow
README_EOF

    # Generate test file using AI
    log "Generating tests/test_basic.sh with AI..."
    TEST_SCRIPT=$(ci <<EOF
Generate a bash test script for this workflow template:

**Template Name:** $TEMPLATE_NAME
**Purpose:** $WORKFLOW_PURPOSE
**Test Method:** $TEST_METHOD
**Success Criteria:** $SUCCESS_CRITERIA

Generate a complete, executable bash test script that:
1. Starts with #!/bin/bash and set -e
2. Tests the workflow based on the test method described
3. Verifies the success criteria are met
4. Prints clear pass/fail messages
5. Exits 0 on success, 1 on failure
6. Uses best practices for bash testing

Output ONLY the bash test script, no explanations before or after.
EOF
)

    if [ $? -ne 0 ] || [ -z "$TEST_SCRIPT" ]; then
        log "Warning: AI test generation failed, using template..."
        cat > "$TEMPLATE_DIR/tests/test_basic.sh" <<TEST_EOF
#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Basic test for $TEMPLATE_NAME workflow

set -e

echo "Running basic test for $TEMPLATE_NAME..."

# TODO: Implement test based on:
# Test Method: $TEST_METHOD
# Success Criteria: $SUCCESS_CRITERIA

echo "Test passed!"
exit 0
TEST_EOF
    else
        # Strip markdown code blocks if present
        TEST_SCRIPT=$(echo "$TEST_SCRIPT" | sed '/^```/d')
        echo "$TEST_SCRIPT" > "$TEMPLATE_DIR/tests/test_basic.sh"
    fi

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
