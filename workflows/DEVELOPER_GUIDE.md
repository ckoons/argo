# Argo Workflows Developer Guide

© 2025 Casey Koons. All rights reserved.

This guide is for developers who want to create new phase handlers or extend the Argo workflow system.

## Table of Contents

1. [Phase Handler Basics](#phase-handler-basics)
2. [Handler Contract](#handler-contract)
3. [Handler Template](#handler-template)
4. [State Management](#state-management)
5. [Logging](#logging)
6. [Git Worktree Operations](#git-worktree-operations)
7. [Testing Phase Handlers](#testing-phase-handlers)
8. [Best Practices](#best-practices)
9. [Examples](#examples)

## Phase Handler Basics

### What is a Phase Handler?

A phase handler is an executable bash script that implements a specific step in the workflow. Handlers are stored in the `phases/` directory and are invoked by the orchestrator when conditions are met.

### Handler Naming

**Convention**: `<action_name>.sh`

Examples:
- `design_program.sh` - Initial design phase
- `design_build.sh` - Builder creation phase
- `execution.sh` - Parallel builder execution phase
- `merge_build.sh` - Branch merging phase

### Handler Selection

The orchestrator selects which handler to run based on the `action_needed` field in state.json:

```bash
# From orchestrator.sh
action_needed=$(read_state "action_needed")
handler_name="${action_needed:-$phase}"  # Fallback to phase name

phase_handler="$WORKFLOWS_DIR/phases/${handler_name}.sh"
if [[ -f "$phase_handler" ]] && [[ -x "$phase_handler" ]]; then
    "$phase_handler" "$project_path"
fi
```

**Priority**: `action_needed` field takes precedence over `phase` name.

## Handler Contract

Every phase handler MUST:

1. **Accept project path** as first argument (`$1`)
2. **Validate inputs** (project directory, state file, required state data)
3. **Perform work** (the handler's purpose)
4. **Update state** (phase, action_owner, action_needed, results)
5. **Return exit code** (0=success, non-zero=failure)
6. **Be idempotent** (safe to re-run without side effects)

### Required Elements

```bash
#!/bin/bash
# © 2025 Casey Koons All rights reserved

# Get script directory
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source library modules
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"

# Main function
main() {
    local project_path="$1"

    # 1. Validate inputs
    # 2. Read state
    # 3. Perform work
    # 4. Update state
    # 5. Return exit code
}

main "$@"
exit $?
```

### Input Validation

Every handler must validate:

```bash
# Validate project path argument
if [[ -z "$project_path" ]]; then
    echo "ERROR: Project path required" >&2
    return 1
fi

# Validate .argo-project directory exists
if [[ ! -d "$project_path/.argo-project" ]]; then
    echo "ERROR: .argo-project not found in $project_path" >&2
    return 1
fi

# Change to project directory with error checking
cd "$project_path" || {
    echo "ERROR: Failed to change to project directory" >&2
    return 1
}
```

### State Updates

Every handler must update state to indicate completion:

```bash
# Minimal state updates
update_state "phase" "next_phase"
update_state "action_owner" "code"  # or "ci" or "user"

# With action_needed for explicit handler selection
update_state "action_needed" "next_handler_name"
```

### Exit Codes

```bash
# Success
return 0

# Failure (generic)
return 1

# Specific error codes (optional)
return 2  # Configuration error
return 3  # Validation error
return 4  # External tool failure
```

## Handler Template

### Complete Template

```bash
#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# my_phase.sh - Brief description of what this phase does
#
# Detailed explanation of the phase:
# - What it reads from state
# - What operations it performs
# - What it writes to state
# - What phase it transitions to

# Get script directory (readonly constant)
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source library modules
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"

# CI tool command (allow override for testing)
readonly CI_TOOL="${CI_COMMAND:-ci}"

#
# helper_function - Helper functions use this format
#
# Args:
#   $1 - description of first argument
#   $2 - description of second argument
#
# Returns:
#   0 on success, non-zero on failure
#
helper_function() {
    local arg1="$1"
    local arg2="$2"

    # Implementation here

    return 0
}

#
# Main function
#
main() {
    local project_path="$1"

    # ========================================
    # 1. VALIDATE INPUTS
    # ========================================

    if [[ -z "$project_path" ]]; then
        echo "ERROR: Project path required" >&2
        return 1
    fi

    if [[ ! -d "$project_path/.argo-project" ]]; then
        echo "ERROR: .argo-project not found in $project_path" >&2
        return 1
    fi

    cd "$project_path" || {
        echo "ERROR: Failed to change to project directory" >&2
        return 1
    }

    # ========================================
    # 2. READ STATE
    # ========================================

    echo "=== my_phase phase ==="

    # Read required data from state
    local required_data=$(read_state "some.field")
    if [[ -z "$required_data" ]]; then
        echo "ERROR: Missing required data in state" >&2
        return 1
    fi

    # Read optional data with defaults
    local optional_data=$(read_state "optional.field")
    optional_data="${optional_data:-default_value}"

    # ========================================
    # 3. PERFORM WORK
    # ========================================

    echo "Performing work..."

    # Call helper functions
    if ! helper_function "$required_data" "$optional_data"; then
        echo "ERROR: Helper function failed" >&2
        return 1
    fi

    # ========================================
    # 4. UPDATE STATE
    # ========================================

    # Log phase transition
    log_state_transition "my_phase" "next_phase"

    # Update state
    update_state "phase" "next_phase"
    update_state "action_owner" "code"
    update_state "action_needed" "next_handler"

    # Update phase-specific state data
    update_state "my_phase.status" "completed"
    update_state "my_phase.result" "success"

    echo "my_phase phase complete"
    echo ""

    # ========================================
    # 5. RETURN SUCCESS
    # ========================================

    return 0
}

# Run main function
main "$@"
exit $?
```

### Template Checklist

- [ ] Copyright header
- [ ] Script description comment
- [ ] `readonly SCRIPT_DIR` declaration
- [ ] Source state_file.sh and logging_enhanced.sh
- [ ] `readonly CI_TOOL` if needed
- [ ] Helper functions with comments
- [ ] Main function with 5 sections
- [ ] Input validation with error messages
- [ ] State reading with null checks
- [ ] Work implementation
- [ ] State updates (phase, action_owner, action_needed)
- [ ] Return exit code
- [ ] `main "$@"` and `exit $?` at bottom

## State Management

### Reading State

Use the `read_state` function from `lib/state_file.sh`:

```bash
# Simple field
phase=$(read_state "phase")

# Nested field (dot notation)
builder_status=$(read_state "execution.builders[0].status")

# With fallback
model=$(read_state "design.model" "default-model")
```

### Updating State

Use the `update_state` function:

```bash
# Simple update
update_state "phase" "execution"

# Nested update
update_state "execution.status" "running"

# Array element (requires jq)
local temp_file=".argo-project/state.json.tmp"
jq --arg id "$builder_id" \
   --arg status "completed" \
   '(.execution.builders[] | select(.id == $id)) |= (.status = $status)' \
   .argo-project/state.json > "$temp_file" && mv "$temp_file" .argo-project/state.json
```

### Complex State Updates

For complex updates, use jq directly:

```bash
# Add item to array
local temp_file=".argo-project/state.json.tmp"
jq --argjson builder "$builder_json" \
   '.execution.builders += [$builder]' \
   .argo-project/state.json > "$temp_file" && mv "$temp_file" .argo-project/state.json

# Update multiple fields atomically
jq --arg phase "execution" \
   --arg owner "code" \
   --arg action "spawn_builders" \
   '.phase = $phase | .action_owner = $owner | .action_needed = $action' \
   .argo-project/state.json > "$temp_file" && mv "$temp_file" .argo-project/state.json
```

### State File Best Practices

1. **Always use temp file pattern**: Write to `.tmp` file, then `mv` for atomicity
2. **Validate before update**: Check that state changes are valid
3. **Update related fields together**: Use single jq command for related changes
4. **Don't read and write in loops**: Batch updates when possible
5. **Add timestamps**: Include `$(date -u +"%Y-%m-%dT%H:%M:%SZ")` for audit trail

## Logging

### Log Levels

Use functions from `lib/logging_enhanced.sh`:

```bash
# State transitions
log_state_transition "old_phase" "new_phase"

# Builder events
log_builder "builder_id" "spawned" "Additional details"
log_builder "builder_id" "completed" "Finished successfully"
log_builder "builder_id" "failed" "Error details"

# Merge operations
log_merge "success" "Merged branch argo-builder-api"
log_merge "conflict" "Conflict in src/index.js"
```

### Log Format

Logs are written to `.argo-project/logs/workflow.log`:

```
[2025-11-03T10:05:23Z] [STATE] init → design
[2025-11-03T10:05:24Z] [BUILDER] api: spawned (PID 12345)
[2025-11-03T10:08:15Z] [BUILDER] api: completed
[2025-11-03T10:08:20Z] [MERGE] Merging branch argo-builder-api
```

### Custom Log Entries

```bash
# Direct log writing
echo "[$(date -u +"%Y-%m-%dT%H:%M:%SZ")] [CUSTOM] Your message" \
    >> .argo-project/logs/workflow.log
```

## Git Worktree Operations

### Creating Worktrees

```bash
# Create worktree with new branch
local worktree_path=".argo-project/builders/$builder_id"
local branch="argo-builder-$builder_id"

if git worktree add "$worktree_path" -b "$branch" >/dev/null 2>&1; then
    log_builder "$builder_id" "worktree_created" "Path: $worktree_path"
else
    log_builder "$builder_id" "worktree_failed" "Could not create worktree"
    return 1
fi
```

### Checking Worktree Status

```bash
# List all worktrees
git worktree list

# Check if worktree exists
if [[ -d "$worktree_path" ]]; then
    echo "Worktree exists"
fi

# Check if branch exists
if git rev-parse --verify "$branch" >/dev/null 2>&1; then
    echo "Branch exists"
fi
```

### Removing Worktrees

```bash
# Remove worktree (force to ignore untracked files)
if [[ -d "$worktree_path" ]]; then
    git worktree remove "$worktree_path" --force 2>/dev/null || {
        # Fallback: manual deletion
        rm -rf "$worktree_path" 2>/dev/null
    }
fi

# Delete branch
if git rev-parse --verify "$branch" >/dev/null 2>&1; then
    git branch -D "$branch" >/dev/null 2>&1
fi
```

### Worktree Best Practices

1. **Always use .argo-project/builders/** as parent directory
2. **Use consistent naming**: `argo-builder-<component-id>`
3. **Clean up after merge**: Remove worktrees and branches
4. **Check for existing worktrees** before creating
5. **Use --force when removing** to handle uncommitted changes

## Testing Phase Handlers

### Test Structure

Create test file: `tests/test_my_phase.sh`

```bash
#!/bin/bash

# Source test utilities
source "$(dirname "$0")/test_utils.sh"

# Test counter
test_count=0
passed_count=0

#
# Test: Handler accepts project path
#
test_handler_accepts_project_path() {
    test_count=$((test_count + 1))

    local test_dir=$(mktemp -d)
    setup_test_project "$test_dir"

    # Run handler
    ../phases/my_phase.sh "$test_dir" >/dev/null 2>&1
    local result=$?

    cleanup_test_project "$test_dir"

    if [[ $result -eq 0 ]]; then
        passed_count=$((passed_count + 1))
        echo "PASS: Handler accepts project path"
        return 0
    else
        echo "FAIL: Handler did not accept project path"
        return 1
    fi
}

#
# Test: Handler fails without project path
#
test_handler_fails_without_project_path() {
    test_count=$((test_count + 1))

    # Run handler without argument
    ../phases/my_phase.sh >/dev/null 2>&1
    local result=$?

    if [[ $result -ne 0 ]]; then
        passed_count=$((passed_count + 1))
        echo "PASS: Handler fails without project path"
        return 0
    else
        echo "FAIL: Handler should fail without project path"
        return 1
    fi
}

#
# Test: Handler updates state correctly
#
test_handler_updates_state() {
    test_count=$((test_count + 1))

    local test_dir=$(mktemp -d)
    setup_test_project "$test_dir"

    # Run handler
    ../phases/my_phase.sh "$test_dir" >/dev/null 2>&1

    # Check state updates
    local phase=$(jq -r '.phase' "$test_dir/.argo-project/state.json")
    local action_owner=$(jq -r '.action_owner' "$test_dir/.argo-project/state.json")

    cleanup_test_project "$test_dir"

    if [[ "$phase" == "expected_phase" ]] && [[ "$action_owner" == "code" ]]; then
        passed_count=$((passed_count + 1))
        echo "PASS: Handler updates state correctly"
        return 0
    else
        echo "FAIL: Handler did not update state correctly (phase=$phase, owner=$action_owner)"
        return 1
    fi
}

# Run all tests
echo "Running my_phase tests..."
test_handler_accepts_project_path
test_handler_fails_without_project_path
test_handler_updates_state

# Report results
echo ""
echo "Results: $passed_count/$test_count tests passed"
if [[ $passed_count -eq $test_count ]]; then
    exit 0
else
    exit 1
fi
```

### Test Utilities

Common test utilities in `tests/test_utils.sh`:

```bash
# Setup test project
setup_test_project() {
    local test_dir="$1"

    mkdir -p "$test_dir/.argo-project/logs"

    # Create minimal state file
    cat > "$test_dir/.argo-project/state.json" <<EOF
{
    "project_name": "test-project",
    "phase": "init",
    "action_owner": "code",
    "action_needed": "my_phase"
}
EOF

    # Initialize git repo
    (
        cd "$test_dir"
        git init >/dev/null 2>&1
        git config user.email "test@example.com"
        git config user.name "Test User"
        echo "test" > README.md
        git add README.md
        git commit -m "Initial commit" >/dev/null 2>&1
    )
}

# Cleanup test project
cleanup_test_project() {
    local test_dir="$1"
    rm -rf "$test_dir"
}

# Mock CI tool
mock_ci_tool() {
    local response="$1"

    # Create mock CI script
    cat > /tmp/mock_ci.sh <<EOF
#!/bin/bash
echo "$response"
exit 0
EOF

    chmod +x /tmp/mock_ci.sh
    export CI_COMMAND="/tmp/mock_ci.sh"
}
```

### Running Tests

```bash
# Run single test file
./tests/test_my_phase.sh

# Run all phase handler tests
./tests/run_phase2_tests.sh

# Run with verbose output
bash -x ./tests/test_my_phase.sh
```

## Best Practices

### Handler Design

1. **Single Responsibility**: Each handler does one thing well
2. **Idempotent**: Safe to re-run without side effects
3. **Fail Fast**: Validate inputs early, fail with clear error messages
4. **Atomic State Updates**: Update related state fields together
5. **Log Everything**: State transitions, errors, important events

### Error Handling

```bash
# Check every command that can fail
if ! some_command; then
    echo "ERROR: Command failed" >&2
    return 1
fi

# Use || for inline error handling
cd "$project_path" || {
    echo "ERROR: Failed to change directory" >&2
    return 1
}

# Capture and check exit codes
local result
result=$(command_that_might_fail 2>&1)
if [[ $? -ne 0 ]]; then
    echo "ERROR: $result" >&2
    return 1
fi
```

### State Management

1. **Read once, write once**: Minimize state file operations
2. **Validate state data**: Check for null/empty before using
3. **Use transactions**: Multiple related updates in one jq call
4. **Add timestamps**: Include created/completed timestamps
5. **Preserve history**: Don't delete data, mark as completed/archived

### Git Operations

1. **Check before creating**: Verify branches/worktrees don't exist
2. **Redirect output**: Use `>/dev/null 2>&1` for clean console output
3. **Handle failures gracefully**: Provide fallbacks (e.g., manual rm)
4. **Clean up**: Always remove worktrees and branches after merge

### Testing

1. **Test happy path**: Normal operation should pass
2. **Test error cases**: Missing inputs, invalid state, etc.
3. **Test idempotency**: Re-running should be safe
4. **Mock external tools**: Don't depend on real CI during tests
5. **Clean up**: Remove test directories, kill test processes

## Examples

### Example 1: Simple Validation Handler

```bash
#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# validate.sh - Validates workflow prerequisites

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"

main() {
    local project_path="$1"

    if [[ -z "$project_path" ]]; then
        echo "ERROR: Project path required" >&2
        return 1
    fi

    cd "$project_path" || return 1

    echo "=== validate phase ==="

    # Check for required tools
    if ! command -v git >/dev/null 2>&1; then
        echo "ERROR: git not found" >&2
        return 1
    fi

    if ! command -v jq >/dev/null 2>&1; then
        echo "ERROR: jq not found" >&2
        return 1
    fi

    # All checks passed
    update_state "phase" "design"
    update_state "action_owner" "code"

    echo "validate phase complete"
    return 0
}

main "$@"
exit $?
```

### Example 2: Data Collection Handler

```bash
#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# collect_metrics.sh - Collects build metrics from builders

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"

main() {
    local project_path="$1"

    if [[ -z "$project_path" ]]; then
        echo "ERROR: Project path required" >&2
        return 1
    fi

    cd "$project_path" || return 1

    echo "=== collect_metrics phase ==="

    # Read builders from state
    local builders=$(jq -r '.execution.builders' .argo-project/state.json)
    local builder_count=$(echo "$builders" | jq 'length')

    # Collect metrics for each builder
    local total_duration=0
    for ((i=0; i<builder_count; i++)); do
        local builder=$(echo "$builders" | jq -r ".[$i]")
        local builder_id=$(echo "$builder" | jq -r '.id')
        local started=$(echo "$builder" | jq -r '.started')
        local completed=$(echo "$builder" | jq -r '.completed')

        # Calculate duration (simplified - real code would parse timestamps)
        echo "Builder $builder_id: $started to $completed"
    done

    # Save metrics to state
    update_state "metrics.collected" "true"
    update_state "metrics.builder_count" "$builder_count"

    # Transition to next phase
    update_state "phase" "report"
    update_state "action_owner" "code"

    echo "collect_metrics phase complete"
    return 0
}

main "$@"
exit $?
```

### Example 3: Conditional Branching Handler

```bash
#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# decide_strategy.sh - Decides which strategy to use based on state

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"

main() {
    local project_path="$1"

    if [[ -z "$project_path" ]]; then
        echo "ERROR: Project path required" >&2
        return 1
    fi

    cd "$project_path" || return 1

    echo "=== decide_strategy phase ==="

    # Read decision criteria from state
    local component_count=$(jq -r '.design.components | length' .argo-project/state.json)

    local next_action
    if [[ $component_count -eq 1 ]]; then
        echo "Single component - using simple strategy"
        next_action="simple_build"
    elif [[ $component_count -le 5 ]]; then
        echo "Few components - using parallel strategy"
        next_action="parallel_build"
    else
        echo "Many components - using batch strategy"
        next_action="batch_build"
    fi

    # Update state with decision
    update_state "phase" "execution"
    update_state "action_owner" "code"
    update_state "action_needed" "$next_action"

    echo "decide_strategy phase complete: chose $next_action"
    return 0
}

main "$@"
exit $?
```

---

## Additional Resources

- [ARCHITECTURE.md](ARCHITECTURE.md) - Deep technical documentation
- [USER_GUIDE.md](USER_GUIDE.md) - User-facing documentation
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Common issues and solutions
- `lib/state_file.sh` - State management utilities
- `lib/logging_enhanced.sh` - Logging utilities
- `tests/test_*.sh` - Example test files

For questions or contributions, examine existing phase handlers in `phases/` for working examples.
