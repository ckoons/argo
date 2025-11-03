# Argo Workflows Architecture

© 2025 Casey Koons. All rights reserved.

## Design Philosophy

Argo Workflows follows these core principles:

1. **OS as Orchestrator** - Use OS processes, not custom runtime
2. **State-File Relay** - Coordination via shared state file
3. **Polling over Events** - Simple, debuggable, resilient
4. **Git for Isolation** - Worktrees enable parallel work
5. **Unix Philosophy** - Do one thing well, compose tools

## System Architecture

### Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│ User                                                         │
│  └─> start_workflow.sh "Build X"                            │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ↓ (initialize state, fork daemon)
┌─────────────────────────────────────────────────────────────┐
│ orchestrator.sh (Background Daemon)                          │
│                                                              │
│  ┌───────────────────────────────────────────┐              │
│  │ Main Loop (every 5 seconds)               │              │
│  │  1. Read state.json                       │              │
│  │  2. Check action_owner                    │              │
│  │  3. If "code" → dispatch handler          │              │
│  │  4. If "ci"/"user" → wait                 │              │
│  │  5. Sleep responsively                    │              │
│  └───────────────────────────────────────────┘              │
│         │                                                    │
│         ├─> design_program.sh   (CI query)                  │
│         ├─> design_build.sh     (create builders)           │
│         ├─> execution.sh        (spawn CI sessions)         │
│         └─> merge_build.sh      (merge branches)            │
│                                                              │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ↓ (all read/write)
┌─────────────────────────────────────────────────────────────┐
│ .argo-project/state.json (Shared State)                     │
│                                                              │
│  {                                                           │
│    "phase": "execution",                                     │
│    "action_owner": "code",                                   │
│    "action_needed": "spawn_builders",                        │
│    "execution": {                                            │
│      "builders": [                                           │
│        {"id": "api", "status": "running", "pid": 1234},      │
│        {"id": "db", "status": "completed", "pid": 1235}      │
│      ]                                                       │
│    }                                                         │
│  }                                                           │
└─────────────────────────────────────────────────────────────┘
```

### State-File Relay Pattern

The relay pattern enables coordination without tight coupling:

```
Time  Action Owner  What Happens
────  ────────────  ─────────────────────────────────────────
T0    code          orchestrator: read state, run design_program.sh
T1    ci            design_program.sh: query CI, save design, set owner="ci"
T2    ci            (orchestrator waits, polls every 5s)
T3    user          user reviews design, sets owner="code", action="design_build"
T4    code          orchestrator: read state, run design_build.sh
T5    code          design_build.sh: create builders, set owner="code"
T6    code          orchestrator: read state, run execution.sh
T7    code          execution.sh: spawn builders, monitor PIDs
...
```

**Key Insight**: The orchestrator never blocks waiting for CI or user input. It simply polls and acts when `action_owner="code"`.

## Orchestrator Daemon

### Lifecycle

```bash
# 1. Start
orchestrator.sh /path/to/project
  ├─ Validate project directory
  ├─ Check for existing orchestrator (PID file)
  ├─ Write PID to .argo-project/orchestrator.pid
  └─ Enter main loop

# 2. Main Loop
while not storage phase and not SIGTERM:
  ├─ Read phase from state
  ├─ Read action_owner from state
  ├─ If action_owner == "code":
  │   ├─ Read action_needed (or use phase name)
  │   ├─ Find handler: phases/${action_needed}.sh
  │   └─ Execute handler with project path
  ├─ Sleep 5 seconds (responsive to signals)
  └─ Loop

# 3. Shutdown
  ├─ Remove PID file
  └─ Exit 0
```

### Signal Handling

```bash
# Graceful shutdown
trap cleanup SIGTERM SIGINT

cleanup() {
    CLEANUP_REQUESTED=1  # Break out of sleep loop
}

# Responsive sleep (checks every 1 second)
for ((i=0; i<POLL_INTERVAL; i++)); do
    sleep 1
    [[ $CLEANUP_REQUESTED -eq 1 ]] && break
done
```

**Why responsive sleep?** A single `sleep 5` would delay shutdown by up to 5 seconds. This approach checks the flag every second, achieving <1s response time.

### Dispatch Logic

```bash
# Determine which handler to call
action_needed=$(read_state "action_needed")
handler_name="${action_needed:-$phase}"  # Fallback to phase name

# Execute handler
phase_handler="$WORKFLOWS_DIR/phases/${handler_name}.sh"
if [[ -f "$phase_handler" ]] && [[ -x "$phase_handler" ]]; then
    "$phase_handler" "$project_path"
