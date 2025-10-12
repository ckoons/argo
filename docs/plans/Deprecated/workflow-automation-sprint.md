# Workflow Automation Sprint

**Status**: Ready for Implementation
**Created**: 2025-01-15
**Target**: 2 days (~14 hours)
**Version**: 2.0 (Revised based on Casey's design)

---

## Overview

Enable workflows to emerge naturally from interactive work through:
1. **Config hierarchy** - User/project/local configuration scopes
2. **Argo environment** - Shell-like variables with expansion and CI extraction
3. **Logs as recordings** - Unified logging/recording infrastructure
4. **Log extraction** - Generate input scripts from logs
5. **Log replay** - Re-run workflows from logs

---

## Core Philosophy

### "Logs ARE Recordings"
- No separate recording infrastructure needed
- Logs already capture everything (inputs, outputs, CI responses, user actions)
- Standard Unix tools work (grep, awk, tail, less)
- Extract input scripts from logs, replay workflows from logs
- 7-day retention (promotion to scripts preserves indefinitely)

### "Argo Environment" (Shell-like Variables)
- Workflow-scoped variables (not system environment)
- Shell semantics: set, unset, expansion (`${var}`)
- CI can set variables via response extraction
- Enables dynamic, intelligent workflows

### "Config Hierarchy" (Familiar Pattern)
- Mirrors `.env.argo` pattern developers already know
- Three scopes: User (`~/.argo/config/`) > Project (`argo/config/`) > Local (`argo/.argo/config/`)
- Clean separation: user preferences, team conventions, machine-specific

---

## Part 1: Config System

### Directory Structure
```
~/.argo/config/config.json          # User scope (personal preferences, API keys)
argo/config/config.json             # Project scope (git-tracked, team conventions)
argo/.argo/config/config.json       # Local scope (git-ignored, machine-specific overrides)
```

### Config Hierarchy (Precedence)
```
Workflow-level > Local > Project > User > Built-in defaults
    (highest priority)                        (lowest priority)
```

### Config Format
```json
{
  "default_provider": "claude_code",
  "default_model": "claude-sonnet-4-5",
  "log_retention_days": 7,
  "recording_format": "structured",
  "auto_extract_on_success": true,
  "git": {
    "remote": "origin",
    "auto_push": false
  },
  "reviewers": ["alice", "bob"]
}
```

### Config Operations
```bash
# Set config value
arc config set default_provider=ollama
arc config set git.remote=upstream

# Get config value
arc config get default_provider

# List all config
arc config list
arc config list --scope=user      # Show only user scope
arc config list --scope=project   # Show only project scope
arc config list --scope=local     # Show only local scope

# Delete config key
arc config delete default_provider
```

### Config Loading
```c
/* Load hierarchy (lowest to highest priority) */
argo_config_load_file(config, "~/.argo/config/config.json");     /* User */
argo_config_load_file(config, "argo/config/config.json");        /* Project */
argo_config_load_file(config, "argo/.argo/config/config.json");  /* Local */
argo_config_load_workflow(config, workflow_path);                /* Workflow-level */
```

### Implementation
**New files**:
- `src/config/argo_config.c` - Config loading, merging, lookup
- `src/config/argo_config_discovery.c` - Find config files in hierarchy
- `include/argo_config.h` - Config API
- `arc/src/cmd_config.c` - `arc config` command

**Key functions**:
```c
int argo_config_load_hierarchy(argo_config_t* config, const char* workflow_path);
int argo_config_get_string(argo_config_t* config, const char* key, char* value, size_t len);
int argo_config_get_int(argo_config_t* config, const char* key, int* value);
int argo_config_get_bool(argo_config_t* config, const char* key, bool* value);
int argo_config_set(argo_config_t* config, const char* key, const char* value, config_scope_t scope);
int argo_config_save(argo_config_t* config, config_scope_t scope);
```

**Effort**: 3 hours

---

## Part 2: Argo Environment (Shell-like Variables)

### Concept
**Workflow-scoped variables** with shell semantics:
- Set: `argo env key=value`
- Unset: `argo env key=` or `argo env !key`
- Expansion: `${variable}` in workflow steps
- CI extraction: Extract values from CI responses into env

### Workflow Step Type: `argo_env`
```json
{
  "step": "argo_env",
  "action": "set",
  "key": "review_status",
  "value": "pending"
}

{
  "step": "argo_env",
  "action": "unset",
  "key": "temp_var"
}
```

### Variable Expansion
```json
{
  "step": "display",
  "message": "Reviewing ${file_path} for ${review_type} issues"
}

{
  "step": "shell",
  "command": "git commit -m '${commit_msg}'"
}
```

### CI Variable Extraction
```json
{
  "step": "ci_analyze",
  "task": "Review this code. Respond APPROVED or CHANGES_NEEDED.",
  "variable": "ci_response",
  "extract_env": {
    "review_status": "^(APPROVED|CHANGES_NEEDED)"
  }
}

{
  "step": "conditional",
  "if": "${review_status} == APPROVED",
  "then": [
    {"step": "shell", "command": "git push"}
  ]
}
```

**CI sets env** → **Workflow uses env for control flow**

### Arc Command
```bash
# Show current argo env
arc env list

# Set env (for current session)
arc env set status=approved

# Unset env
arc env unset status
```

### Implementation
**New files**:
- `src/workflow/argo_env.c` - Environment variable management
- `include/argo_env.h` - Environment API
- `arc/src/cmd_env.c` - `arc env` command

**Key structures**:
```c
typedef struct {
    char key[256];
    char value[1024];
} argo_env_var_t;

typedef struct {
    argo_env_var_t* vars;
    int count;
    int capacity;
} argo_env_t;
```

**Key functions**:
```c
int argo_env_set(argo_env_t* env, const char* key, const char* value);
int argo_env_unset(argo_env_t* env, const char* key);
const char* argo_env_get(argo_env_t* env, const char* key);
int argo_env_expand(argo_env_t* env, const char* input, char* output, size_t len);
int argo_env_extract_from_response(argo_env_t* env, const char* response, const char* regex_pattern, const char* key);
```

**Modified files**:
- `src/workflow/argo_workflow_context.c` - Add env to context, expansion
- `src/workflow/argo_workflow_steps_basic.c` - Add `argo_env` step handler

**Effort**: 4 hours

---

## Part 3: Logs as Recordings

### Log Format (Structured)
**Hybrid format**: Human-readable + machine-parseable

```
[2025-01-15 10:30:00] [INFO] [wf-123] Workflow started: code_review.json
[2025-01-15 10:30:01] [INFO] [wf-123] Step 1/5: ci_analyze
[2025-01-15 10:30:01] [DEBUG] [wf-123] CI prompt: "Review this code..."
[2025-01-15 10:30:05] [DEBUG] [wf-123] CI response: "The code looks good. APPROVED."
[2025-01-15 10:30:05] [ENV] [wf-123] Set review_status=APPROVED
[2025-01-15 10:30:06] [INPUT] [wf-123] prompt="Continue?" response="y"
[2025-01-15 10:30:10] [INFO] [wf-123] Workflow completed: success
```

**Key insight**: Already contains everything needed for replay!

### Log Directory Structure
```
~/.argo/logs/
├── 2025-01-15/
│   ├── wf-123.log
│   └── wf-124.log
├── 2025-01-16/
│   └── wf-125.log
└── archive/                    # Manually preserved logs
    └── important-run.log
```

### Log Retention
- **Default**: 7 days (configurable: `log_retention_days`)
- **Auto-prune**: Daemon runs daily cleanup
- **Promotion**: Extracting script preserves log until expiry
- **Archive**: Manual preservation (`arc log archive wf-123`)

### New Log Event Types
```c
/* Existing: INFO, DEBUG, WARN, ERROR */

/* New event types for recording */
LOG_ENV(workflow_id, "Set %s=%s", key, value);           /* Argo env changes */
LOG_INPUT(workflow_id, "prompt=\"%s\" response=\"%s\"", prompt, response);  /* User input */
LOG_CI_QUERY(workflow_id, "prompt=\"%s\"", prompt);      /* CI query start */
LOG_CI_RESPONSE(workflow_id, "response=\"%s\"", response);  /* CI response */
```

### Implementation
**Modified files**:
- `include/argo_log.h` - Add new log macros (LOG_ENV, LOG_INPUT, LOG_CI_QUERY, LOG_CI_RESPONSE)
- `src/argo_log.c` - Implement new log event types
- `src/workflow/argo_workflow_executor.c` - Log all workflow events
- `src/workflow/argo_workflow_input.c` - Log user inputs
- `src/workflow/argo_env.c` - Log env changes

**Effort**: 2 hours

---

## Part 4: Log Extraction (Logs → Input Scripts)

### Purpose
Generate reusable input scripts from logs.

### Usage
```bash
# Extract input script from log
arc log extract wf-123

# Save to file
arc log extract wf-123 > my_script.json

# Extract and move to scripts directory
arc log extract wf-123 --save-as=code_review_automation.json
```

### Extraction Logic
Parse log file and extract:
1. **User inputs**: `[INPUT]` log entries → `inputs` array
2. **Env variables**: `[ENV]` log entries → `env` object
3. **CI responses**: `[CI_RESPONSE]` entries → Optional: canned responses for testing

### Generated Input Script Format
```json
{
  "workflow_name": "Extracted from wf-123",
  "description": "Auto-generated from workflow execution on 2025-01-15",
  "base_template": "code_review.json",
  "provider": "mock",
  "env": {
    "review_status": "APPROVED",
    "file_path": "src/auth.c"
  },
  "inputs": [
    {"prompt": "Continue? (y/n)", "response": "y"},
    {"prompt": "Approve changes?", "response": "yes"}
  ]
}
```

### Implementation
**New files**:
- `src/recording/argo_log_extract.c` - Parse logs, generate input scripts
- `include/argo_log_extract.h` - Extraction API
- `arc/src/cmd_log.c` - `arc log` commands (extract, list, show, archive, prune)

**Key functions**:
```c
int argo_log_parse(const char* log_path, workflow_log_t* parsed);
int argo_log_extract_inputs(workflow_log_t* log, char* json_output, size_t len);
int argo_log_extract_env(workflow_log_t* log, char* json_output, size_t len);
```

**Effort**: 3 hours

---

## Part 5: Log Replay

### Purpose
Re-run workflows using extracted input scripts from logs.

### Usage
```bash
# Replay workflow from log (auto-extract + run)
arc log replay wf-123

# Interactive replay (pause at each step)
arc log replay wf-123 --interactive

# Diff replay (compare with baseline)
arc log replay wf-123 --diff
```

### Replay Modes

#### 1. Automated Replay
```bash
arc log replay wf-123
```
- Extract input script from log (in-memory)
- Run workflow with extracted inputs
- Non-interactive (uses scripted inputs)
- Perfect for CI/CD

#### 2. Interactive Replay
```bash
arc log replay wf-123 --interactive
```
- Pause at each step
- Show: expected input vs. actual response
- Allow: skip, modify, continue
- Perfect for debugging

#### 3. Diff Replay
```bash
arc log replay wf-123 --diff
```
- Run workflow with extracted inputs
- Compare: current execution vs. recorded baseline
- Show: divergences (env, outputs, control flow)
- Perfect for regression testing

### Implementation
**New files**:
- `src/recording/argo_log_replay.c` - Replay workflows from logs
- `include/argo_log_replay.h` - Replay API
- `arc/src/cmd_log.c` - Add `arc log replay` subcommand

**Key functions**:
```c
int argo_log_replay(const char* workflow_id, replay_mode_t mode);
int argo_log_replay_interactive(const char* workflow_id);
int argo_log_replay_diff(const char* workflow_id);
```

**Effort**: 2 hours

---

## Implementation Plan

### Phase 1: Foundation (Day 1, 6-7 hours)

#### Task 1.1: Config System (3 hours)
- [ ] Create `argo_config.c` and `argo_config.h`
- [ ] Implement hierarchy loading (user > project > local)
- [ ] Config operations: load, get, set, save
- [ ] Add `arc config` command
- [ ] Test config precedence

#### Task 1.2: Argo Environment (4 hours)
- [ ] Create `argo_env.c` and `argo_env.h`
- [ ] Implement set, unset, get, expand
- [ ] Add `argo_env` workflow step
- [ ] Integrate with workflow context
- [ ] Variable expansion in steps
- [ ] CI response extraction (regex)
- [ ] Add `arc env` command
- [ ] Test expansion and extraction

### Phase 2: Recording & Replay (Day 2, 7-8 hours)

#### Task 2.1: Enhanced Logging (2 hours)
- [ ] Add LOG_ENV, LOG_INPUT, LOG_CI_QUERY, LOG_CI_RESPONSE macros
- [ ] Log all workflow events (inputs, env changes, CI responses)
- [ ] Structured log format (human-readable + parseable)
- [ ] Daily log directories (`~/.argo/logs/YYYY-MM-DD/`)

#### Task 2.2: Log Extraction (3 hours)
- [ ] Create `argo_log_extract.c`
- [ ] Parse log files (extract inputs, env, responses)
- [ ] Generate input scripts from logs
- [ ] Add `arc log extract` command
- [ ] Add `arc log list`, `arc log show` commands

#### Task 2.3: Log Replay (2 hours)
- [ ] Create `argo_log_replay.c`
- [ ] Implement automated replay
- [ ] Implement interactive replay (optional)
- [ ] Add `arc log replay` command
- [ ] Test replay with extracted scripts

#### Task 2.4: Log Management (1 hour)
- [ ] Add `arc log archive` command (move to archive/)
- [ ] Add `arc log prune` command (delete old logs)
- [ ] Automatic pruning in daemon (daily, 7-day retention)

---

## File Structure

### New Files
```
src/
├── config/
│   ├── argo_config.c                    # Config system (load/save/merge)
│   └── argo_config_discovery.c          # Find config files in hierarchy
├── workflow/
│   └── argo_env.c                       # Argo environment variables
└── recording/
    ├── argo_log_extract.c               # Extract input scripts from logs
    └── argo_log_replay.c                # Replay workflows from logs

include/
├── argo_config.h                        # Config API
├── argo_env.h                           # Environment API
├── argo_log_extract.h                   # Log extraction API
└── argo_log_replay.h                    # Replay API

arc/src/
├── cmd_config.c                         # arc config command
├── cmd_env.c                            # arc env command
└── cmd_log.c                            # arc log commands (list/show/extract/replay/archive/prune)

tests/
├── test_config.c                        # Config system tests
├── test_env.c                           # Argo env tests
└── test_log_extract.c                   # Log extraction tests
```

### Modified Files
```
include/argo_log.h                       # Add LOG_ENV, LOG_INPUT, LOG_CI_QUERY, LOG_CI_RESPONSE
src/argo_log.c                           # Implement new log event types
src/workflow/argo_workflow_context.c     # Add env to context, expansion
src/workflow/argo_workflow_executor.c    # Log all workflow events
src/workflow/argo_workflow_input.c       # Log user inputs
src/workflow/argo_workflow_steps_basic.c # Add argo_env step handler
src/daemon/argo_shared_services.c        # Add log pruning task (daily)
arc/src/main.c                           # Register new commands (config, env, log)
```

---

## Storage Locations

```
~/.argo/
├── config/
│   └── config.json                  # User-level config
└── logs/
    ├── 2025-01-15/
    │   ├── wf-123.log
    │   └── wf-124.log
    ├── 2025-01-16/
    │   └── wf-125.log
    └── archive/                     # Manually preserved logs
        └── important-run.log

argo/
└── config/
    └── config.json                  # Project-level config (git-tracked)

argo/.argo/
└── config/
    └── config.json                  # Local config (git-ignored)
```

---

## Key Design Decisions

### Why Config in `config/` Subdirectories?
- **Organization**: Clear, predictable structure
- **Git-friendly**: Easy to track/ignore appropriately
- **Extensible**: Future configs can be added alongside

### Why Argo Env (Not System Env)?
- **Scoped**: Workflow-only, doesn't pollute system environment
- **Safe**: Can't break system
- **Inspectable**: `arc env list` shows current state
- **Shell-like**: Familiar semantics (set, unset, expansion)

### Why Logs ARE Recordings?
- **No duplication**: Logs already exist, already contain everything
- **Standard tools**: grep, awk, tail, less all work
- **Efficient**: Logs optimized for append-only writes
- **Structured**: Parseable back into events
- **Unix philosophy**: Text is universal interface

### Why 7-Day Log Retention?
- **Sufficient**: 99% of debugging happens within days
- **Storage**: 7 days × 100 workflows × 50KB = 35MB (negligible)
- **Configurable**: Can be adjusted per installation
- **Archive option**: Important logs preserved manually

### Why Variable Expansion (${var})?
- **Familiar**: Shell variable syntax everyone knows
- **Powerful**: Enables dynamic workflows
- **Safe**: Scoped to workflow execution
- **Simple**: Easy to implement, easy to debug

---

## Success Criteria

- [ ] Config hierarchy loads correctly (user > project > local)
- [ ] Config values can be set/get/delete via `arc config`
- [ ] Argo env variables can be set/unset via workflow steps
- [ ] Variable expansion works in workflow steps (`${var}`)
- [ ] CI responses can extract values into env variables
- [ ] All workflow events logged with structured format
- [ ] Logs stored in daily directories with 7-day retention
- [ ] `arc log extract` generates valid input scripts from logs
- [ ] `arc log replay` runs workflow with extracted inputs
- [ ] `arc log archive` preserves important logs
- [ ] `arc log prune` deletes old logs (respects retention period)
- [ ] Daemon auto-prunes logs daily
- [ ] All tests pass
- [ ] No memory leaks
- [ ] Compiles with `-Werror`

---

## Testing Strategy

### Unit Tests
- Config loading, precedence, merging
- Argo env set/unset/get/expand
- Variable expansion edge cases
- Log parsing and extraction
- Regex extraction from CI responses

### Integration Tests
- Config hierarchy (user overrides project overrides local)
- Workflow with argo env variables
- CI extraction → env → conditional branching
- Log extraction → input script generation
- Log replay → workflow execution

### Manual Testing
- Create workflow with user inputs
- Run interactively (logged automatically)
- Extract input script: `arc log extract wf-123`
- Replay: `arc log replay wf-123`
- Verify: same inputs used, workflow behaves identically

---

## Timeline

**Day 1** (6-7 hours):
- Config system (3 hours)
- Argo environment (4 hours)

**Day 2** (7-8 hours):
- Enhanced logging (2 hours)
- Log extraction (3 hours)
- Log replay (2 hours)
- Log management (1 hour)

**Total**: ~14 hours (2 days)

---

## Open Questions Before Implementation

1. **Log format details**:
   - Should we use key-value pairs in log lines? e.g., `[INPUT] [wf-123] prompt="..." response="..."`
   - Or structured format with delimiters?
   - How to handle multi-line values (CI responses)?

2. **Regex extraction from CI responses**:
   - What regex library? (POSIX regex? PCRE?)
   - How to handle extraction failures? (log warning, set empty value, or fail step?)
   - Support multiple captures? e.g., `extract_env: {"key1": "pattern1", "key2": "pattern2"}`

3. **Config file format**:
   - JSON only? Or support YAML/TOML?
   - How to handle nested keys in `arc config set`? (dot notation: `git.remote=origin`)

4. **Input script format**:
   - Should extracted scripts include CI responses for mock provider?
   - Should we support partial input scripts (missing values prompt user)?
   - How to handle workflows that changed since log was recorded?

5. **Replay divergence handling**:
   - What if workflow structure changed? (steps added/removed)
   - What if CI response differs from recorded? (log diff, continue, or fail?)
   - Should replay support "best effort" mode? (skip missing inputs, use defaults)

6. **Arc command structure**:
   - Should `arc log` have subcommands? `arc log extract`, `arc log replay`, etc.?
   - Or separate top-level commands? `arc extract`, `arc replay`?
   - Preference for consistency with existing `arc` patterns?

7. **Security**:
   - Should we detect/redact API keys in logs? (if so, how?)
   - Should logs be git-ignored by default? (add to .gitignore template?)

8. **Performance**:
   - Should we index logs for faster extraction? (simple index file per log?)
   - Or is linear scan sufficient? (logs are small, <100KB typically)

9. **Workflow step: argo_env**:
   - Should `argo_env` be a step type? Or integrated into existing steps?
   - Alternative: Add `set_env` field to all steps? e.g., `{"step": "display", "set_env": {"key": "value"}}`

10. **Daemon log pruning**:
    - Should pruning be configurable per workflow? (some workflows keep logs longer?)
    - Should archive/ be exempt from pruning? (assumed yes)
    - Should we log pruning actions? (what was deleted, when)

---

## Notes from Brainstorming Session

### Casey's Key Insights
- **Config in `config/` subdirectories**: Clean, organized, clear intent
- **Argo env as shell variables**: Familiar, powerful, intuitive
- **Logs ARE recordings**: Elegant, no redundant infrastructure
- **7-day retention**: Practical, sufficient, storage-friendly
- **CI sets env variables**: Enables dynamic, intelligent workflows

### Design Principles
1. **Unix philosophy**: Text is universal interface, tools are composable
2. **Familiarity**: Shell semantics reduce learning curve
3. **Simplicity**: Logs-as-recordings eliminates a whole subsystem
4. **Practicality**: Storage, retention, and workflow patterns all balance utility vs. complexity

### Key Unification
**Argo env unifies**:
- Config values (read from hierarchy)
- Workflow variables (set by steps)
- CI extractions (parsed from responses)
- User inputs (captured from prompts)

All accessed via `${variable}` expansion.

---

## Future Enhancements (Out of Scope)

- Secret detection and redaction in logs
- Response validation during replay (assert expected outputs)
- Log diff/merge tools
- GUI for replay (visual stepping)
- Export logs to different formats (JUnit XML, TAP, etc.)
- Distributed log aggregation (multi-machine workflows)
- Replay with modified inputs (edit-replay workflow)

---

**End of Sprint Plan**
