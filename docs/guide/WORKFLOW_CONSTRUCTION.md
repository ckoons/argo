# Argo Workflow Construction Guide

© 2025 Casey Koons. All rights reserved.

## Overview

Argo workflows are JSON-defined, pauseable, checkpointable task automation sequences. Workflows coordinate AI providers, user interaction, and code operations to accomplish complex multi-step tasks.

**Key Features:**
- **JSON-driven** - Declarative workflow definitions
- **Pauseable** - Stop and resume at any step
- **Checkpointable** - Save and restore workflow state
- **Variable context** - Share data between steps with `{{variable}}` syntax
- **Provider-agnostic** - Works with any CI provider (Claude, OpenAI, etc.)
- **Branching** - Conditional logic and user choices

## Quick Start

### Minimal Workflow

```json
{
  "workflow_name": "hello_world",
  "description": "Simplest possible workflow",
  "steps": [
    {
      "step": "greet",
      "type": "display",
      "message": "Hello, World!",
      "next_step": "EXIT"
    }
  ]
}
```

### Running Workflows

```bash
# Start a workflow (auto-attaches to output)
arc start hello_world test1

# While attached:
# - See live output
# - Ctrl+D to detach (workflow continues in background)

# List running workflows
arc list

# Re-attach to workflow
arc attach hello_world_test1

# Abandon workflow
arc abandon hello_world_test1
```

## Workflow Structure

### Top-Level Fields

```json
{
  "workflow_name": "example",           /* Required: Template name */
  "description": "What this does",      /* Required: Human description */
  "provider": "claude_code",            /* Optional: Override default provider */
  "model": "claude-sonnet-4-5",         /* Optional: Override default model */
  "steps": [ /* ... */ ]                /* Required: Array of step objects */
}
```

**Provider Configuration Precedence:**
1. Workflow JSON `provider`/`model` fields (highest)
2. `.env.argo.local` environment variables
3. `.env.argo` project defaults
4. Built-in defaults: `claude_code` / `claude-sonnet-4-5` (lowest)

### Step Structure

Every step has these common fields:

```json
{
  "step": "unique_step_name",     /* Required: Step identifier */
  "type": "step_type",            /* Required: See step types below */
  "next_step": "next_step_name",  /* Required: "EXIT" to end workflow */
  /* ... type-specific fields ... */
}
```

## Step Types Reference

### Basic Steps

#### `user_ask` - Prompt User for Input

Asks the user to enter text input.

```json
{
  "step": "get_filename",
  "type": "user_ask",
  "prompt": "Enter filename to process:",
  "save_to": "filename",
  "next_step": "process_file"
}
```

**Fields:**
- `prompt` (required) - Text to show user
- `save_to` (required) - Variable name to store input

**Notes:**
- Only works in interactive mode (not daemon background)
- Input saved to workflow context for use in later steps

---

#### `display` - Show Message

Displays text to the user, with variable substitution.

```json
{
  "step": "show_result",
  "type": "display",
  "message": "Processing {{filename}}...\nResult: {{result}}",
  "next_step": "EXIT"
}
```

**Fields:**
- `message` (required) - Text to display (supports `{{var}}` substitution)

**Variable Substitution:**
```
{{variable_name}}  → Replaced with value from workflow context
```

---

#### `save_file` - Write to File

Saves content to a file on disk.

```json
{
  "step": "save_output",
  "type": "save_file",
  "content": "{{ai_response}}",
  "filename": "/tmp/output.txt",
  "next_step": "EXIT"
}
```

**Fields:**
- `content` (required) - Data to write (supports `{{var}}` substitution)
- `filename` (required) - Path to file (will be created/overwritten)

---

### CI Steps (AI Interaction)

**Note:** Current implementation of `ci_ask` prompts the USER for input (with optional AI-generated conversational prompts), not the CI provider directly. See "Known Issues" section below.

#### `ci_ask` - AI-Assisted User Prompt

