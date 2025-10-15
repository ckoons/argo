# Workflow Creation System Design

© 2025 Casey Koons. All rights reserved.

## Overview

Design for a meta-workflow system that creates, tests, and documents Argo workflows through AI-guided conversation.

## Goals

1. **User-Friendly Creation**: Non-technical users can create workflows through conversation
2. **Self-Documenting**: Workflows include their own documentation and tests
3. **Testable**: Workflows can be tested before "production" use
4. **Iterative**: Easy to refine and improve workflows
5. **Single Source of Truth**: All workflow metadata in one file

## Workflow Template Structure

### Complete Template Format

A workflow template is a bash script with structured metadata sections:

```bash
#!/bin/bash
# Argo Workflow Template
# Generated: 2025-01-15 by create_workflow

#===================================================================
# METADATA
#===================================================================
# @workflow-name: build_and_test
# @description: Build Python application and run tests
# @author: Casey Koons
# @created: 2025-01-15
# @version: 1.0
# @tags: python, testing, ci
#
# @parameters:
#   TEST_PATH: Path to tests (default: tests/)
#   PYTHON_VERSION: Python version (default: 3.11)
#
# @success-criteria:
#   - All tests pass (exit code 0)
#   - Build produces artifacts
#   - No lint errors
#
# @dependencies:
#   - Python 3.11+
#   - pytest
#   - build tools
#===================================================================

#===================================================================
# DOCUMENTATION
#===================================================================
# ## Purpose
#
# This workflow builds a Python application and runs its test suite.
# It validates code quality, runs tests, and produces build artifacts.
#
# ## Usage
#
#   arc workflow start build_and_test my_build
#   arc workflow start build_and_test my_build TEST_PATH=tests/unit/
#
# ## Steps
#
# 1. Environment Setup - Install dependencies
# 2. Lint Check - Run ruff/black for code quality
# 3. Run Tests - Execute pytest with coverage
# 4. Build - Create distribution packages
# 5. Validate - Verify artifacts exist
#
# ## Success Criteria
#
# Workflow succeeds when:
# - All linters pass (exit code 0)
# - All tests pass with >80% coverage
# - Build artifacts exist in dist/
#
# ## Troubleshooting
#
# - If tests fail: Check test logs in workflow output
# - If build fails: Verify dependencies in requirements.txt
# - If lint fails: Run 'ruff check --fix' locally
#
# ## Examples
#
#   # Run with specific test path
#   arc workflow start build_and_test test1 TEST_PATH=tests/integration/
#
#   # Run with different Python version
#   arc workflow start build_and_test test2 PYTHON_VERSION=3.12
#===================================================================

#===================================================================
# TESTS
#===================================================================
# Embedded tests for this workflow template
# Run with: arc workflow test build_and_test
#
# @test-name: test_with_passing_tests
# @test-description: Verify workflow succeeds with passing tests
# @test-setup:
#   - Create temporary Python project
#   - Add passing test file
# @test-expected: Exit code 0, artifacts created
# @test-cleanup: Remove temporary project
#
# @test-name: test_with_failing_tests
# @test-description: Verify workflow fails appropriately with failing tests
# @test-setup:
#   - Create temporary Python project
#   - Add failing test file
# @test-expected: Exit code 1, error message in logs
# @test-cleanup: Remove temporary project
#
# @test-name: test_missing_dependencies
# @test-description: Verify workflow handles missing dependencies
# @test-setup:
#   - Create project without requirements.txt
# @test-expected: Exit code 1, clear error message
# @test-cleanup: Remove temporary project
#===================================================================

# Actual workflow implementation starts here

set -e  # Exit on error
set -u  # Exit on undefined variable

# Parse parameters with defaults
TEST_PATH=${TEST_PATH:-tests/}
PYTHON_VERSION=${PYTHON_VERSION:-3.11}

echo "=== Build and Test Workflow ==="
echo "Test Path: $TEST_PATH"
echo "Python Version: $PYTHON_VERSION"
echo ""

# Step 1: Environment Setup
echo "Step 1: Setting up environment..."
python${PYTHON_VERSION} -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
pip install pytest pytest-cov ruff black

# Step 2: Lint Check
echo ""
echo "Step 2: Running linters..."
ruff check .
black --check .

# Step 3: Run Tests
echo ""
echo "Step 3: Running tests..."
pytest "$TEST_PATH" --cov=. --cov-report=term-missing

# Step 4: Build
echo ""
echo "Step 4: Building distribution..."
python -m build

# Step 5: Validate
echo ""
echo "Step 5: Validating artifacts..."
if [ ! -d "dist" ] || [ -z "$(ls -A dist)" ]; then
    echo "ERROR: Build artifacts not found in dist/"
    exit 1
fi

echo ""
echo "=== Workflow Complete ==="
echo "Build artifacts: $(ls dist/)"
exit 0
```

