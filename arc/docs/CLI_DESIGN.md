# Arc CLI Design (Final)

## Overview
Arc is the command-line interface for managing Argo workflows. It tells the Argo orchestrator to start/stop workflows and queries workflow state from the registry and executor.

## Architecture
- **argo** = Core library (orchestrator spawns/manages workflow processes, maintains registry)
- **arc** = CLI tool (sends commands to argo, queries state, manages user context)
- **Context** = `$ARGO_ACTIVE_WORKFLOW` environment variable (terminal-session scoped)
- **Logs** = `~/.argo/logs/workflow-name.log`

## Context

Arc maintains a **terminal-session context** via `$ARGO_ACTIVE_WORKFLOW`:
- Set by `arc switch [workflow_name]`
- Used as default for single-workflow commands (pause, resume, abandon)
- Does NOT persist across terminals
- Convenience to avoid repeating workflow names

## Commands

### `arc help`
Show general usage.

**Output:**
```
arc - Argo Workflow CLI

Usage:
  arc help [command]          Show help for a specific command
  arc switch [workflow_name]  Set active workflow context
  arc workflow <subcommand>   Manage workflows

Workflow Subcommands:
  start [template] [instance]     Create and start workflow
  list [active|template]          List workflows or templates
  status [workflow_name]          Show workflow status
  pause [workflow_name]           Pause workflow at next checkpoint
  resume [workflow_name]          Resume paused workflow
  abandon [workflow_name]         Stop and remove workflow

For more details: arc help <command>
```

---

### `arc help [command]`
Show detailed help for a specific command.

**Example:**
```bash
$ arc help workflow start
arc workflow start [template] [instance]

Creates a new workflow instance and starts background execution.

Arguments:
  template  - Workflow template name (from system or user templates)
  instance  - Unique instance identifier

Workflow name format: template_instance
Example: arc workflow start create_proposal my_feature
         Creates workflow: create_proposal_my_feature
```

---

### `arc switch [workflow_name]`
Set the active workflow context for the current terminal.

**Behavior:**
1. Validate workflow exists in registry
2. Set `$ARGO_ACTIVE_WORKFLOW` to workflow_name
3. Update workflow's `last_active` timestamp in registry
4. Print confirmation

**Output:**
```bash
$ arc switch create_proposal_my_feature
Switched to workflow: create_proposal_my_feature
```

**Errors:**
- Workflow not found → stderr + exit 1
- No workflow name provided → stderr + exit 1

---

### `arc workflow start [template] [instance]`
Create a new workflow instance and start background execution.

**Arguments:**
- `template` - Template name (required)
- `instance` - Instance identifier (required)

**Behavior:**
1. Validate template exists (system or user templates)
2. Create workflow ID: `template_instance`
3. Check for duplicate in registry
4. Add to workflow registry (status: active)
5. Tell argo orchestrator to start workflow execution
6. Print confirmation with workflow name

**Output:**
```bash
$ arc workflow start create_proposal my_feature
Started workflow: create_proposal_my_feature
Logs: ~/.argo/logs/create_proposal_my_feature.log
```

**Errors:**
- Template not found → stderr + exit 1
- Duplicate workflow ID → stderr + exit 1
- Invalid instance name → stderr + exit 1
- Orchestrator start failed → stderr + exit 1

**Notes:**
- Does NOT automatically switch context
- Workflow begins executing in background immediately
- User can switch to it later with `arc switch`

---

### `arc workflow list`
List all active workflows and available templates.

**Output:**
```bash
$ arc workflow list

ACTIVE WORKFLOWS:
CONTEXT  NAME                           TEMPLATE         INSTANCE     STATUS   STEP        TIME
*        create_proposal_my_feature     create_proposal  my_feature   running  3/5         2m 15s
         fix_bug_issue_123              fix_bug          issue_123    paused   2/4         45s

TEMPLATES:
SCOPE    NAME
user     custom_review
user     experimental_flow
system   create_proposal
system   fix_bug
system   refactor
```

**Notes:**
- `*` marks current context workflow
- Shows both active workflows and available templates

---

### `arc workflow list active`
List only active workflows.

