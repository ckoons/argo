# Enhancement Plan: Workflow Scripting

**Status**: Planning
**Created**: 2025-01-11
**Target**: Near-term sprint (1-2 days)

## Background

This enhancement draws from Casey's experience building network management systems where engineers could:
- Record interactive sessions automatically (always-on, not a special mode)
- Replay sessions to debug instead of reading logs
- Convert recordings to reusable scripts with text editor
- Protect secrets automatically during recording
- Run workflows with hybrid input (script + user interaction)

## Goals

Enable workflows to be:
1. **Scriptable**: Run with pre-defined inputs (testing, automation, CI/CD)
2. **Recordable**: Automatically capture all sessions for replay/debugging
3. **Hybrid**: Mix scripted inputs with user interaction
4. **Configurable**: Use persistent config values across workflows
5. **Replayable**: Debug by watching what actually happened

## Core Capabilities

### 1. Input Scripts + User Input (Hybrid Mode)

**Feature**: Workflows can use inputs from scripts, config, or user prompts in priority order.

**Usage**:
```bash
# Run with partial input script
arc start code_review issue-123 --input-script=inputs.json

# Run with inputs from previous recording
arc start code_review issue-456 --use-inputs-from=issue-123
```

**Input script format** (semantic keys matching workflow `save_to` fields):
```json
{
  "branch_name": "main",
  "auto_merge": "yes"
  // Missing values will prompt user interactively
}
```

**Workflow behavior**:
```
For each user_ask step:
  1. Check input script for value (by save_to key)
  2. If not in script, check config defaults
  3. If still not found, prompt user interactively
  4. Record actual value used
```

### 2. Always-On Session Recording

**Feature**: Every workflow execution automatically records all inputs/outputs.

**Storage**: `~/.argo/recordings/<workflow_name>/<instance_id>/`
```
recordings/
  └── code_review/
      ├── issue-123/
      │   ├── session.json       # Full recording with timestamps
      │   ├── inputs.json        # Extracted user inputs only
      │   └── metadata.json      # Start/end times, result, duration
      └── issue-456/
          └── ...
```

**Recording format** (`session.json`):
```json
{
  "workflow_name": "code_review",
  "instance_id": "issue-123",
  "started_at": "2025-01-11T14:30:00Z",
  "completed_at": "2025-01-11T14:35:22Z",
  "result": "success",
  "inputs": {
    "step_2": {
      "save_to": "branch_name",
      "prompt": "Enter branch name:",
      "value": "feature/api",
      "source": "user",
      "timestamp": "2025-01-11T14:30:05Z"
    },
    "step_5": {
      "save_to": "confirm_merge",
      "prompt": "Confirm merge (yes/no):",
      "value": "yes",
      "source": "user",
      "timestamp": "2025-01-11T14:34:50Z"
    }
  }
}
```

**Extracted inputs** (`inputs.json` - directly reusable as input script):
```json
{
  "branch_name": "feature/api",
  "confirm_merge": "yes"
}
```

### 3. Configuration System

**Feature**: Persistent key-value config storage (user, project, and script scope).

**Usage**:
```bash
# Set simple values
arc config default_branch=main
arc config auto_merge=false

# Set JSON structures
arc config git_settings='{"remote":"origin","push":true}'

# Set arrays
arc config reviewers='["casey","alice","bob"]'

# Read config
arc config default_branch
# Output: main

# List all config
arc config --list

# Delete config key
arc config --delete default_branch
```

**Storage locations** (priority order):
1. **Script level**: In input script `config` section (highest priority)
2. **Project level**: `./.argo/config.json` (project-specific)
3. **User level**: `~/.argo/config.json` (global defaults)

**Workflow usage**:
```json
{
  "step": 2,
  "type": "user_ask",
  "prompt": "Enter branch name:",
  "save_to": "branch_name",
  "default": "{{config.default_branch}}"
}
```

**Variable substitution**:
- `{{config.key}}` - Read from config (any scope)
- `{{config.nested.key}}` - Nested JSON values
- `{{variable}}` - Workflow context variables