### Metadata Section Design

```bash
#===================================================================
# METADATA
#===================================================================
# Structured key-value pairs for workflow discovery and management
#
# Required fields:
#   @workflow-name: Unique identifier
#   @description: One-line summary
#   @version: Semantic version (major.minor.patch)
#
# Optional fields:
#   @author: Creator name
#   @created: ISO date
#   @updated: ISO date
#   @tags: Comma-separated keywords
#   @parameters: Named parameters with defaults
#   @success-criteria: List of success conditions
#   @dependencies: Required tools/libraries
#   @timeout: Maximum execution time (seconds)
#   @retry-on-failure: Boolean (true/false)
#===================================================================
```

### Documentation Section Design

```bash
#===================================================================
# DOCUMENTATION
#===================================================================
# User-facing documentation in Markdown format
#
# Sections:
#   ## Purpose - Why this workflow exists
#   ## Usage - How to run it
#   ## Steps - What it does
#   ## Success Criteria - What "done" looks like
#   ## Troubleshooting - Common issues
#   ## Examples - Real usage examples
#
# This documentation is:
#   - Extracted by 'arc workflow docs <workflow>'
#   - Searchable by 'arc workflow search <keyword>'
#   - Displayed in workflow listings
#===================================================================
```

### Tests Section Design

```bash
#===================================================================
# TESTS
#===================================================================
# Embedded test cases using structured annotations
#
# Each test has:
#   @test-name: Unique test identifier
#   @test-description: What this test validates
#   @test-setup: Preparation steps (bash commands)
#   @test-expected: Expected outcome
#   @test-cleanup: Cleanup steps (bash commands)
#
# Tests are executed by 'arc workflow test <workflow> <test-name>'
# All tests run with 'arc workflow test <workflow> --all'
#
# Test execution creates isolated environment:
#   - Temporary directory
#   - Separate log file
#   - No side effects on real data
#===================================================================
```

## Command Design

### `arc workflow test <template> [test-name]`

**Purpose**: Run embedded tests against a workflow template

**Behavior**:
```bash
# Run specific test
arc workflow test build_and_test test_with_passing_tests
  → Executes test-setup commands
  → Runs workflow with test data
  → Validates against test-expected
  → Runs test-cleanup
  → Reports PASS/FAIL

# Run all tests
arc workflow test build_and_test --all
  → Runs each @test-name in sequence
  → Reports summary (X passed, Y failed)
  → Exit code 0 if all pass, 1 if any fail

# List available tests
arc workflow test build_and_test --list
  → Shows all @test-name entries
  → Shows descriptions
```

**Implementation**:
1. Parse workflow template for `@test-name` blocks
2. Extract setup/expected/cleanup commands
3. Create temporary test environment
4. Execute setup → workflow → validation → cleanup
5. Report results to user

### `arc workflow start` vs `arc workflow test`

**Key Difference**:
- `start`: Runs workflow "for keeps" (production use)
- `test`: Runs workflow in isolated test environment (validation)

```bash
# Testing phase (development)
arc workflow test my_workflow test_basic      # Validate it works
arc workflow test my_workflow --all           # Run all tests

# Production use
arc workflow start my_workflow instance1      # Real execution
```

### `arc workflow docs <template>`

**Purpose**: Display workflow documentation

```bash
arc workflow docs build_and_test
  → Extracts DOCUMENTATION section
  → Renders Markdown
  → Shows in terminal (or browser if --web flag)
```

### `arc workflow search <keyword>`

**Purpose**: Find workflows by keyword

```bash
arc workflow search python
  → Searches @tags, @description, documentation
  → Returns matching workflow templates
  → Shows brief summary of each
```

## User Onboarding Prompt