Asks user for input, optionally using AI to make the prompt more conversational.

```json
{
  "step": "get_user_goal",
  "type": "ci_ask",
  "prompt_template": "What would you like to accomplish?",
  "save_to": "user_goal",
  "next_step": "analyze_goal"
}
```

**Fields:**
- `prompt_template` (required) - Base prompt text
- `save_to` (required) - Variable name to store user's response

**Behavior:**
- If workflow has a provider: AI generates conversational version of prompt
- If no provider: Uses prompt_template directly
- Reads user input from stdin
- Saves user's answer to context variable

---

#### `ci_analyze` - AI Analysis

Sends content to AI for analysis.

```json
{
  "step": "analyze_code",
  "type": "ci_analyze",
  "content": "{{source_code}}",
  "analysis_type": "review",
  "save_to": "code_review",
  "next_step": "show_review"
}
```

**Fields:**
- `content` (required) - What to analyze
- `analysis_type` (required) - Type of analysis (e.g., "review", "summarize")
- `save_to` (required) - Variable to store AI response

---

#### `ci_ask_series` - Multi-Question Dialogue

Conducts a series of AI questions with user.

```json
{
  "step": "gather_requirements",
  "type": "ci_ask_series",
  "questions": [
    "What is the feature name?",
    "Who is the target user?",
    "What problem does it solve?"
  ],
  "save_to": "requirements",
  "next_step": "design_feature"
}
```

**Fields:**
- `questions` (required) - Array of question strings
- `save_to` (required) - Variable to store collected answers

---

#### `ci_present` - AI Presents Information

AI presents information in a structured way.

```json
{
  "step": "explain_results",
  "type": "ci_present",
  "topic": "test results",
  "data": "{{test_output}}",
  "format": "summary",
  "next_step": "EXIT"
}
```

**Fields:**
- `topic` (required) - What is being presented
- `data` (required) - Information to present
- `format` (required) - Presentation style (e.g., "summary", "detailed")

---

### Branching Steps

#### `decide` - AI Makes Decision

AI evaluates a condition and branches based on result.

```json
{
  "step": "check_complexity",
  "type": "decide",
  "question": "Is this code complex enough to need refactoring?",
  "context": "{{code_metrics}}",
  "on_yes": "refactor_code",
  "on_no": "skip_refactor"
}
```

**Fields:**
- `question` (required) - Decision to make
- `context` (required) - Information for AI to evaluate
- `on_yes` (required) - Step name if AI decides "yes"
- `on_no` (required) - Step name if AI decides "no"

---

#### `user_choose` - User Chooses Option

Presents options to user and branches based on choice.

```json
{
  "step": "choose_action",
  "type": "user_choose",
  "prompt": "What would you like to do?",
  "options": [
    {"label": "Fix bugs", "step": "run_bug_fixes"},
    {"label": "Add feature", "step": "add_feature"},
    {"label": "Refactor", "step": "refactor_code"}
  ],
  "next_step": "EXIT"
}
```

**Fields:**
- `prompt` (required) - Question to ask user
- `options` (required) - Array of `{label, step}` objects
- User selects by number (1, 2, 3...)
- Workflow jumps to chosen step

---

### Advanced Steps

#### `workflow_call` - Call Another Workflow

Invokes another workflow as a sub-workflow.

```json
{
  "step": "run_tests",
  "type": "workflow_call",
  "workflow": "test_suite",
  "parameters": {
    "test_type": "unit",
    "verbose": "true"
  },
  "save_to": "test_results",
  "next_step": "check_results"
}
```

**Fields:**
- `workflow` (required) - Name of workflow template to call
- `parameters` (optional) - Variables to pass to sub-workflow
- `save_to` (optional) - Variable to store sub-workflow results

---

#### `parallel` - Run Steps in Parallel

Executes multiple steps concurrently.