fi
```

**Priority**: `action_needed` takes precedence over `phase` name. This allows explicit handler selection independent of current phase.

## Phase Handlers

### Handler Contract

Every phase handler must:

1. **Accept project path** as first argument
2. **Validate inputs** (project directory, state file, required data)
3. **Perform work** (query CI, create builders, spawn processes, etc.)
4. **Update state** (new phase, action_owner, results)
5. **Return exit code** (0=success, non-zero=failure)
6. **Be idempotent** (safe to re-run)

### Handler Template

```bash
#!/bin/bash
# © 2025 Casey Koons All rights reserved

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/logging_enhanced.sh"

main() {
    local project_path="$1"

    # 1. Validate
    if [[ -z "$project_path" ]]; then
        echo "ERROR: Project path required" >&2
        return 1
    fi

    if [[ ! -d "$project_path/.argo-project" ]]; then
        echo "ERROR: .argo-project not found" >&2
        return 1
    fi

    cd "$project_path" || {
        echo "ERROR: Failed to cd to project" >&2
        return 1
    }

    # 2. Read state
    local required_data=$(read_state "some.field")
    if [[ -z "$required_data" ]]; then
        echo "ERROR: Missing required data" >&2
        return 1
    fi

    # 3. Perform work
    echo "=== my_phase phase ==="
    # ... do work ...

    # 4. Update state
    update_state "phase" "next_phase"
    update_state "action_owner" "ci"  # or "code" or "user"
    update_state "action_needed" "next_action"

    echo "my_phase phase complete"
    return 0
}

main "$@"
exit $?
```

### Phase Transitions

```
Phase           Handler              Next Phase    Next Owner
─────           ───────              ──────────    ──────────
init            design_program.sh    design        ci
design          design_build.sh      execution     code
execution       execution.sh         merge_build   code
merge_build     merge_build.sh       storage       user
storage         (none - terminal)    -             -
```

## State File Schema

### Top-Level Structure

```json
{
  "project_name": "my-project",
  "phase": "execution",
  "action_owner": "code",
  "action_needed": "spawn_builders",
  "user_query": "Build a calculator",
  "created": "2025-11-03T10:00:00Z",
  "last_update": "2025-11-03T10:05:23Z",
  "design": { ... },
  "execution": { ... },
  "merge": { ... },
  "error_recovery": { ... },
  "limits": { ... },
  "checkpoints": { ... }
}
```

### Design Object

```json
"design": {
  "summary": "High-level design from CI",
  "components": [
    {
      "id": "api",
      "description": "REST API endpoints",
      "technologies": ["express", "nodejs"]
    },
    {
      "id": "database",
      "description": "Data persistence layer",
      "technologies": ["postgresql"]
    }
  ],
  "created": "2025-11-03T10:01:00Z",
  "approved": true
}
```

### Execution Object

```json
"execution": {
  "builders": [
    {
      "id": "api",
      "description": "REST API endpoints",
      "status": "completed",
      "branch": "argo-builder-api",
      "worktree": "/path/to/.argo-project/builders/api",
      "pid": 12345,
      "progress": 100,
      "started": "2025-11-03T10:02:00Z",
      "completed": "2025-11-03T10:05:00Z"
    }
  ],
  "status": "completed",
  "started": "2025-11-03T10:02:00Z",
  "completed": "2025-11-03T10:05:30Z"
}
```

### Error Recovery Object

```json
"error_recovery": {
  "last_error": "CI query timeout",
  "retry_count": 2,
  "recovery_action": "retry_with_backoff"
}
```

## Git Worktree Strategy

### Why Worktrees?

Git worktrees allow multiple working directories from the same repository, enabling parallel work:

- **Isolation**: Each builder gets its own checkout
- **Lightweight**: Share `.git` directory, minimal disk usage
- **Clean**: Easy to create and destroy
- **Safe**: Changes don't interfere with each other

### Worktree Lifecycle

```bash
# 1. Create (design_build.sh)
git worktree add .argo-project/builders/api -b argo-builder-api

# 2. Work (execution.sh spawns CI in worktree)
cd .argo-project/builders/api
ci "Build API component based on design"
git commit -am "Implement API"

# 3. Merge (merge_build.sh)
git checkout main
git merge --no-ff argo-builder-api