```
╔════════════════════════════════════════════════════════════════╗
║                   WORKFLOW CREATOR                             ║
║              Create Custom Argo Workflows                      ║
╚════════════════════════════════════════════════════════════════╝

Welcome! I'll help you create a custom workflow for Argo.

A workflow is a series of steps that automate a task - like building
code, running tests, deploying applications, or processing data.

I'll ask you a few questions to understand what you need:

┌────────────────────────────────────────────────────────────────┐
│ 1. WHAT SHOULD THIS WORKFLOW DO?                              │
│                                                                │
│    Describe the task in plain English:                        │
│    - What process should it automate?                         │
│    - What inputs does it need?                                │
│    - What outputs should it produce?                          │
│                                                                │
│    Examples:                                                   │
│    • "Build my Python app and run tests"                      │
│    • "Deploy my website to production"                        │
│    • "Process CSV files and generate reports"                 │
│    • "Backup database and upload to S3"                       │
└────────────────────────────────────────────────────────────────┘

Your answer: _

┌────────────────────────────────────────────────────────────────┐
│ 2. WHAT DOES SUCCESS LOOK LIKE?                               │
│                                                                │
│    How do you know when the workflow completed successfully?  │
│                                                                │
│    Examples:                                                   │
│    • "All tests pass and build artifacts exist"               │
│    • "Website is accessible at production URL"                │
│    • "Report file is created with valid data"                 │
│    • "Backup file exists and is not empty"                    │
└────────────────────────────────────────────────────────────────┘

Your answer: _

┌────────────────────────────────────────────────────────────────┐
│ 3. HOW SHOULD WE TEST AND VALIDATE?                           │
│                                                                │
│    How can we verify the workflow works correctly?            │
│                                                                │
│    Examples:                                                   │
│    • "Run pytest and check exit code is 0"                    │
│    • "Curl the URL and check for HTTP 200"                    │
│    • "Check report has >0 rows of data"                       │
│    • "Verify backup file size > 1MB"                          │
└────────────────────────────────────────────────────────────────┘

Your answer: _

┌────────────────────────────────────────────────────────────────┐
│ OPTIONAL: Additional Context                                  │
│                                                                │
│ Anything else I should know?                                  │
│ - Dependencies (Python, Node, specific tools)                 │
│ - Where files/code are located                                │
│ - Environment variables needed                                │
│ - How often this should run                                   │
└────────────────────────────────────────────────────────────────┘

Your answer (press Enter to skip): _

Great! Let me analyze your requirements and propose a solution...
[User input passed to CI with context...]
```

## CI Onboarding Prompt (The Complex One)

```markdown
# WORKFLOW CREATOR AI - System Instructions

You are an AI assistant helping users create Argo workflow templates.
Your role is to translate user requirements into executable bash scripts
with embedded documentation and tests.

## Context: What Argo Workflows Are

Argo is a workflow automation system that executes bash scripts as
workflows. Each workflow:

- Is a standard bash script (#!/bin/bash)
- Executes in isolated environment with stdin/stdout/stderr
- Can receive parameters via environment variables
- Succeeds (exit 0) or fails (exit non-zero)
- Logs all output to ~/.argo/logs/{workflow_id}.log
- Can be paused, resumed, or abandoned during execution

## Available Argo Commands (in workflows)

Workflows can use these `arc` commands:

```bash
# Workflow management
arc workflow start <template> <instance> [ENV_VAR=value]
arc workflow list
arc workflow status <workflow_id>
arc workflow pause <workflow_id>
arc workflow resume <workflow_id>
arc workflow abandon <workflow_id>
arc workflow attach <workflow_id>

# Workflow development
arc workflow test <template> [test-name]
arc workflow docs <template>
arc workflow search <keyword>
```

## Workflow Template Format

Every workflow template has this structure:

```bash
#!/bin/bash
# Argo Workflow Template

#===================================================================
# METADATA
#===================================================================
# @workflow-name: unique_name
# @description: Brief summary
# @version: 1.0
# @author: User Name
# @created: YYYY-MM-DD
# @parameters:
#   PARAM_NAME: Description (default: value)
# @success-criteria:
#   - Condition 1
#   - Condition 2
# @dependencies:
#   - tool1
#   - tool2
#===================================================================

#===================================================================
# DOCUMENTATION
#===================================================================
# ## Purpose
# [What this workflow does and why]
#
# ## Usage
# [How to run it with examples]
#
# ## Steps
# [What happens during execution]
#
# ## Success Criteria
# [What "done" looks like]
#
# ## Troubleshooting
# [Common issues and solutions]
#
# ## Examples
# [Real command examples]
#===================================================================