```json
{
  "step": "parallel_review",
  "type": "parallel",
  "steps": ["review_code", "run_tests", "check_docs"],
  "wait_for": "all",
  "next_step": "merge_results"
}
```

**Fields:**
- `steps` (required) - Array of step names to run in parallel
- `wait_for` (required) - "all" or "any" (when to proceed)

---

## Variable Context

### Setting Variables

Variables are set by steps with `save_to` field:

```json
{
  "step": "get_name",
  "type": "user_ask",
  "prompt": "Enter your name:",
  "save_to": "user_name",  /* Creates {{user_name}} variable */
  "next_step": "greet"
}
```

### Using Variables

Use `{{variable}}` syntax in any string field:

```json
{
  "step": "greet",
  "type": "display",
  "message": "Hello, {{user_name}}! Welcome to Argo.",
  "next_step": "EXIT"
}
```

**Variable Substitution Works In:**
- `message` fields
- `content` fields
- `prompt` fields
- `prompt_template` fields
- `filename` fields

### Special Variables

Some variables are automatically available:

- `{{workflow_id}}` - Current workflow instance ID
- `{{workflow_name}}` - Workflow template name
- `{{step_number}}` - Current step index (1-based)

## Complete Examples

### Example 1: Simple Query and Display

```json
{
  "workflow_name": "ask_question",
  "description": "Ask user a question and show response",
  "steps": [
    {
      "step": "ask",
      "type": "user_ask",
      "prompt": "What is your favorite programming language?",
      "save_to": "language",
      "next_step": "respond"
    },
    {
      "step": "respond",
      "type": "display",
      "message": "Great choice! {{language}} is a powerful language.",
      "next_step": "EXIT"
    }
  ]
}
```

### Example 2: Conditional Branch

```json
{
  "workflow_name": "check_and_act",
  "description": "Check condition and branch accordingly",
  "steps": [
    {
      "step": "get_number",
      "type": "user_ask",
      "prompt": "Enter a number:",
      "save_to": "number",
      "next_step": "check_even"
    },
    {
      "step": "check_even",
      "type": "decide",
      "question": "Is {{number}} an even number?",
      "context": "The number is {{number}}",
      "on_yes": "show_even",
      "on_no": "show_odd"
    },
    {
      "step": "show_even",
      "type": "display",
      "message": "{{number}} is even!",
      "next_step": "EXIT"
    },
    {
      "step": "show_odd",
      "type": "display",
      "message": "{{number}} is odd!",
      "next_step": "EXIT"
    }
  ]
}
```

### Example 3: Multi-Step with AI

```json
{
  "workflow_name": "code_review_simple",
  "description": "Get code from user and review it",
  "steps": [
    {
      "step": "get_code",
      "type": "user_ask",
      "prompt": "Paste the code to review:",
      "save_to": "code",
      "next_step": "analyze"
    },
    {
      "step": "analyze",
      "type": "ci_analyze",
      "content": "{{code}}",
      "analysis_type": "code_review",
      "save_to": "review",
      "next_step": "show_results"
    },
    {
      "step": "show_results",
      "type": "display",
      "message": "=== Code Review Results ===\n\n{{review}}",
      "next_step": "ask_save"
    },
    {
      "step": "ask_save",
      "type": "user_choose",
      "prompt": "What would you like to do?",
      "options": [
        {"label": "Save to file", "step": "save_review"},
        {"label": "Exit", "step": "EXIT"}
      ],
      "next_step": "EXIT"
    },
    {
      "step": "save_review",
      "type": "save_file",
      "content": "{{review}}",
      "filename": "/tmp/code_review.txt",
      "next_step": "EXIT"
    }
  ]
}
```

## Workflow Files

### Location

Workflow templates are stored in:
```
workflows/templates/*.json
```

### Naming Convention

- Filename: `workflow_name.json`
- Workflow name in JSON should match filename (without `.json`)
- Use lowercase with underscores: `code_review.json`, `fix_bug.json`