**Config scoping** (all three levels can coexist):
```json
// ~/.argo/config.json (user defaults)
{
  "default_branch": "main",
  "editor": "vim"
}

// ./.argo/config.json (project overrides)
{
  "default_branch": "develop",
  "reviewers": ["alice", "bob"]
}

// Input script (workflow-specific)
{
  "config": {
    "default_branch": "feature/api"
  },
  "inputs": {
    "auto_merge": "yes"
  }
}
```

### 4. Session Replay

**Feature**: Re-run any workflow using its recording as input.

**Usage**:
```bash
# Replay exact session
arc replay code_review issue-123

# Use recording as input for new workflow
arc start code_review issue-456 --use-inputs-from=issue-123

# Replay with validation (future)
arc replay code_review issue-123 --validate
```

**Replay behavior**:
- Uses recorded inputs automatically
- Skips user prompts (runs non-interactively)
- Outputs comparison (future: validate responses match)

### 5. Recording Management

**Commands**:
```bash
# List recordings
arc recording list [workflow_name]

# Show recording details
arc recording show <workflow_name> <instance_id>

# Export recording as input script
arc recording export <workflow_name> <instance_id> [output_file]

# Delete recording
arc recording delete <workflow_name> <instance_id>

# Clean old recordings
arc recording clean --older-than=2d
```

**Automatic cleanup**:
- Shared services runs cleanup daily
- Default retention: 2 days for recordings in `recordings/` directory
- Recordings moved to `recordings/archive/` are preserved
- Manual deletion always available

## Input Priority and Matching

### Priority Order (Highest to Lowest)
1. Input script (explicit `--input-script=file.json`)
2. Config - script level (in input script `config` section)
3. Config - project level (`./.argo/config.json`)
4. Config - user level (`~/.argo/config.json`)
5. Workflow defaults (`default` field in step)
6. User prompt (interactive)

### Matching Strategy
**Use semantic keys** (Option C) - match by `save_to` field name.