#===================================================================
# TESTS
#===================================================================
# @test-name: test_basic_success
# @test-description: Verify workflow succeeds in happy path
# @test-setup:
#   mkdir -p /tmp/test_data
#   echo "test" > /tmp/test_data/input.txt
# @test-expected: Exit code 0, output file exists
# @test-cleanup:
#   rm -rf /tmp/test_data
#
# [Additional test cases...]
#===================================================================

# Implementation starts here
set -e  # Exit on error
set -u  # Exit on undefined variable

# Your workflow code...
```

## Your Responsibilities

### 1. Understand User Requirements

When user provides their answers:
- Parse what they want to accomplish
- Identify inputs, outputs, and dependencies
- Recognize success criteria
- Spot ambiguities or missing information

### 2. Ask Clarifying Questions (When Needed)

If requirements are unclear, ask specific questions:
- "What programming language is your app written in?"
- "Where is your code located (directory path)?"
- "What test framework do you use (pytest, jest, etc)?"
- "Should this run automatically or manually?"
- "What should happen if a step fails?"

**Don't make assumptions** - ask rather than guess.

### 3. Propose a Solution

Present your understanding and proposed workflow:

```
I understand you want to:
- [Restate goal in clear terms]
- [List key steps]
- [Identify success conditions]

Here's my proposed workflow:

Step 1: [Name] - [What it does]
Step 2: [Name] - [What it does]
Step 3: [Name] - [What it does]

Success means:
- [Criteria 1]
- [Criteria 2]

Does this match your expectations?
```

Wait for user confirmation before proceeding.

### 4. Iterate Based on Feedback

User may say:
- "Yes, looks good" → Proceed to build
- "Actually, I need..." → Incorporate changes, re-propose
- "What about...?" → Answer questions, clarify design

### 5. Generate Complete Workflow Template

When user says "build":

1. **Check completeness**: Do you have all needed information?
   - If gaps exist: Ask final questions
   - If complete: Proceed to generation

2. **Generate workflow script**:
   - Complete METADATA section (all fields)
   - Comprehensive DOCUMENTATION section (all subsections)
   - At least 3 test cases in TESTS section
   - Clean, commented implementation code
   - Proper error handling (set -e, checks, meaningful errors)
   - Parameter defaults and validation

3. **Include tests for**:
   - Happy path (everything succeeds)
   - Failure case (something goes wrong)
   - Edge case (missing dependency, bad input, etc.)

4. **Verify generated code**:
   - Valid bash syntax
   - Uses existing tools (not made-up commands)
   - Has clear error messages
   - Includes cleanup steps
   - Documents all parameters

### 6. Present Results

After generation, tell user:

```
✓ Workflow template created: .argo/workflows/templates/[name].sh

Next steps:
1. Review the workflow:
   arc workflow docs [name]

2. Test it (recommended):
   arc workflow test [name] --all

3. Run it:
   arc workflow start [name] my_instance

4. Refine if needed:
   arc workflow revise [name]
```

## Best Practices for Workflow Creation

### Error Handling

Always include:
```bash
set -e  # Exit immediately on error
set -u  # Exit on undefined variable

# Validate inputs
if [ ! -f "required_file.txt" ]; then
    echo "ERROR: required_file.txt not found"
    exit 1
fi
```

### Parameter Handling

```bash
# Parameters with defaults
PARAM_NAME=${PARAM_NAME:-default_value}

# Validate parameters
if [ -z "$REQUIRED_PARAM" ]; then
    echo "ERROR: REQUIRED_PARAM must be set"
    exit 1
fi
```

### Progress Reporting

```bash
echo "=== Workflow: [Name] ==="
echo "Step 1: [Doing something]..."
# ... do work ...
echo "✓ Step 1 complete"

echo "Step 2: [Doing next thing]..."
# ... do work ...
echo "✓ Step 2 complete"
```

### Success Validation

```bash
# Check files exist
if [ ! -f "expected_output.txt" ]; then
    echo "ERROR: Output file not created"
    exit 1
fi

# Check commands succeeded
if ! command -v required_tool &> /dev/null; then
    echo "ERROR: required_tool not found"
    exit 1
fi
```

## Common Pitfalls to Avoid

❌ **Don't assume tools are installed**
   → Check with `command -v` or provide installation steps

❌ **Don't hardcode paths**
   → Use parameters or detect automatically

❌ **Don't ignore errors**
   → Use `set -e` and explicit error checks

❌ **Don't skip documentation**
   → Users will need to understand the workflow later

❌ **Don't skip tests**
   → Tests prove the workflow works as intended

## Example Interaction Flow

**User Input**:
```
1. What: Build my Python app and run tests
2. Success: All tests pass
3. Test: Run pytest and check exit code
```

**Your Response**:
```
I understand you want to:
- Build a Python application
- Run its test suite using pytest
- Succeed when all tests pass (exit code 0)

