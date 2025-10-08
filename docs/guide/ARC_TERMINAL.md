# Arc Terminal - Argo Command Line Interface

© 2025 Casey Koons All rights reserved

## Overview

**Arc Terminal** (`arc-term`) is a lightweight command-line interface for managing Argo workflows. It provides an interactive terminal where users can create, monitor, and control multi-CI development workflows.

### Key Concepts

- **One Argo Daemon**: Single background process manages all workflows
- **Multiple Workflows**: Each workflow orchestrates development on a GitHub project
- **Parallel Development**: CIs work on separate repo clones, merge to main feature branch
- **Deterministic Merge**: No negotiation - builders resolve conflicts, push when tests pass
- **Reviewer = User**: Human approves sprint progression (CI reviewers future enhancement)

## Architecture

```
User
  ↓
arc-term (terminal UI)
  ↓
arc-minder (execution bridge)
  ↓
argo daemon (workflow engine)
  ↓
Manages workflows in separate git repos
```

### Process Model

**Argo Daemon** (`argo --daemon`)
- Single background process
- Listens on socket: `/tmp/argo.sock`
- Manages all active workflows
- Survives arc-term restarts

**Arc Terminal** (`arc-term`)
- Interactive UI (foreground)
- Parses commands
- Displays status
- Spawns arc-minder for execution

**Arc Minder** (`arc-minder`)
- Spawned by arc-term per command
- Connects to argo daemon socket
- Executes commands
- Reports status back to arc-term
- Handles async operations

## Directory Structure

### Workspace Layout

```
~/.argo/                                    # Default workspace root
  projects/
    workflows/
      active/
        auth-system/                        # One directory per active workflow
          workflow.json                     # Workflow definition
          state.json                        # Current state (phase, status)
          sprint.json                       # Sprint configuration
          ci-assignments.json               # CI → role → branch mapping
          branches.json                     # Branch merge status
          history.json                      # Audit log
          phases/
            phase-1-status.json             # Per-phase status
            phase-2-status.json
        payment-flow/
          workflow.json
          state.json
          ...
      definitions/                          # User-defined workflows
        custom/
          my-workflow.json
          special-release.json
    github/
      auth-system/                          # Main feature branch repo
        .git/
        src/
        tests/
      auth-system-2/                        # Parallel builder repo
        .git/
        src/
        tests/
      auth-system-3/                        # Another parallel builder repo
        .git/
        src/
        tests/
      payment-flow/                         # Different workflow
        .git/
        src/
  argo.pid                                  # Daemon PID file
  argo.log                                  # Daemon log
```

### System Workflows

System workflows ship with Argo source:
```
~/projects/github/argo/                     # Argo source repo
  workflows/
    development/
      feature/
        standard-feature.json
        parallel-feature.json
      bugfix/
        hotfix.json
    release/
      version-bump.json
```

**Environment Variables:**
- `ARGO_REPO_PATH` - Where argo source repo is installed (default: `~/projects/github/argo`)
- `ARGO_SYSTEM_WORKFLOW_PATH` - Where system workflows are (default: `$ARGO_REPO_PATH/workflows`)
- `ARGO_USER_WORKFLOW_PATH` - Where user workflows are (default: `~/.argo/projects/workflows/definitions`)

## Workflow Lifecycle

### 1. Creating a Workflow

```
User: arc start auth-system
  ↓
Argo daemon:
  - Creates ~/.argo/projects/workflows/active/auth-system/
  - Loads workflow definition
  - Initializes state.json
  - Creates workflow.json
  ↓
Status: PROPOSAL phase
```

### 2. Proposal Phase

User works with CI to refine requirements:
```
arc propose "Implement JWT authentication system"
  ↓
CI analyzes requirements
  ↓
User refines with CI
  ↓
Proposal finalized
  ↓
Status: DEFINE_SPRINT phase
```

### 3. Define Sprint Phase

User defines sprint with CI assistance:
```
arc sprint define
  ↓
CI helps create sprint.json:
  - Phases and steps
  - Parallel vs sequential
  - Acceptance criteria
  - Test requirements
  ↓
User approves sprint
  ↓
Status: REVIEW_SPRINT phase
```

### 4. Review Sprint Phase

User reviews complete sprint plan:
```
arc sprint review
  ↓
User examines:
  - All phases
  - Parallel steps
  - Test requirements
  - Merge strategy
  ↓
User approves: arc sprint approve
  ↓
Status: BUILD phase
```