### Template vs Instance

- **Template** - The JSON definition file (e.g., `code_review.json`)
- **Instance** - A running execution of a template (e.g., `code_review_test1`)
- One template can have many instances running concurrently

## Checkpointing & Resuming

### Automatic Checkpointing

Workflows automatically save state after each step completion:
- Current step position
- All context variables
- Execution status

### Checkpoint Files

Located in: `~/.argo/checkpoints/<workflow_id>.json`

### Resuming

```bash
# Workflow paused or interrupted
arc resume code_review_test1

# Continues from last completed step
# Context variables restored automatically
```

## Best Practices

### Step Naming

```bash
# Good - Clear, descriptive
"get_filename", "analyze_code", "show_results"

# Bad - Vague, unclear
"step1", "do_thing", "x"
```

### Error Handling

Always include fallback steps:

```json
{
  "step": "try_operation",
  "type": "ci_analyze",
  "content": "{{data}}",
  "analysis_type": "process",
  "save_to": "result",
  "on_error": "handle_error",  /* Falls back on failure */
  "next_step": "show_result"
}
```

### Variable Names

```bash
# Good - Clear, specific
"user_name", "file_path", "ai_response"

# Bad - Ambiguous
"input", "data", "x"
```

### Workflow Design

**Do:**
- Start simple, add complexity as needed
- One clear purpose per workflow
- Descriptive step names and messages
- Test with small data before production use

**Don't:**
- Create overly complex branching
- Use generic variable names
- Skip error handling
- Forget to set `next_step` (workflow will fail)

## Debugging Workflows

### View Execution Logs

```bash
# Follow live output
arc attach workflow_id

# View complete log
cat ~/.argo/logs/workflow_id.log
```

### Common Issues

**Workflow hangs on step:**
- Check if step requires user input (won't work in daemon mode)
- Verify provider is available (`arc status`)

**Variable not substituted:**
- Check variable was set in previous step with `save_to`
- Verify variable name spelling (case-sensitive)
- Ensure `{{}}` syntax is used

**Step fails with "missing field":**
- Verify all required fields are present for step type
- Check JSON syntax (commas, quotes, brackets)

**Workflow repeats prompt multiple times:**
- This is a known issue with `ci_ask` - see "Known Issues" below

## Known Issues & Limitations

### `ci_ask` Step Behavior

**Current Issue:** The `ci_ask` step type is designed to prompt the USER for input (with optional AI-generated conversational prompts), not to query the CI provider directly.

**What it does:**
1. Optionally sends `prompt_template` to AI to make it more conversational
2. Shows prompt to user
3. Reads user's response from stdin
4. Saves user's answer to context variable

**What it doesn't do:**
- Send query to AI and get AI's response directly

**Workaround:** Currently under investigation. A new step type may be added for direct CI queries.

### Background Execution Limitations

Steps requiring stdin (`user_ask`, `ci_ask`, `user_choose`) don't work when workflow runs as daemon background process. These steps require interactive terminal.

## Advanced Topics

### Custom Personas (Future)

Workflows can define AI personas with specific characteristics:

```json
{
  "persona": {
    "name": "CodeReviewer",
    "role": "Senior Software Engineer",
    "style": "Direct, technical, focused on best practices"
  },
  "steps": [ /* ... */ ]
}
```

### Workflow Composition

Complex workflows can call other workflows:

```json
{
  "step": "run_full_test_suite",
  "type": "workflow_call",
  "workflow": "test_runner",
  "parameters": {
    "suite": "all",
    "parallel": "true"
  },
  "save_to": "test_results",
  "next_step": "evaluate_results"
}
```

## See Also

- **DAEMON_ARCHITECTURE.md** - How workflows execute in daemon
- **CONTRIBUTING.md** - Adding new step types
- **examples/** - More workflow templates

---

*Argo workflows make complex multi-step AI interactions simple, repeatable, and maintainable.*