A few clarifying questions:
1. Where is your Python code located?
2. Do you have a requirements.txt file?
3. What Python version should we use?
4. Should we generate a coverage report?
```

**User Answers**:
```
1. Code is in ./src
2. Yes, requirements.txt exists
3. Python 3.11
4. Yes, coverage would be helpful
```

**Your Response**:
```
Perfect! Here's my proposed workflow:

Step 1: Setup - Create virtual environment, install dependencies
Step 2: Lint - Run ruff/black to check code quality
Step 3: Test - Run pytest with coverage reporting
Step 4: Validate - Verify coverage meets threshold (80%)

Success criteria:
- All linters pass (exit 0)
- All tests pass (exit 0)
- Coverage >= 80%

Does this look good? Say "build" to generate the template.
```

**User**: "build"

**Your Action**:
- Generate complete workflow template
- Save to .argo/workflows/templates/build_and_test.sh
- Report success and next steps

## Remember

- Be conversational but precise
- Ask questions rather than assume
- Validate before generating
- Include comprehensive documentation
- Create realistic, useful tests
- Use standard bash practices
- Handle errors gracefully
- Give clear next-step instructions

Your goal: Help users create workflows they can trust and maintain.
```

## Prototype `create_workflow` Script

```bash
#!/bin/bash
# Argo Workflow: Create new workflow templates
# This is the meta-workflow that creates other workflows

#===================================================================
# METADATA
#===================================================================
# @workflow-name: create_workflow
# @description: Interactive workflow creator using AI guidance
# @version: 1.0
# @author: Argo System
# @created: 2025-01-15
# @tags: meta, workflow-creation, ai-guided
#
# @parameters:
#   WORKFLOW_NAME: Name for new workflow (optional, can prompt)
#
# @success-criteria:
#   - New workflow template created
#   - Template passes validation
#   - User receives next-step instructions
#
# @dependencies:
#   - argo-daemon (running)
#   - arc CLI
#   - Claude AI provider
#===================================================================

set -e
set -u

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║                   WORKFLOW CREATOR                             ║"
echo "║              Create Custom Argo Workflows                      ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Workflow name (parameter or prompt)
if [ -z "${WORKFLOW_NAME:-}" ]; then
    echo "What would you like to name this workflow?"
    read -p "> " WORKFLOW_NAME
fi

# Check if workflow already exists
TEMPLATE_DIR="${HOME}/.argo/workflows/templates"
TEMPLATE_PATH="${TEMPLATE_DIR}/${WORKFLOW_NAME}.sh"

if [ -f "$TEMPLATE_PATH" ]; then
    echo ""
    echo "⚠ Workflow '$WORKFLOW_NAME' already exists."
    echo "Use 'arc workflow revise $WORKFLOW_NAME' to modify it."
    exit 1
fi

echo ""
echo "Great! Let's create the '$WORKFLOW_NAME' workflow."
echo ""

# Display user onboarding
cat << 'EOF'
┌────────────────────────────────────────────────────────────────┐
│ 1. WHAT SHOULD THIS WORKFLOW DO?                              │
│                                                                │
│    Describe the task in plain English:                        │
│    - What process should it automate?                         │
│    - What inputs does it need?                                │
│    - What outputs should it produce?                          │
│                                                                │
│    Examples:                                                   │
│    • "Build my Python app and run tests"                      │
│    • "Deploy my website to production"                        │
│    • "Process CSV files and generate reports"                 │
└────────────────────────────────────────────────────────────────┘
EOF

read -p "Your answer: " USER_GOAL
echo ""

cat << 'EOF'
┌────────────────────────────────────────────────────────────────┐
│ 2. WHAT DOES SUCCESS LOOK LIKE?                               │
│                                                                │
│    How do you know when the workflow completed successfully?  │
│                                                                │
│    Examples:                                                   │
│    • "All tests pass and build artifacts exist"               │
│    • "Website is accessible at production URL"                │
└────────────────────────────────────────────────────────────────┘
EOF

read -p "Your answer: " SUCCESS_CRITERIA
echo ""

cat << 'EOF'
┌────────────────────────────────────────────────────────────────┐
│ 3. HOW SHOULD WE TEST AND VALIDATE?                           │
│                                                                │
│    How can we verify the workflow works correctly?            │
│                                                                │
│    Examples:                                                   │
│    • "Run pytest and check exit code is 0"                    │
│    • "Curl the URL and check for HTTP 200"                    │
└────────────────────────────────────────────────────────────────┘
EOF

read -p "Your answer: " TEST_METHOD
echo ""

cat << 'EOF'
┌────────────────────────────────────────────────────────────────┐
│ OPTIONAL: Additional Context                                  │
│                                                                │
│ Anything else I should know?                                  │
│ - Dependencies (Python, Node, specific tools)                 │
│ - Where files/code are located                                │
│ - Environment variables needed                                │
└────────────────────────────────────────────────────────────────┘
EOF

read -p "Your answer (press Enter to skip): " ADDITIONAL_CONTEXT
echo ""

# Build the prompt for the CI
AI_PROMPT_FILE="/tmp/create_workflow_prompt_$$.txt"
cat > "$AI_PROMPT_FILE" << EOF
[CI Onboarding Context from system - see full prompt above]

USER REQUIREMENTS:
==================

Workflow Name: $WORKFLOW_NAME

1. Goal: $USER_GOAL

2. Success Criteria: $SUCCESS_CRITERIA

3. Test Method: $TEST_METHOD

4. Additional Context: ${ADDITIONAL_CONTEXT:-None provided}

==================

Please analyze these requirements and:
1. Ask any clarifying questions you need
2. Propose a solution
3. Wait for user approval
4. Generate complete workflow template when user says "build"

Start by restating your understanding and asking clarifying questions.
EOF

echo "Connecting to AI assistant..."
echo ""

# TODO: This needs integration with Claude provider
# For now, show what would be sent
echo "=== AI Prompt Ready ==="
echo "Prompt file: $AI_PROMPT_FILE"
echo ""
echo "Next steps (TODO - implement AI integration):"
echo "1. Send prompt to Claude API"
echo "2. Display AI response"
echo "3. Enter interactive dialog"
echo "4. Generate template when user approves"
echo "5. Save to $TEMPLATE_PATH"
echo "6. Validate and report success"
echo ""
echo "For now, you can review the prompt:"
cat "$AI_PROMPT_FILE"

rm "$AI_PROMPT_FILE"

# Placeholder for now
echo ""
echo "⚠ AI integration not yet implemented"
echo "This is a design prototype showing the user flow."

exit 0
```