**Output:**
```bash
$ arc workflow list active

CONTEXT  NAME                           TEMPLATE         INSTANCE     STATUS   STEP        TIME
*        create_proposal_my_feature     create_proposal  my_feature   running  3/5         2m 15s
         fix_bug_issue_123              fix_bug          issue_123    paused   2/4         45s
```

---

### `arc workflow list template`
List only available templates (user and system).

**Output:**
```bash
$ arc workflow list template

SCOPE    NAME                DESCRIPTION
user     custom_review       Custom code review workflow
user     experimental_flow   Experimental multi-agent flow
system   create_proposal     Create GitHub issue/PR proposal
system   fix_bug            Debug and fix reported issues
system   refactor           Code refactoring workflow
```

**Template Discovery:**
- User templates: `~/.argo/workflows/templates/`
- System templates: `argo/workflows/templates/` (shipped with argo)

---

### `arc workflow status`
Show status of all active workflows.

**Output:**
```bash
$ arc workflow status

WORKFLOW: create_proposal_my_feature
  Template:       create_proposal
  Instance:       my_feature
  Status:         running
  Current Step:   3/5 (generate_proposal)
  Checkpoint:     step_2_complete
  Step Time:      45s
  Total Time:     2m 15s
  Created:        2025-01-15 10:30:00

WORKFLOW: fix_bug_issue_123
  Template:       fix_bug
  Instance:       issue_123
  Status:         paused
  Current Step:   2/4 (analyze_code)
  Checkpoint:     step_1_complete
  Step Time:      12s
  Total Time:     45s
  Created:        2025-01-14 15:20:00
```

---

### `arc workflow status [workflow_name]`
Show status of a specific workflow.

**Arguments:**
- `workflow_name` - Full workflow ID (template_instance)

**Output:**
```bash
$ arc workflow status create_proposal_my_feature

WORKFLOW: create_proposal_my_feature
  Template:       create_proposal
  Instance:       my_feature
  Status:         running
  Current Step:   3/5 (generate_proposal)
  Checkpoint:     step_2_complete
  Step Time:      45s
  Total Time:     2m 15s
  Created:        2025-01-15 10:30:00
  Logs:           ~/.argo/logs/create_proposal_my_feature.log
```

**Errors:**
- Workflow not found → stderr + exit 1

---

### `arc workflow pause [workflow_name]`
Pause workflow execution at the next checkpoint.

**Arguments:**
- `workflow_name` - Optional (uses context if omitted)

**Behavior:**
1. Get workflow name (from arg or `$ARGO_ACTIVE_WORKFLOW`)
2. Validate workflow exists and is running
3. Signal argo orchestrator to pause at next checkpoint
4. Update registry status to 'paused'
5. Print confirmation

**Output:**
```bash
$ arc workflow pause
Pausing workflow: create_proposal_my_feature
Workflow will pause at next checkpoint.

$ arc workflow pause fix_bug_issue_123
Pausing workflow: fix_bug_issue_123
Workflow will pause at next checkpoint.
```

**Idempotent:**
- Already paused → No change, print "Workflow already paused"

**Errors:**
- No context and no name provided → stderr + exit 1
- Workflow not found → stderr + exit 1

---

### `arc workflow resume [workflow_name]`
Resume paused workflow from last checkpoint.

**Arguments:**
- `workflow_name` - Optional (uses context if omitted)

**Behavior:**
1. Get workflow name (from arg or `$ARGO_ACTIVE_WORKFLOW`)
2. Validate workflow exists and is paused
3. Signal argo orchestrator to resume from checkpoint
4. Update registry status to 'running'
5. Print confirmation

**Output:**
```bash
$ arc workflow resume
Resuming workflow: create_proposal_my_feature
Workflow restarted from checkpoint: step_2_complete

$ arc workflow resume fix_bug_issue_123
Resuming workflow: fix_bug_issue_123
Workflow restarted from checkpoint: step_1_complete
```

**Idempotent:**
- Already running → No change, print "Workflow already running"

**Errors:**
- No context and no name provided → stderr + exit 1
- Workflow not found → stderr + exit 1

---

### `arc workflow abandon [workflow_name]`
Stop workflow execution and remove from registry.

**Arguments:**
- `workflow_name` - Optional (uses context if omitted)