### 5. Build Phase

**Parallel Development Model:**

Sprint defines phases with parallel steps:
```json
{
  "phase": 1,
  "name": "Core Authentication",
  "parallel_steps": [
    {"step": 1, "builder": "builder-1", "task": "JWT token generation"},
    {"step": 2, "builder": "builder-2", "task": "Token validation"},
    {"step": 3, "builder": "builder-3", "task": "User session management"}
  ]
}
```

**Repo Creation:**
At phase start:
1. Check if enough repos exist (need 3 for above example)
2. Create missing repos: `auth-system-2/`, `auth-system-3/`
3. `git pull` feature branch in all repos (start at same commit)
4. Assign CIs to repos

**Branch Naming:**
```
feature-branch-main     # Main feature branch (builder-1)
feature-branch-2        # Parallel builder-2
feature-branch-3        # Parallel builder-3
```

**Development Flow:**
```
Builder-1 works in feature-branch-main
Builder-2 works in feature-branch-2
Builder-3 works in feature-branch-3
  ↓
Each builder independently:
  - Writes code
  - Runs tests locally
  - When tests pass:
    → git push feature-branch-N
    → Mark step "complete"
```

**Merge to Main Feature Branch:**
```
Builder-1 (main feature branch) sees builder-2 marked "complete"
  ↓
Builder-1: git merge feature-branch-2
  ↓
Conflict?
  ↓
  NO: Clean merge
    → Run tests
    → Tests pass: Mark "merged", continue
    → Tests fail: Fix, retest, push, mark "merged"
  ↓
  YES: Conflicts detected
    → TRIVIAL conflict:
      → Builder-1 fixes
      → Run tests
      → Tests pass: Mark "merged"
      → Tests fail: Treat as COMPLEX
    → COMPLEX conflict:
      → Mark step "not-started"
      → Reset feature-branch-2 to match feature-branch-main
      → Builder-2 starts over (full rework)
```

**Phase Completion:**
All parallel steps merged to main feature branch
  ↓
All tests pass on main feature branch
  ↓
Phase marked "complete"
  ↓
Next phase starts (or workflow complete)

### 6. Sprint Completion

All phases complete:
```
Sprint configured for auto-merge to main?
  ↓
  YES:
    git checkout main
    git merge feature-branch
    → Clean: Tests pass → Sprint "COMPLETE"
    → Conflicts: Builder-1 fixes → Tests pass → Sprint "COMPLETE"
  ↓
  NO:
    Feature branch ready for user review
    Sprint "COMPLETE" (merge deferred)
```

### 7. Workflow States

```
PROPOSAL       - Gathering requirements
DEFINE_SPRINT  - Creating sprint plan
REVIEW_SPRINT  - User reviewing sprint
BUILD          - CIs building in parallel
TEST           - Running phase tests
MERGE          - Merging feature branches
COMPLETE       - Sprint finished
PAUSED         - User paused workflow
ABANDONED      - User canceled workflow
```

## Merge Process Detail

### Step Status Values

Each step tracks:
```json
{
  "step": 2,
  "builder": "builder-2",
  "status": "complete",      // building | complete | merged | conflict
  "branch": "feature-branch-2",
  "tests_passing": true,
  "pushed_at": "2025-10-03T14:30:00Z"
}
```

### Main Builder Merge Logic

```
FOR each completed step:
  git merge feature-branch-N

  IF merge has conflicts:
    IF trivial (simple fix, tests pass after):
      Fix conflicts
      Run tests
      IF tests pass:
        git push
        Mark step "merged"
      ELSE:
        Treat as COMPLEX
    ELSE:
      # COMPLEX conflict - full rework
      Mark step "not-started"
      git reset --hard feature-branch-main
      git push -f feature-branch-N  # Reset to clean state
      Builder-N starts over
  ELSE:
    # Clean merge
    Run tests
    IF tests pass:
      Mark step "merged"
    ELSE:
      Fix test failures
      git push
      Mark step "merged"
```

### Fast Merge Philosophy

**Key principle:** Either merge cleanly or restart from scratch. No complex conflict resolution.

- **No conflicts**: Merge, test, done
- **Trivial conflicts**: Fix, test, if pass then done
- **Complex conflicts**: Reset branch, start over
- **Why this works**: With proper parallel task decomposition, conflicts should be rare
- **Tests are gate**: Nothing marked "merged" until tests pass

