# Arc CLI Design

## Overview
Arc is the command-line interface for managing Argo workflows. It queries and updates the workflow registry managed by the Argo core library.

## Architecture
- **argo** = Core library (manages workflow registry in `.argo/workflows/registry/active_workflow_registry.json`)
- **arc** = CLI tool (queries/updates registry via argo API)
- **Per-terminal context** = `$ARGO_ACTIVE_WORKFLOW` environment variable

## Commands

### `arc init <template> <instance> [branch]`
Create and activate a new workflow instance.

**Arguments:**
- `template` - Workflow template name (e.g., `create_proposal`, `fix_bug`)
- `instance` - Instance identifier (e.g., `my_feature`, `issue_123`)
- `branch` - Optional git branch name (default: `main`)

**Behavior:**
1. Validate template exists (check `.argo/workflows/templates/` and `~/.argo/workflows/templates/`)
2. Create workflow ID: `template_instance`
3. Check for duplicates in registry
4. Add to workflow registry
5. Set `$ARGO_ACTIVE_WORKFLOW` to new workflow ID
6. Save registry
7. Print success message with workflow ID

**Example:**
```bash
$ arc init create_proposal my_feature
Created workflow: create_proposal_my_feature
Active workflow: create_proposal_my_feature (branch: main)

$ arc init fix_bug issue_123 bugfix/issue-123
Created workflow: fix_bug_issue_123
Active workflow: fix_bug_issue_123 (branch: bugfix/issue-123)
```

**Errors:**
- Template not found → Exit with error
- Duplicate workflow ID → Exit with error
- Invalid characters in instance name → Exit with error

---

### `arc list`
List all active workflows in the registry.

**Behavior:**
1. Load workflow registry
2. Display table of all workflows
3. Mark active workflow with `*`

**Example:**
```bash
$ arc list
ACTIVE  ID                          TEMPLATE         INSTANCE     BRANCH           STATUS      CREATED
*       create_proposal_my_feature  create_proposal  my_feature   main             active      2025-01-15
        fix_bug_issue_123          fix_bug          issue_123    bugfix/issue-123 active      2025-01-14
        refactor_cleanup           refactor         cleanup      develop          suspended   2025-01-10
```

**Options:**
- `--status <status>` - Filter by status (active/suspended/completed)
- `--template <name>` - Filter by template

---

### `arc switch <workflow_id>`
Switch the active workflow for the current terminal.

**Arguments:**
- `workflow_id` - Full workflow ID (template_instance)

**Behavior:**
1. Validate workflow exists in registry
2. Set `$ARGO_ACTIVE_WORKFLOW` to workflow_id
3. Update workflow's `last_active` timestamp
4. Print confirmation

**Example:**
```bash
$ arc switch fix_bug_issue_123
Switched to workflow: fix_bug_issue_123 (branch: bugfix/issue-123)

$ arc switch create_proposal_my_feature
Switched to workflow: create_proposal_my_feature (branch: main)
```

**Errors:**
- Workflow not found → Exit with error

---

### `arc status`
Show information about the current active workflow.

**Behavior:**
1. Read `$ARGO_ACTIVE_WORKFLOW`
2. If not set → "No active workflow"
3. Load workflow from registry
4. Display workflow details

**Example:**
```bash
$ arc status
Active workflow: create_proposal_my_feature
  Template:      create_proposal
  Instance:      my_feature
  Branch:        main
  Status:        active
  Created:       2025-01-15 10:30:00
  Last active:   2025-01-15 14:45:23

$ arc status
No active workflow. Use 'arc init' or 'arc switch' to activate a workflow.
```

---

### `arc branch <name>`
Switch the active branch within the current workflow.

**Arguments:**
- `name` - Branch name

**Behavior:**
1. Read `$ARGO_ACTIVE_WORKFLOW`
2. Validate workflow exists
3. Update workflow's `active_branch` field
4. Save registry
5. Print confirmation

**Example:**
```bash
$ arc branch feature/new-design
Updated workflow branch: create_proposal_my_feature → feature/new-design

$ arc status
Active workflow: create_proposal_my_feature
  Template:      create_proposal
  Instance:      my_feature
  Branch:        feature/new-design
  ...
```

**Errors:**
- No active workflow → Exit with error
- Workflow not found → Exit with error

---

### `arc complete`
Mark the current workflow as completed.

**Behavior:**
1. Read `$ARGO_ACTIVE_WORKFLOW`
2. Validate workflow exists
3. Set status to `completed`
4. Clear `$ARGO_ACTIVE_WORKFLOW`
5. Save registry
6. Print confirmation

**Example:**
```bash
$ arc complete
Workflow complete: create_proposal_my_feature
Active workflow cleared.
```

