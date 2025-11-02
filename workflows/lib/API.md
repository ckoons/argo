# Argo Workflows API Reference

© 2025 Casey Koons. All rights reserved.

**Version:** Sprint 2 - State-File Relay Architecture
**Last Updated:** 2025-01-15

This document defines all function signatures and data structures for the Argo workflows system. Functions are organized by module.

---

## Table of Contents

1. [State File Operations](#state-file-operations) - state_file.sh
2. [Project Registry](#project-registry) - project_registry.sh
3. [Enhanced Logging](#enhanced-logging) - logging_enhanced.sh
4. [Checkpoint System](#checkpoint-system) - checkpoint.sh
5. [Templates](#templates) - project_setup.sh, project_cleanup.sh
6. [Data Structures](#data-structures)
7. [Constants](#constants)

---

## State File Operations

**Module:** `workflows/lib/state_file.sh`

Functions for atomic state file operations. All state modifications go through these functions to ensure consistency and enable concurrent access.

### update_state(key, value)

Updates a field in state.json atomically.

**Arguments:**
- `key` (string): JSON path to field (e.g., "phase" or "execution.builders[0].status")
- `value` (string): Value to set

**Returns:**
- 0 on success
- 1 on failure

**Side Effects:**
- Writes state.json.tmp
- Renames to state.json (atomic)

**Example:**
```bash
update_state "phase" "execution"
update_state "execution.builders[0].progress" "50"
```

### read_state(key)

Reads a value from state.json.

**Arguments:**
- `key` (string): JSON path to field

**Returns:**
- Value to stdout
- Exit code 0 on success, 1 if field not found or error

**Side Effects:** None (read-only)

**Example:**
```bash
phase=$(read_state "phase")
progress=$(read_state "execution.builders[0].progress")
```

### with_state_lock(command, args...)

Executes a command with exclusive lock on state file.

**Arguments:**
- `command` (string): Command to execute
- `args...` (strings): Command arguments

**Returns:**
- Command's exit code

**Side Effects:**
- Creates/uses .argo-project/state.lock
- Holds exclusive lock during execution

**Example:**
```bash
with_state_lock update_complex_state "builder-1" "completed"
```

### init_state_file(project_name, project_path)

Creates initial state.json with default structure.

**Arguments:**
- `project_name` (string): Name of project
- `project_path` (string): Path to project directory (default: ".")

**Returns:**
- 0 on success
- 1 on failure

**Side Effects:**
- Creates .argo-project/state.json
- Sets default limits and empty structures

**Example:**
```bash
init_state_file "calculator" "/path/to/project"
```

### validate_state_file(state_file_path)

Validates state.json is well-formed JSON with required fields.

**Arguments:**
- `state_file_path` (string): Path to state file (default: ".argo-project/state.json")

**Returns:**
- 0 if valid
- 1 if invalid

**Side Effects:** None

**Example:**
```bash
if validate_state_file; then
    echo "State file is valid"
fi
```

---

## Project Registry

**Module:** `workflows/lib/project_registry.sh`

Functions for managing the global registry of active projects in ~/.argo/projects.json.

### register_project(name, path)

Registers a new project in the global registry.

**Arguments:**
- `name` (string): Project name
- `path` (string): Absolute path to project directory

**Returns:**
- Project ID to stdout (e.g., "calculator-1736960400")
- Exit code 0 on success, 1 on failure

**Side Effects:**
- Creates/updates ~/.argo/projects.json
- Adds project entry with generated ID

**Example:**
```bash
project_id=$(register_project "calculator" "/Users/casey/projects/calc")
```

### deregister_project(project_id)

Removes a project from the global registry.

**Arguments:**
- `project_id` (string): Project ID (from register_project)

**Returns:**
- 0 on success
- 1 if project not found

**Side Effects:**
- Updates ~/.argo/projects.json
- Removes project entry

**Example:**
```bash
deregister_project "calculator-1736960400"
```

### list_projects()

Lists all registered projects.

**Arguments:** None

**Returns:**
- Tab-separated lines: `project_id\tproject_name\tstatus\tlast_active`
- Exit code 0

**Side Effects:** None (read-only)

**Example:**
```bash
list_projects
# calculator-123  calculator  building  2025-01-15T14:30:00Z
# webapp-456      webapp      idle      2025-01-14T10:00:00Z
```

### get_project_info(project_id)

Gets detailed information about a project.

**Arguments:**
- `project_id` (string): Project ID

**Returns:**
- JSON object to stdout
- Exit code 0 on success, 1 if not found

**Side Effects:** None (read-only)

**Example:**
```bash
info=$(get_project_info "calculator-123")
echo "$info" | jq -r '.path'
```

### update_project_status(project_id, status)

Updates the status of a project.

**Arguments:**
- `project_id` (string): Project ID
- `status` (string): New status (e.g., "building", "idle", "failed")

**Returns:**
- 0 on success
- 1 if project not found

**Side Effects:**
- Updates ~/.argo/projects.json
- Updates last_active timestamp

**Example:**
```bash
update_project_status "calculator-123" "building"
```

### find_project_by_path(project_path)

Finds a project ID by its path.

**Arguments:**
- `project_path` (string): Project directory path

**Returns:**
- Project ID to stdout if found
- Exit code 0 if found, 1 if not found

**Side Effects:** None (read-only)

**Example:**
```bash
project_id=$(find_project_by_path "/Users/casey/projects/calc")
```

---

## Enhanced Logging

**Module:** `workflows/lib/logging_enhanced.sh`

Functions for workflow logging with semantic tags. All logging functions write to both logs and stdout for live monitoring.

### progress(builder_id, percent, message)

Logs progress update (triple output: screen, log, state).

**Arguments:**
- `builder_id` (string): Builder identifier (e.g., "backend", "frontend")
- `percent` (int): Progress percentage (0-100)
- `message` (string): Human-readable status message

**Returns:** 0 always

**Side Effects:**
- Writes to .argo-project/logs/workflow.log
- Updates state.json (execution.builders[].progress)
- Prints to stdout (overwriting line if terminal)

**Format:** `[TIMESTAMP] [PROGRESS] [builder_id] percent% - message`

**Example:**
```bash
progress "backend" 0 "Starting build"
progress "backend" 50 "Running tests"
progress "backend" 100 "Complete"
```

### log_checkpoint(name, description)

Logs checkpoint save event.

**Arguments:**
- `name` (string): Checkpoint name
- `description` (string): Human-readable description

**Returns:** 0 always

**Side Effects:**
- Appends to .argo-project/logs/workflow.log

**Format:** `[TIMESTAMP] [CHECKPOINT] Saved: name - description`

**Example:**
```bash
log_checkpoint "requirements_complete" "User approved requirements"
```

### log_user_query(question)

Logs user query event.

**Arguments:**
- `question` (string): Question posed to user

**Returns:** 0 always

**Side Effects:**
- Appends to .argo-project/logs/workflow.log

**Format:** `[TIMESTAMP] [USER_QUERY] question`

**Example:**
```bash
log_user_query "Approve this build plan?"
```

### log_builder(builder_id, event, message)

Logs builder-specific event.

**Arguments:**
- `builder_id` (string): Builder identifier
- `event` (string): Event type (e.g., "started", "completed", "failed")
- `message` (string): Event details

**Returns:** 0 always

**Side Effects:**
- Appends to .argo-project/logs/workflow.log
- Appends to .argo-project/logs/builder-{builder_id}.log

**Format:** `[TIMESTAMP] [BUILDER] [builder_id] event - message`

**Example:**
```bash
log_builder "backend" "started" "PID: 12345"
log_builder "backend" "completed" "Exit code: 0"
```

### log_merge(event, message)

Logs merge operation event.

**Arguments:**
- `event` (string): Event type (e.g., "started", "conflict", "completed")
- `message` (string): Event details

**Returns:** 0 always

**Side Effects:**
- Appends to .argo-project/logs/workflow.log

**Format:** `[TIMESTAMP] [MERGE] event - message`

**Example:**
```bash
log_merge "started" "Merging backend into main"
log_merge "conflict" "Conflict in user.py"
log_merge "completed" "All merges successful"
```

### log_state_transition(from_phase, to_phase)

Logs workflow phase transition.

**Arguments:**
- `from_phase` (string): Previous phase
- `to_phase` (string): New phase

**Returns:** 0 always

**Side Effects:**
- Appends to .argo-project/logs/workflow.log

**Format:** `[TIMESTAMP] [STATE] Phase transition: from_phase -> to_phase`

**Example:**
```bash
log_state_transition "design" "execution"
```

---

## Checkpoint System

**Module:** `workflows/lib/checkpoint.sh`

Functions for saving and restoring workflow state via checkpoints.

### save_checkpoint(name, description)

Saves current state as a named checkpoint.

**Arguments:**
- `name` (string): Checkpoint identifier (short, no spaces)
- `description` (string): Human-readable description

**Returns:**
- 0 on success
- 1 on failure

**Side Effects:**
- Creates .argo-project/checkpoints/{timestamp}_{name}.json
- Updates state.json checkpoints object
- Calls log_checkpoint()

**Example:**
```bash
save_checkpoint "requirements_complete" "User approved requirements"
save_checkpoint "before_merge" "All builders complete, starting merge"
```

### restore_checkpoint(name)

Restores state from a named checkpoint.

**Arguments:**
- `name` (string): Checkpoint identifier

**Returns:**
- 0 on success
- 1 if checkpoint not found

**Side Effects:**
- Overwrites .argo-project/state.json
- Logs restoration event

**Example:**
```bash
restore_checkpoint "requirements_complete"
```

### list_checkpoints()

Lists all available checkpoints for current project.

**Arguments:** None

**Returns:**
- Tab-separated lines: `name\ttimestamp\tdescription`
- Exit code 0

**Side Effects:** None (read-only)

**Example:**
```bash
list_checkpoints
# requirements_complete  2025-01-15T10:20:30Z  User approved requirements
# decomposition_approved 2025-01-15T10:30:45Z  User approved build plan
```

### delete_checkpoint(name)

Deletes a specific checkpoint.

**Arguments:**
- `name` (string): Checkpoint identifier

**Returns:**
- 0 on success
- 1 if checkpoint not found

**Side Effects:**
- Deletes checkpoint file
- Updates state.json checkpoints object

**Example:**
```bash
delete_checkpoint "old_checkpoint"
```

### cleanup_old_checkpoints(max_age_days)

Deletes checkpoints older than specified age.

**Arguments:**
- `max_age_days` (int): Maximum age in days (default: 30 from limits)

**Returns:**
- Number of checkpoints deleted to stdout
- Exit code 0

**Side Effects:**
- Deletes old checkpoint files
- Updates state.json

**Example:**
```bash
deleted_count=$(cleanup_old_checkpoints 7)
echo "Deleted $deleted_count old checkpoints"
```

---

## Templates

Templates are executable scripts for project lifecycle management.

### project_setup.sh

**Purpose:** Initialize .argo-project structure for a new project.

**Usage:** `./workflows/templates/project_setup.sh <project_name> [project_path]`

**Arguments:**
- `project_name` (string): Name of project
- `project_path` (string): Path to project directory (default: current directory)

**Returns:**
- 0 on success
- 1 on failure

**Side Effects:**
- Creates .argo-project/ directory structure
- Creates state.json with initial values
- Creates config.json
- Adds .argo-project/ to .gitignore
- Registers project in ~/.argo/projects.json

**Example:**
```bash
./workflows/templates/project_setup.sh calculator /Users/casey/projects/calc
```

### project_cleanup.sh

**Purpose:** Cleanup build artifacts and workspace.

**Usage:** `./workflows/templates/project_cleanup.sh [project_path] [cleanup_type]`

**Arguments:**
- `project_path` (string): Path to project directory (default: current directory)
- `cleanup_type` (string): Type of cleanup (default: "partial")
  - "partial": Delete builders/, temp files, temp branches (keep logs/checkpoints)
  - "full": Delete builders/, logs/ (keep checkpoints)
  - "nuclear": Delete entire .argo-project/

**Returns:**
- 0 on success
- 1 on failure

**Side Effects:**
- Removes git worktrees
- Deletes specified files/directories
- Deletes git branches matching patterns

**Example:**
```bash
./workflows/templates/project_cleanup.sh . partial
./workflows/templates/project_cleanup.sh /path/to/project full
```

---

## Data Structures

### state.json (root)

**Location:** `.argo-project/state.json`

```json
{
  "project_name": string,
  "phase": enum["init", "design", "design_build", "execution", "merge", "storage"],
  "action_owner": enum["code", "ci"],
  "action_needed": string,
  "created": ISO8601,
  "last_update": ISO8601,
  "last_checkpoint": string | null,
  "limits": limits_t,
  "design": design_t | null,
  "execution": execution_t | null,
  "merge": merge_t | null,
  "error_recovery": error_recovery_t,
  "user_query": user_query_t | null,
  "checkpoints": map<string, checkpoint_ref_t>
}
```

### limits_t

```json
{
  "max_parallel_builders": int (default: 10),
  "max_builder_retries": int (default: 5),
  "max_workflow_hours": int (default: 48),
  "checkpoint_retention_days": int (default: 30)
}
```

### design_t

```json
{
  "conversation_history": array<message_t>,
  "requirements": string,
  "decomposition": decomposition_t | null
}
```

### message_t

```json
{
  "role": enum["user", "assistant"],
  "content": string,
  "timestamp": ISO8601
}
```

### decomposition_t

```json
{
  "strategy": enum["single", "parallel"],
  "complexity_score": float (0-10),
  "rationale": string,
  "warnings": array<string>,
  "estimated_time_parallel": string,
  "estimated_time_sequential": string,
  "builders": array<builder_plan_t>,
  "merge_order": array<string>,
  "approved": boolean
}
```

### builder_plan_t

```json
{
  "id": string,
  "prompt": string,
  "directories": array<string>,
  "branch": string (e.g., "sub/backend"),
  "provider": string (default: "claude_code"),
  "model": string (default: "claude-sonnet-4-5"),
  "estimated_hours": float,
  "risk_level": enum["low", "medium", "high"]
}
```

### execution_t

```json
{
  "builders": array<builder_state_t>
}
```

### builder_state_t

```json
{
  "id": string,
  "pid": int | null,
  "status": enum["pending", "running", "completed", "failed"],
  "progress": int (0-100),
  "last_update": ISO8601,
  "exit_code": int | null,
  "tests": test_results_t | null
}
```

### test_results_t

```json
{
  "existing_passed": int,
  "existing_failed": int,
  "generated_count": int,
  "generated_passed": int
}
```

### merge_t

```json
{
  "status": enum["pending", "in_progress", "completed", "failed"],
  "completed": array<string>,
  "current": string | null,
  "remaining": array<string>,
  "conflicts_encountered": int,
  "rebuilds_triggered": int
}
```

### error_recovery_t

```json
{
  "last_error": error_detail_t | null,
  "retry_count": int,
  "recovery_action": enum["retry", "pause_for_user", "abort"] | null
}
```

### error_detail_t

```json
{
  "code": string (e.g., "E_BUILDER_RETRY_EXHAUSTED"),
  "message": string,
  "timestamp": ISO8601,
  "builder_id": string | null
}
```

### user_query_t

```json
{
  "question": string,
  "options": array<string>,
  "response": string | null,
  "timeout_at": ISO8601,
  "status": enum["awaiting_response", "timed_out", "answered"]
}
```

### checkpoint_ref_t

```json
{
  "file": string (relative path),
  "description": string,
  "timestamp": ISO8601
}
```

### projects.json (global registry)

**Location:** `~/.argo/projects.json`

```json
{
  "projects": {
    "<project_id>": {
      "name": string,
      "path": string (absolute path),
      "argo_project_dir": string (absolute path to .argo-project),
      "status": enum["active", "idle", "building", "failed"],
      "current_phase": string,
      "created": ISO8601,
      "last_active": ISO8601
    }
  }
}
```

---

## Constants

### Phase Names

```bash
PHASE_INIT="init"
PHASE_DESIGN="design"
PHASE_DESIGN_BUILD="design_build"
PHASE_EXECUTION="execution"
PHASE_MERGE="merge"
PHASE_STORAGE="storage"
```

### Action Owners

```bash
OWNER_CODE="code"
OWNER_CI="ci"
```

### Builder Status

```bash
STATUS_PENDING="pending"
STATUS_RUNNING="running"
STATUS_COMPLETED="completed"
STATUS_FAILED="failed"
```

### Merge Status

```bash
MERGE_PENDING="pending"
MERGE_IN_PROGRESS="in_progress"
MERGE_COMPLETED="completed"
MERGE_FAILED="failed"
```

### Project Status

```bash
PROJECT_ACTIVE="active"
PROJECT_IDLE="idle"
PROJECT_BUILDING="building"
PROJECT_FAILED="failed"
```

### Log Tags

```bash
TAG_INFO="[INFO]"
TAG_WARN="[WARN]"
TAG_ERROR="[ERROR]"
TAG_PROGRESS="[PROGRESS]"
TAG_CHECKPOINT="[CHECKPOINT]"
TAG_USER_QUERY="[USER_QUERY]"
TAG_BUILDER="[BUILDER]"
TAG_MERGE="[MERGE]"
TAG_STATE="[STATE]"
```

### Cleanup Types

```bash
CLEANUP_PARTIAL="partial"
CLEANUP_FULL="full"
CLEANUP_NUCLEAR="nuclear"
```

### File Patterns (for cleanup)

**Safe to delete:**
```bash
.argo-project/builders/*
.argo-project/*.tmp
.argo-project/*.lock
.argo-project/logs/*.log (with retention policy)
```

**Protected:**
```bash
.argo-project/state.json
.argo-project/checkpoints/*
.argo-project/config.json
```

**Branch patterns (safe to delete):**
```bash
sub/*                  # Builder sub-branches
feature/argo-*         # Argo-created feature branches
```

**Branch patterns (protected):**
```bash
main, master, develop  # Protected branches
feature/* (user)       # User-created feature branches
```

---

## Usage Patterns

### State File Relay Pattern

```bash
# Code's turn
update_state "action_owner" "ci"
update_state "action_needed" "gather_requirements"

# Wait for CI
while [ "$(read_state 'action_owner')" != "code" ]; do
    sleep 1
done

# CI finished, code's turn again
action_needed=$(read_state 'action_needed')
```

### Progress Tracking

```bash
# Builder reports progress
progress "backend" 0 "Starting"
progress "backend" 25 "Dependencies analyzed"
progress "backend" 50 "Code generated"
progress "backend" 75 "Tests running"
progress "backend" 100 "Complete"
```

### Checkpoint Flow

```bash
# Save checkpoint at key moments
save_checkpoint "requirements_complete" "User approved requirements"

# ... work happens ...

# Crash recovery: restore from checkpoint
restore_checkpoint "requirements_complete"
```

### User Query Mechanism

```bash
# Ask user (code side)
update_state "user_query" '{"question": "Approve plan?", "options": ["yes", "no"]}'

# Wait for response
while [ "$(read_state 'user_query.response')" = "null" ]; do
    sleep 1
done

response=$(read_state 'user_query.response')
update_state "user_query" "null"  # Clear query
```

---

## Testing Requirements

Each function must have:
- Unit test for success case
- Unit test for failure case (if applicable)
- Test for edge cases (empty inputs, invalid JSON, etc.)
- Integration test with related functions

Use the existing test framework (`workflows/tests/test_framework.sh`).

---

**End of API Reference**