## Configuration

### ~/.argorc Format

```bash
# Argo Runtime Configuration
# Generated by: make install

[terminal]
# Prompt format (see variables below)
PROMPT_FORMAT="[%h] [%d] [%b] [%w] [%s]\narc "

# Prompt colors (ANSI escape codes)
PROMPT_COLOR_HOST="\033[36m"      # Cyan
PROMPT_COLOR_DIR="\033[34m"       # Blue
PROMPT_COLOR_BRANCH="\033[35m"    # Magenta
PROMPT_COLOR_WORKFLOW="\033[33m"  # Yellow
PROMPT_COLOR_STATE="\033[32m"     # Green
PROMPT_COLOR_RESET="\033[0m"      # Reset

[paths]
# Workspace root (where workflows and repos live)
ARGO_ROOT=~/.argo

# Argo source repository
ARGO_REPO_PATH=~/projects/github/argo

# System workflows
ARGO_SYSTEM_WORKFLOW_PATH=$ARGO_REPO_PATH/workflows

# User workflows
ARGO_USER_WORKFLOW_PATH=~/.argo/projects/workflows/definitions

[daemon]
# Daemon socket
ARGO_SOCKET=/tmp/argo.sock

# Daemon log
ARGO_LOG=~/.argo/argo.log

[defaults]
# Default CI providers
DEFAULT_BUILDER=claude
DEFAULT_REVIEWER=gpt4
```

### Prompt Variables

```
%h - Hostname
%d - Current working directory
%b - Git branch (if in git repo)
%w - Workflow name (or "no workflow")
%s - Workflow state (or "idle")
%u - Username
%t - Current time (HH:MM)
```

### Environment Variable Precedence

```
1. Shell environment (highest priority)
   export ARGO_ROOT=/custom/path

2. ~/.argorc file
   [paths]
   ARGO_ROOT=/default/path

3. Hard-coded defaults (lowest priority)
   ~/.argo/
```

## Command Reference

### Workflow Management

**`arc`**
- Show active workflows (from `~/.argo/projects/workflows/active/`)
- No-op command, returns to prompt

**`arc start <workflow-name>`**
- Create and start new workflow
- Example: `arc start auth-system`

**`arc stop`**
- Stop current workflow (preserves state)

**`arc pause`**
- Pause current workflow
- Can resume later with `arc resume`

**`arc resume`**
- Resume paused workflow

**`arc switch <workflow-name>`**
- Switch to different active workflow
- Prompt updates to show new workflow context

**`arc list workflows`**
- List all available workflows (user + system)
- Shows: User-defined first, then system workflows

**`arc status`**
- Show current workflow state
- Display: Phase, step statuses, CI assignments

### Sprint Management

**`arc propose <description>`**
- Start proposal phase with CI assistance
- Example: `arc propose "Implement OAuth2 login"`

**`arc sprint define`**
- Work with CI to define sprint
- Creates sprint.json with phases, steps, criteria

**`arc sprint review`**
- Review complete sprint plan
- Shows all phases, parallel steps, tests

**`arc sprint approve`**
- User approves sprint, advances to BUILD phase

**`arc sprint status`**
- Show sprint progress
- Display: Current phase, step completions, merge status

**`arc sprint complete`**
- Mark sprint done
- Triggers final merge if configured

**`arc sprint rollback <phase-number>`**
- Rollback to earlier phase
- Allows sprint revision before BUILD

**`arc sprint revise`**
- Modify sprint configuration
- Only for phases not yet started

### CI Management

**`arc ci list`**
- Show all assigned CIs
- Display: Role, provider, branch, status

**`arc ci assign <role> <provider>`**
- Assign CI to role
- Example: `arc ci assign builder-1 claude`

**`arc ci status <role>`**
- Show specific CI status
- Display: Current task, branch, test results

**`arc ci remove <role>`**
- Remove CI assignment

### Build & Test

**`arc build start`**
- Begin build phase
- Creates repos, assigns CIs, starts development

**`arc build status`**
- Show build progress
- Display: All step statuses, merge states

**`arc test run`**
- Run tests on current branch

**`arc test status`**
- Show test results
- Display: Pass/fail, coverage, errors

### Branch Management

**`arc branch list`**
- List all feature branches
- Display: Branch name, builder, status

**`arc branch status <branch-name>`**
- Show specific branch status
- Display: Commits, test status, merge state