# 4. Cleanup (merge_build.sh)
git worktree remove .argo-project/builders/api --force
git branch -D argo-builder-api
```

### Directory Structure

```
project/
├── .git/                          # Shared git directory
├── .argo-project/
│   ├── state.json
│   ├── logs/
│   └── builders/                  # Worktrees here
│       ├── api/                   # Builder 1 worktree
│       │   ├── src/
│       │   └── (branch: argo-builder-api)
│       ├── database/              # Builder 2 worktree
│       │   ├── migrations/
│       │   └── (branch: argo-builder-database)
│       └── auth/                  # Builder 3 worktree
│           ├── middleware/
│           └── (branch: argo-builder-auth)
├── src/                           # Main worktree (branch: main)
└── README.md
```

## Process Management

### Builder Spawning

```bash
# spawn_builder function in execution.sh
spawn_builder() {
    local builder_id="$1"
    local worktree="$2"
    local description="$3"

    # Build CI prompt
    local prompt="Build the '$builder_id' component..."

    # Spawn in background, capture PID
    (
        cd "$worktree" 2>/dev/null || exit 1
        "$CI_TOOL" "$prompt" >/dev/null 2>&1
        echo $? > ".build_result"
    ) &

    local pid=$!
    echo "$pid"  # Return PID to caller
}
```

### Builder Monitoring

```bash
# Monitor loop in execution.sh
while [[ $all_complete == false ]]; do
    sleep $MONITOR_INTERVAL

    for builder in builders; do
        if [[ "$status" == "running" ]]; then
            # Check if still running
            if ! ps -p "$pid" >/dev/null 2>&1; then
                # Completed - update state
                update_state "execution.builders[$i].status" "completed"
            fi
        fi
    done
done
```

## State Management

### Atomic Updates

```bash
# update_state function (lib/state_file.sh)
update_state() {
    local key="$1"
    local value="$2"

    # 1. Read current state
    # 2. Modify via jq
    # 3. Write to temp file
    # 4. Atomic rename
    jq --arg k "$key" --arg v "$value" \
       'setpath($k | split("."); $v)' \
       "$STATE_FILE" > "${STATE_FILE}.tmp"

    mv "${STATE_FILE}.tmp" "$STATE_FILE"
}
```

**Why atomic?** The `mv` operation is atomic on most filesystems. This prevents the orchestrator from reading a partially-written file.

### Locking (Optional)

```bash
# with_state_lock function
with_state_lock() {
    local command="$1"

    perl -e '
        use Fcntl qw(:flock);
        open(my $fh, ">", $ARGV[0]) or die;
        flock($fh, LOCK_EX) or die;
        system("bash", "-c", $ARGV[1]);
        exit($? >> 8);
    ' "$STATE_LOCK" "$command"
}
```

## Logging

### Log Levels

- **STATE**: State transitions (phase changes, ownership changes)
- **BUILDER**: Builder lifecycle events (spawned, completed, failed)
- **MERGE**: Merge operations (success, conflict, resolution)
- **ERROR**: Errors and failures

### Log Format

```
[2025-11-03T10:05:23Z] [STATE] init → design
[2025-11-03T10:05:24Z] [BUILDER] api: spawned (PID 12345)
[2025-11-03T10:08:15Z] [BUILDER] api: completed
[2025-11-03T10:08:20Z] [MERGE] Merging branch argo-builder-api
[2025-11-03T10:08:21Z] [ERROR] Merge conflict in src/index.js
```

## Performance Characteristics

### Time Complexity

- **State read**: O(1) - direct jq query
- **Handler dispatch**: O(1) - file lookup
- **Builder monitoring**: O(n) - iterate all builders
- **Merge**: O(b) - merge b branches sequentially

### Space Complexity

- **State file**: O(n) - grows with number of builders
- **Logs**: O(t) - grows with time (rotation recommended)
- **Worktrees**: O(n × s) - n builders × s size of codebase

### Scalability Limits

- **Max parallel builders**: Limited by OS process limit (~1000s)
- **Max state file size**: Limited by jq memory (~100MB practical)
- **Max workflow duration**: No limit (daemon runs indefinitely)

## Security Considerations

1. **PID File Race**: orchestrator checks existing PID before starting
2. **Path Injection**: All paths validated before use
3. **Command Injection**: No user input passed to shell without quoting
4. **File Permissions**: State files readable only by owner (644)
5. **Signal Handling**: Only SIGTERM/SIGINT handled, no SIGKILL

## Testing Strategy

### Test Isolation

Each test creates its own:
- Temporary project directory
- Isolated state file
- Mock CI tool
- Separate orchestrator instance

### Test Coverage

```
Component                Tests    Coverage
─────────────────────   ──────    ────────
start_workflow.sh         9       100%
orchestrator.sh           8       100%
design_program.sh         7       100%
design_build.sh           6       100%
execution.sh              6       100%
merge_build.sh            7       100%
─────────────────────   ──────    ────────
TOTAL                    43       100%
```

### Test Execution Time

- Individual test: 2-8 seconds
- Full suite: ~60 seconds
- Parallel execution: Not yet implemented

## Future Architecture Improvements

1. **Event-Based Dispatch** - inotify instead of polling
2. **Distributed Orchestration** - Multiple machines via shared filesystem
3. **Workflow DAG** - Generic DAG executor, not just linear phases
4. **Persistent Daemon** - Single daemon for all projects
5. **HTTP API** - REST interface for remote control

## References

- State management: `lib/state_file.sh`
- Logging: `lib/logging_enhanced.sh`
- Tests: `tests/test_*.sh`
- Examples: See `tests/` for working examples of all patterns