**Behavior:**
1. Get workflow name (from arg or `$ARGO_ACTIVE_WORKFLOW`)
2. Validate workflow exists
3. **Prompt for confirmation:** "Abandon workflow [name]? (y/N)"
4. If confirmed:
   - Signal argo executor to kill workflow processes
   - Remove from workflow registry
   - Clear context if this was the active workflow
   - Keep logs in `~/.argo/logs/`
   - Leave /tmp files (cleaned on reboot)
5. Print confirmation

**Output:**
```bash
$ arc workflow abandon
Abandon workflow: create_proposal_my_feature? (y/N) y
Stopping workflow processes...
Removed from registry: create_proposal_my_feature
Logs preserved: ~/.argo/logs/create_proposal_my_feature.log
Context cleared.

$ arc workflow abandon fix_bug_issue_123
Abandon workflow: fix_bug_issue_123? (y/N) n
Cancelled.
```

**Cleanup:**
- ✓ Kill processes (executor handles graceful then force)
- ✓ Remove registry entry
- ✗ Keep logs (user manages)
- ✗ Leave /tmp files

**Errors:**
- No context and no name provided → stderr + exit 1
- Workflow not found → stderr + exit 1
- User declined confirmation → exit 0 (not an error)

---

## Environment Variable Management

Arc sets `$ARGO_ACTIVE_WORKFLOW` via wrapper function in user's shell rc file.

**User adds to `.bashrc` / `.zshrc`:**
```bash
arc() {
    local output
    output=$(command arc "$@" 2>&1)
    local exit_code=$?

    echo "$output"

    # Check if arc wants to set environment variable
    if echo "$output" | grep -q "^ARGO_SET_ENV:"; then
        local envvar=$(echo "$output" | grep "^ARGO_SET_ENV:" | cut -d: -f2)
        export ARGO_ACTIVE_WORKFLOW="$envvar"
    fi

    # Check if arc wants to clear environment variable
    if echo "$output" | grep -q "^ARGO_CLEAR_ENV"; then
        unset ARGO_ACTIVE_WORKFLOW
    fi

    return $exit_code
}
```

**Arc outputs special directives:**
- `ARGO_SET_ENV:workflow_name` - Sets context
- `ARGO_CLEAR_ENV` - Clears context

**These lines are filtered from user-visible output by the wrapper.**

---

## Error Handling

- Exit code 0 on success
- Exit code 1 on error
- Error messages to stderr
- Success messages to stdout
- Special env directives to stdout (filtered by wrapper)

---

## File Structure

```
arc/
├── src/
│   ├── main.c              # Entry point, command dispatcher
│   ├── arc_context.c       # Context management (set/get/clear)
│   ├── arc_help.c          # Help text
│   ├── cmd_switch.c        # arc switch
│   ├── cmd_workflow.c      # Workflow subcommand dispatcher
│   ├── workflow_start.c    # arc workflow start
│   ├── workflow_list.c     # arc workflow list
│   ├── workflow_status.c   # arc workflow status
│   ├── workflow_pause.c    # arc workflow pause
│   ├── workflow_resume.c   # arc workflow resume
│   └── workflow_abandon.c  # arc workflow abandon
├── include/
│   ├── arc_commands.h      # Command function signatures
│   └── arc_context.h       # Context utilities
├── Makefile
└── arc                     # Executable (component-local)
```

---

## Integration with Argo

Arc interacts with argo through:
1. **Workflow Registry API** (`argo_workflow_registry.h`)
   - Add/remove workflows
   - Query workflow state
   - Update status/timestamps

2. **Workflow Executor API** (to be defined)
   - Start workflow execution
   - Pause/resume at checkpoints
   - Kill workflow processes
   - Query execution state

3. **Template Discovery**
   - Scan `~/.argo/workflows/templates/`
   - Scan `argo/workflows/templates/`
   - Validate template files

---

## Implementation Phases

**Phase 1 (MVP):**
1. `arc help`
2. `arc switch`
3. `arc workflow start`
4. `arc workflow list`
5. `arc workflow status`

**Phase 2:**
6. `arc workflow pause`
7. `arc workflow resume`
8. `arc workflow abandon`

**Phase 3:**
9. Enhanced status displays
10. Shell completion
11. Error recovery