## Implementation Plan

### Phase 1: Template Parser (Foundation)

```bash
# Tool: arc workflow parse <template>
# Purpose: Extract metadata, docs, tests from template
# Output: JSON structure with all sections

{
  "metadata": {
    "workflow-name": "build_and_test",
    "description": "...",
    "parameters": {...},
    ...
  },
  "documentation": "...",
  "tests": [
    {
      "name": "test_basic",
      "description": "...",
      "setup": "...",
      "expected": "...",
      "cleanup": "..."
    }
  ],
  "implementation": "#!/bin/bash\n..."
}
```

### Phase 2: Test Runner

```bash
# Tool: arc workflow test <template> [test-name]
# Uses parsed test data to execute tests
```

### Phase 3: Documentation Viewer

```bash
# Tool: arc workflow docs <template>
# Displays formatted documentation
```

### Phase 4: AI Integration

```bash
# Tool: create_workflow with full AI interaction
# Implements dialog, generation, validation
```

### Phase 5: Workflow Revision

```bash
# Tool: revise_workflow <template>
# Loads existing, shows to AI, modifies, regenerates
```

## Open Questions

1. **AI Provider Selection**: Should create_workflow use a specific provider (Claude Sonnet for 200K context)?

2. **Test Execution Environment**: Should tests run in Docker containers for isolation?

3. **Template Validation**: Should we validate bash syntax before saving template?

4. **Version Control**: Should workflow templates be git-tracked automatically?

5. **Template Discovery**: Should we index templates for fast searching?

6. **Template Sharing**: Should users be able to share templates with others?

## Next Steps

1. **Review this design document** - Does the template structure make sense?
2. **Refine AI prompts** - Are the prompts comprehensive enough?
3. **Implement template parser** - Build the foundation for extracting sections
4. **Create prototype** - Test the user experience with manual AI interaction
5. **Iterate based on testing** - Refine prompts and structure based on real usage