**Why semantic keys?**
- Stable (variable names don't change often)
- Readable (self-documenting)
- Simple string matching (exact match, no regex)
- Matches existing workflow field

**Example**:
```json
// Workflow step
{
  "step": 2,
  "type": "user_ask",
  "prompt": "Enter branch name:",
  "save_to": "branch_name",
  "next_step": 3
}

// Input script (matches by save_to)
{
  "branch_name": "main"
}
```

**Fallback behavior**:
- If `save_to` not found in input script → check config
- If not in config → use workflow `default` if present
- If no default → prompt user
- Always record actual value used

## Implementation Plan

### Phase 1: Core Infrastructure (Day 1)

**Goal**: Input scripts work, recordings stored, config system functional.

#### Task 1.1: Config System
- [ ] Create `argo_config.h` and `argo_config.c`
- [ ] Functions: `config_load()`, `config_get()`, `config_set()`, `config_save()`
- [ ] Support nested JSON access (`config.git.remote`)
- [ ] Support three scopes: user, project, script
- [ ] Add `arc config` command to CLI

#### Task 1.2: Input Script Support
- [ ] Add `--input-script=PATH` flag to `arc start`
- [ ] Load input script JSON in workflow executor
- [ ] Store in workflow context as `input_script`
- [ ] Update `step_user_ask()` to check input script first
- [ ] Fall through to user prompt if not found

#### Task 1.3: Recording Infrastructure
- [ ] Create recording directory structure on workflow start
- [ ] Add recording writer functions: `recording_start()`, `recording_add_input()`, `recording_finalize()`
- [ ] Log all user inputs with timestamps
- [ ] Generate `inputs.json` on workflow completion
- [ ] Store in `~/.argo/recordings/<workflow>/<instance>/`

#### Task 1.4: Variable Substitution for Config
- [ ] Extend `workflow_context_substitute()` to handle `{{config.key}}`
- [ ] Load config at workflow start
- [ ] Merge all three scopes (user, project, script)
- [ ] Substitute in `default` fields

### Phase 2: Replay and Management (Day 2)

**Goal**: Replay works, recording management commands available.

#### Task 2.1: Replay Functionality
- [ ] Add `arc replay <workflow> <instance>` command
- [ ] Load recording from `~/.argo/recordings/<workflow>/<instance>/inputs.json`
- [ ] Pass as `--input-script` to workflow executor
- [ ] Run non-interactively

#### Task 2.2: Use Inputs From
- [ ] Add `--use-inputs-from=<instance>` flag to `arc start`
- [ ] Copy `inputs.json` from source recording
- [ ] Use as input script for new workflow

#### Task 2.3: Recording Management Commands
- [ ] `arc recording list [workflow]`
- [ ] `arc recording show <workflow> <instance>`
- [ ] `arc recording export <workflow> <instance> [file]`
- [ ] `arc recording delete <workflow> <instance>`
- [ ] `arc recording clean --older-than=<duration>`

#### Task 2.4: Automatic Cleanup
- [ ] Add cleanup task to shared services
- [ ] Run daily
- [ ] Delete recordings older than 2 days (configurable)
- [ ] Skip `recordings/archive/` directory
- [ ] Log cleanup actions

### Phase 3: Polish and Testing (Continuous)

#### Task 3.1: Testing
- [ ] Unit tests for config system
- [ ] Unit tests for recording writer
- [ ] Integration test: workflow with input script
- [ ] Integration test: replay recorded session
- [ ] Integration test: config priority order
- [ ] Test cleanup policy

#### Task 3.2: Documentation
- [ ] Update workflow documentation with input script examples
- [ ] Document config system usage
- [ ] Document recording format
- [ ] Add examples for common use cases

## File Structure

### New Files
```
src/
  ├── argo_config.c                    # Config system (load/save/merge)
  └── argo_recording.c                 # Recording writer

include/
  ├── argo_config.h                    # Config API
  └── argo_recording.h                 # Recording API

arc/src/
  ├── config_cmd.c                     # arc config command
  └── recording_cmd.c                  # arc recording commands

tests/
  ├── test_config.c                    # Config system tests
  └── test_recording.c                 # Recording tests
```

### Modified Files
```
src/workflow/argo_workflow_executor.c        # Load input script, initialize recording
src/workflow/argo_workflow_executor_main.c   # Parse --input-script flag
src/workflow/argo_workflow_steps_basic.c     # Check input script in step_user_ask
src/workflow/argo_workflow_context.c         # Add config variable substitution
arc/src/workflow_start.c                     # Add --input-script and --use-inputs-from flags
arc/src/main.c                               # Register new commands
src/daemon/argo_shared_services.c            # Add recording cleanup task
```

### Storage Locations
```
~/.argo/
  ├── config.json                    # User-level config
  └── recordings/                    # All recordings
      ├── <workflow_name>/
      │   └── <instance_id>/
      │       ├── session.json       # Full recording
      │       ├── inputs.json        # Extracted inputs (reusable)
      │       └── metadata.json      # Timestamps, result
      └── archive/                   # Preserved recordings (not auto-deleted)

./.argo/
  └── config.json                    # Project-level config
```

## Success Criteria

- [ ] Can run workflow with input script
- [ ] Can run workflow with partial input script (prompts for missing values)
- [ ] All workflow sessions recorded automatically
- [ ] Can replay any workflow from its recording
- [ ] Can set/get config values at user and project level
- [ ] Config values work in workflow `default` fields
- [ ] Config priority order works (script > project > user)
- [ ] `arc recording` commands list/show/export/delete work
- [ ] Automatic cleanup runs daily, removes recordings older than 2 days
- [ ] All tests pass
- [ ] No memory leaks

## Future Enhancements (Out of Scope for This Sprint)

- Secret detection and redaction
- Response validation during replay
- Recording diff/merge tools
- GUI for non-engineers
- Complex validation rules (regex patterns)
- Recording editing UI
- Export to different formats

## Open Questions

1. **Input script format for nested workflows**: How do we handle `workflow_call` steps?
2. **Recording size limits**: Should we truncate large outputs?
3. **Concurrent workflows**: How do we handle multiple workflows updating config?
4. **Config locking**: Do we need file locking for config writes?
5. **Replay divergence**: What happens if workflow structure changed since recording?

## Design Decisions

### Why Semantic Keys (save_to)?
- **Stable**: Variable names rarely change
- **Readable**: Self-documenting
- **Simple**: Exact string match, no regex
- **Consistent**: Already exists in workflow JSON

### Why Always-On Recording?
- **No friction**: Engineers don't forget to turn it on
- **Debugging**: Replay instead of reading logs
- **Script creation**: Sessions naturally become scripts
- **Audit trail**: See exactly what happened

### Why Three Config Scopes?
- **User**: Personal defaults (editor, name, preferences)
- **Project**: Team settings (branch policy, reviewers)
- **Script**: Workflow-specific overrides (test vs. prod)

### Why 2-Day Recording Retention?
- **Debugging window**: Enough time to investigate issues
- **Disk space**: Prevents unbounded growth
- **Archive option**: Important recordings moved to `archive/`
- **Configurable**: Can be changed per installation

## Notes from Discussion

### Casey's Network Management System
- Recording was always on (not a special mode)
- Engineers could replay and watch instead of reading logs
- System was dashboard orchestrating multiple management systems
- Secrets automatically protected during recording
- Text editor used to convert recordings to scripts
- System name: [To be recalled]

### Key Principles
1. Don't make engineers remember to turn on recording
2. Replay is better debugging than log files
3. Scripts should emerge naturally from work
4. Simple string matching beats regex for maintenance
5. Humans need to understand the matching logic

## Timeline

**Day 1**: Core infrastructure (config, input scripts, recording)
**Day 2**: Replay and management commands
**Continuous**: Testing and documentation

**Total Estimated Effort**: 2 days

---

## Appendix: Current Testing Capabilities Analysis

*Analysis conducted 2025-01-11 to understand what automated workflow testing is possible before and after this enhancement.*

### Current State: What Works NOW

#### ✅ Fully Automated Tests (No Changes Needed)

**1. Non-Interactive Workflows**
- **Works**: Any workflow with only `display` steps
- **Examples**: `test_basic_steps.json`, `test_variable_flow.json`, `simple_test.json`
- **Execution**: `bin/argo_workflow_executor <id> <template.json> main`
- **Exit codes**: 0 = success, non-zero = failure
- **Current coverage**: Shell script `tests/integration/test_workflows.sh` tests 2 workflows

**2. Unit Tests**
- **File**: `tests/integration/test_workflow_scripting.c`
- **Coverage**: 38 tests, all passing
- **Tests**: Variable substitution, escape sequences, context management, JSON parsing
- **Execution**: `bin/tests/test_workflow_scripting`

#### ❌ Cannot Test NOW (Blocked)

**1. Workflows with User Input (`user_ask` steps)**
- **Problem**: Requires HTTP I/O channel with arc CLI
- **Blocks on**: `http_io_prompt_user()` waits for HTTP request
- **Examples**: `test_interactive_chat.json`, `test_context_display.json`
- **Why blocked**: No way to provide input without interactive session

**2. Workflows with CI Steps**
- **Problem**: Requires actual AI provider connection
- **Step types**: `ci_ask`, `ci_analyze`, `ci_ask_series`, `ci_present`, `user_ci_chat`
- **Issues**:
  - Requires API keys or Claude Code CLI
  - Makes actual network calls
  - Non-deterministic responses
  - Slow (seconds per query)
  - Costs money (API calls)
- **Examples**: `test_interactive_chat.json`, `simple_user_ci_chat.json`

### Test Coverage Matrix

| Workflow Type | Example | Testable Now? | Blocker |
|---------------|---------|---------------|---------|
| Display only | `test_basic_steps.json` | ✅ YES | None |
| Display + variables | `test_variable_flow.json` | ✅ YES | None |
| User input | `test_context_display.json` | ❌ NO | `user_ask` blocks |
| CI query | `simple_user_ci_chat.json` | ❌ NO | Real AI + user input |
| Chat loop | `test_interactive_chat.json` | ❌ NO | Real AI + user loop |

### What COULD Be Tested Today (No Code Changes)

**1. More Display-Only Workflows**
- Escape sequences edge cases
- Variable substitution edge cases (missing vars, nested `{{}}`)
- Step flow (forward, conditional branches)
- Multiple phases
- Long workflows (100+ steps)

**2. Negative Tests**
- Missing required fields
- Invalid JSON
- Circular step references
- Unknown step types
- Invalid next_step IDs

**3. Extended Shell Script Tests**
- Test all display-only workflows
- Verify output contains expected strings
- Check exit codes
- Time execution
- Memory leak detection (valgrind)

### Quick Wins (Before Full Enhancement)

**1. Mock CI Provider** (~1 hour)
- Create mock provider returning canned responses
- Enables testing all CI step types
- Fast, deterministic, no API keys needed
- No network calls

**2. More Test Workflows** (~30 minutes)
- Create comprehensive display-only test suite
- Better coverage of edge cases
- Test error handling

**3. Negative Test Cases** (~1 hour)
- Validate error handling
- Ensure workflows fail gracefully
- Test boundary conditions

### After Enhancement: What Becomes Testable

**With Input Scripts (Phase 1)**:
- ✅ All `user_ask` workflows (scripted inputs)
- ✅ Hybrid workflows (partial scripts + user interaction)
- ✅ Replay of recorded sessions
- ✅ CI/CD integration

**With Mock CI Provider**:
- ✅ All CI step types (without real API)
- ✅ Chat loops (deterministic responses)
- ✅ Error handling for CI failures
- ✅ Full workflow coverage

**Combined**:
- ✅ **100% automated testing possible**
- ✅ Fast test suite (no network calls)
- ✅ Deterministic tests (no flaky AI responses)
- ✅ CI/CD friendly (no API keys needed)

### Recommendations for Sprint Planning

**Priority 1: Input Scripts** (This Enhancement)
- **Unlocks**: User input testing, replay, automation
- **Impact**: High - makes most workflows testable
- **Effort**: 2 days

**Priority 2: Mock CI Provider** (Quick Add)
- **Unlocks**: CI step testing without real API
- **Impact**: High - completes test coverage
- **Effort**: 1-2 hours
- **Can be done during or after sprint**

**Priority 3: Test Workflow Suite** (Ongoing)
- **Unlocks**: Better coverage, edge cases, regression detection
- **Impact**: Medium - validates existing functionality
- **Effort**: Incremental (add as needed)

### Testing Strategy Post-Enhancement

**1. Fast Unit Tests** (seconds)
- Variable substitution
- Context management
- JSON parsing
- Config system
- Recording writer

**2. Integration Tests** (< 1 minute)
- Display-only workflows
- Input script workflows
- Config priority testing
- Recording/replay

**3. Mock CI Tests** (< 5 minutes)
- All CI step types
- Chat loops
- Error handling
- Full workflow coverage

**4. Manual/Exploratory** (as needed)
- Real AI interaction
- User experience
- Edge cases
- New features

### Key Insight for Sprint

**This enhancement (input scripts + recording) is the foundation for automated testing.** Without it, we can only test ~20% of workflows (display-only). With it, we can test 100% of workflows (with mock CI provider).

**Testing becomes a first-class use case**, not an afterthought. Every workflow can be:
1. Developed interactively (normal workflow)
2. Recorded automatically (no extra work)
3. Converted to test (edit `inputs.json`)
4. Run in CI/CD (fast, deterministic)

This aligns with the original network management system philosophy: **scripts emerge naturally from work**, including test scripts.