**`arc merge status`**
- Show merge status for all branches
- Display: Completed, pending, conflicts

### Utility Commands

**`arc help [command]`**
- Show help for command
- No argument: Show all commands

**`arc config`**
- Show current configuration
- Display: All ARGO_* settings

**`arc config set <key> <value>`**
- Set configuration value
- Updates ~/.argorc

**`arc quit`** or **`arc exit`**
- Exit arc terminal
- Background workflows continue running

## Command Dispatch Table

Arc-term uses table-driven command dispatch:

```c
typedef struct {
    const char* command;
    const char* description;
    int (*handler)(const char* args);
} arc_command_t;

static arc_command_t arc_commands[] = {
    // Workflow management
    {"start",   "Start new workflow",           arc_cmd_start},
    {"stop",    "Stop current workflow",        arc_cmd_stop},
    {"pause",   "Pause workflow",               arc_cmd_pause},
    {"resume",  "Resume paused workflow",       arc_cmd_resume},
    {"switch",  "Switch to another workflow",   arc_cmd_switch},
    {"status",  "Show workflow status",         arc_cmd_status},

    // Sprint management
    {"propose", "Start proposal phase",         arc_cmd_propose},
    {"sprint",  "Sprint operations",            arc_cmd_sprint},

    // CI management
    {"ci",      "CI operations",                arc_cmd_ci},

    // Build & test
    {"build",   "Build operations",             arc_cmd_build},
    {"test",    "Test operations",              arc_cmd_test},

    // Branch management
    {"branch",  "Branch operations",            arc_cmd_branch},
    {"merge",   "Merge operations",             arc_cmd_merge},

    // Utility
    {"help",    "Show help",                    arc_cmd_help},
    {"config",  "Configuration",                arc_cmd_config},
    {"list",    "List workflows",               arc_cmd_list},
    {"quit",    "Exit terminal",                arc_cmd_quit},
    {"exit",    "Exit terminal",                arc_cmd_quit},

    {NULL, NULL, NULL}  // Sentinel
};
```

## Installation

### Via Make

```bash
cd ~/projects/github/argo
make install
```

**Installs:**
- Binary: `/usr/local/bin/argo` (daemon)
- Binary: `/usr/local/bin/arc-term` (terminal)
- Creates: `~/.argorc` (if missing)
- Creates: `~/.argo/` workspace structure (if missing)

**Does NOT:**
- Start daemon automatically (user runs `argo --daemon`)
- Clone repos (Till handles this)
- Install to system locations (user-local only)

### Via Till

Till automates complete Argo setup:
```bash
till install argo
```

**Till:**
1. Clones argo repo to `~/projects/github/argo/`
2. Runs `make install`
3. Creates `.argorc` with detected settings
4. Starts argo daemon
5. Registers installation in `.till/` symlink registry

## Daemon Management

### Starting Daemon

```bash
argo --daemon
```

**Daemon:**
- Forks to background
- Writes PID to `~/.argo/argo.pid`
- Logs to `~/.argo/argo.log`
- Listens on `/tmp/argo.sock`

### Stopping Daemon

```bash
kill $(cat ~/.argo/argo.pid)
```

Or via arc-term:
```bash
arc daemon stop
```

### Checking Daemon Status

```bash
arc daemon status
```

Shows:
- PID
- Uptime
- Active workflows
- Socket status

## Arc-Minder Process Model

### Command Execution Flow

```
1. User types: arc ci list
   ↓
2. Arc-term parses command
   ↓
3. Arc-term validates command
   ↓
4. Arc-term spawns arc-minder
   ↓
5. Arc-term displays: "working . .. ..."
   ↓
6. Arc-minder:
   - Connects to /tmp/argo.sock
   - Sends JSON: {"command": "ci.list", "workflow": "auth-system"}
   - Receives JSON: {"status": "success", "cis": [...]}
   - Formats output
   - Writes to stdout
   - Exits
   ↓
7. Arc-term:
   - Reads arc-minder output
   - Displays results
   - Returns to prompt
```

### Async Status Display

While arc-minder executes:
```
arc> ci list
working .
working ..
working ...
working .
```

User can interrupt with Ctrl+C (future enhancement).

### Multiple Workflow Tracking

Arc-term tracks PIDs for all spawned arc-minders:
```c
typedef struct {
    pid_t pid;
    char workflow[64];
    char command[256];
    time_t started;
} minder_process_t;

static minder_process_t active_minders[MAX_MINDERS];
```