---

### `arc suspend`
Mark the current workflow as suspended (pause work).

**Behavior:**
1. Read `$ARGO_ACTIVE_WORKFLOW`
2. Validate workflow exists
3. Set status to `suspended`
4. Save registry
5. Print confirmation (keep workflow active)

**Example:**
```bash
$ arc suspend
Workflow suspended: create_proposal_my_feature
(Workflow remains active. Use 'arc switch' to change workflows.)
```

---

### `arc resume`
Resume a suspended workflow.

**Arguments:**
- `workflow_id` - Optional (uses current if not specified)

**Behavior:**
1. Get workflow ID (from arg or `$ARGO_ACTIVE_WORKFLOW`)
2. Validate workflow exists and is suspended
3. Set status to `active`
4. If workflow_id was provided, also switch to it
5. Save registry
6. Print confirmation

**Example:**
```bash
$ arc resume
Resumed workflow: create_proposal_my_feature

$ arc resume fix_bug_issue_123
Resumed and switched to workflow: fix_bug_issue_123
```

---

### `arc remove <workflow_id>`
Remove a workflow from the registry.

**Arguments:**
- `workflow_id` - Full workflow ID

**Behavior:**
1. Validate workflow exists
2. If workflow is active, clear `$ARGO_ACTIVE_WORKFLOW`
3. Remove from registry
4. Save registry
5. Print confirmation

**Example:**
```bash
$ arc remove old_experiment_test
Removed workflow: old_experiment_test

$ arc remove create_proposal_my_feature
Warning: Removing active workflow
Removed workflow: create_proposal_my_feature
Active workflow cleared.
```

**Options:**
- `--force` or `-f` - Skip confirmation prompt

---

## Environment Variable Management

Arc needs to set `$ARGO_ACTIVE_WORKFLOW` in the parent shell. Options:

### Option 1: Wrapper Function (Recommended)
User adds to `.bashrc` / `.zshrc`:
```bash
arc() {
    local output=$(command arc "$@")
    echo "$output"

    # Check if arc set an environment variable
    if [[ "$output" == "ARGO_SET_ENV:"* ]]; then
        local envvar="${output#ARGO_SET_ENV:}"
        export ARGO_ACTIVE_WORKFLOW="$envvar"
    fi
}
```

Arc prints: `ARGO_SET_ENV:workflow_id` when it needs to set the variable.

### Option 2: Eval Pattern
```bash
$ eval "$(arc init create_proposal my_feature)"
```

Arc outputs: `export ARGO_ACTIVE_WORKFLOW=create_proposal_my_feature`

### Option 3: Source Pattern
```bash
$ arc init create_proposal my_feature > /tmp/arc_env.sh
$ source /tmp/arc_env.sh
```

**Recommendation:** Start with Option 1 (wrapper function) - most user-friendly.

---

## Error Handling

All commands should:
- Return exit code 0 on success
- Return exit code 1 on error
- Print error messages to stderr
- Print success messages to stdout

---

## File Structure

```
arc/
├── src/
│   ├── main.c              # Main entry point, command dispatcher
│   ├── cmd_init.c          # arc init
│   ├── cmd_list.c          # arc list
│   ├── cmd_switch.c        # arc switch
│   ├── cmd_status.c        # arc status
│   ├── cmd_branch.c        # arc branch
│   ├── cmd_complete.c      # arc complete
│   ├── cmd_suspend.c       # arc suspend
│   ├── cmd_resume.c        # arc resume
│   └── cmd_remove.c        # arc remove
├── include/
│   └── arc_commands.h      # Command function signatures
├── Makefile                # Build system
└── arc                     # Executable (component-local)
```

---

## Questions for Review

1. **Command names:** Are these command names clear? Any aliases needed?
2. **Environment variable:** Which pattern do you prefer for setting `$ARGO_ACTIVE_WORKFLOW`?
3. **Template validation:** Should `arc init` validate that the template exists, or just trust the user?
4. **Confirmation prompts:** Should `arc remove` always prompt, or only without `--force`?
5. **Missing commands:** Any other workflow management commands needed?
6. **Output format:** Plain text vs. JSON option for `arc list`?
7. **Color output:** Should we use colors for status indicators?

---

## Implementation Priority

Phase 1 (MVP):
1. `arc init` - Create workflow
2. `arc list` - List workflows
3. `arc status` - Show current workflow
4. `arc switch` - Change workflow

Phase 2:
5. `arc branch` - Switch branch
6. `arc complete` - Mark complete
7. `arc suspend` / `arc resume` - Pause/resume

Phase 3:
8. `arc remove` - Delete workflow
9. Options and filters for `arc list`
10. Shell completion scripts