Allows:
- Switch workflows while commands execute
- Check status of background operations
- Kill stale minders

## Workflow State Files

### workflow.json

Workflow definition (loaded from system or user workflows):
```json
{
  "name": "standard-feature",
  "category": "development/feature",
  "phases": [
    {
      "name": "PROPOSAL",
      "description": "Gather and refine requirements"
    },
    {
      "name": "DEFINE_SPRINT",
      "description": "Create sprint plan"
    },
    {
      "name": "REVIEW_SPRINT",
      "description": "User reviews and approves sprint"
    },
    {
      "name": "BUILD",
      "description": "CIs build in parallel",
      "parallel": true
    },
    {
      "name": "TEST",
      "description": "Run tests after merges"
    },
    {
      "name": "COMPLETE",
      "description": "Merge to main if configured"
    }
  ]
}
```

### state.json

Current workflow state:
```json
{
  "workflow_name": "auth-system",
  "current_phase": "BUILD",
  "current_state": "building",
  "git_branch": "feature/auth-system",
  "created_at": "2025-10-03T10:00:00Z",
  "updated_at": "2025-10-03T14:22:00Z",
  "paused": false
}
```

### sprint.json

Sprint configuration:
```json
{
  "sprint_name": "implement-auth",
  "description": "JWT-based authentication system",
  "auto_merge_to_main": false,
  "phases": [
    {
      "phase_number": 1,
      "name": "Core Authentication",
      "parallel_steps": [
        {
          "step": 1,
          "builder": "builder-1",
          "task": "JWT token generation",
          "branch": "feature/auth-system",
          "repo": "auth-system"
        },
        {
          "step": 2,
          "builder": "builder-2",
          "task": "Token validation",
          "branch": "feature-branch-2",
          "repo": "auth-system-2"
        }
      ]
    }
  ]
}
```

### ci-assignments.json

CI to role/branch mapping:
```json
{
  "assignments": [
    {
      "role": "builder-1",
      "provider": "claude",
      "branch": "feature/auth-system",
      "repo": "auth-system",
      "assigned_at": "2025-10-03T10:15:00Z"
    },
    {
      "role": "builder-2",
      "provider": "gpt4",
      "branch": "feature-branch-2",
      "repo": "auth-system-2",
      "assigned_at": "2025-10-03T10:15:00Z"
    }
  ]
}
```

### branches.json

Branch merge status:
```json
{
  "main_branch": "feature/auth-system",
  "feature_branches": [
    {
      "name": "feature-branch-2",
      "builder": "builder-2",
      "status": "complete",
      "merge_status": "merged",
      "tests_passing": true,
      "last_commit": "abc123",
      "pushed_at": "2025-10-03T14:00:00Z",
      "merged_at": "2025-10-03T14:30:00Z"
    },
    {
      "name": "feature-branch-3",
      "builder": "builder-3",
      "status": "building",
      "merge_status": "pending",
      "tests_passing": false,
      "last_commit": "def456",
      "pushed_at": null,
      "merged_at": null
    }
  ]
}
```

### history.json

Audit log:
```json
{
  "events": [
    {
      "timestamp": "2025-10-03T10:00:00Z",
      "event": "workflow_created",
      "details": "Workflow auth-system created"
    },
    {
      "timestamp": "2025-10-03T10:15:00Z",
      "event": "ci_assigned",
      "details": "builder-1 assigned to claude"
    },
    {
      "timestamp": "2025-10-03T14:30:00Z",
      "event": "branch_merged",
      "details": "feature-branch-2 merged to main"
    }
  ]
}
```

## Future Enhancements

### Phase 1 (Current)
- ✅ Basic workflow management
- ✅ Single workflow at a time
- ✅ Manual CI assignment
- ✅ User as reviewer

### Phase 2 (Near Future)
- Workflow switching while commands execute
- Background command tracking
- Interrupt support (Ctrl+C)
- CI as reviewer option

### Phase 3 (Later)
- Remote argo daemon support
- Multi-user workflows
- Workflow templates
- Automated sprint generation

## See Also

- [INITIALIZATION.md](INITIALIZATION.md) - Argo library lifecycle
- [WORKFLOW.md](WORKFLOW.md) - Development workflow phases
- [TERMINOLOGY.md](TERMINOLOGY.md) - Naming conventions
- [CLAUDE.md](../CLAUDE.md) - Coding standards
