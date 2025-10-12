# Unix Workflow Pivot - Current Status

© 2025 Casey Koons. All rights reserved.

**Last Updated**: 2025-01-15 (Phase 3 complete)

## Overview

The Unix Workflow Pivot (documented in [`unix-workflow-pivot.md`](unix-workflow-pivot.md)) was a major architectural proposal to simplify Argo by replacing JSON workflows with bash scripts and the HTTP daemon with Unix socket-based AI daemons.

**Status**: **PARTIALLY IMPLEMENTED** - Core deletions done, new architecture NOT implemented yet.

---

## What Was Actually Done

### ✅ Deleted (~13,000 lines)

The old workflow engine was completely removed:

```
src/workflow/               # DELETED entire directory
  argo_workflow.c
  argo_workflow_executor.c
  argo_workflow_executor_main.c
  argo_workflow_checkpoint.c
  argo_workflow_loader.c
  argo_workflow_context.c
  argo_workflow_json.c
  argo_workflow_conditions.c
  argo_workflow_steps_*.c  # All step implementations
  argo_workflow_persona.c
  argo_workflow_input.c
  argo_provider.c
  argo_provider_messaging.c
  argo_memory.c
  argo_io_channel.c
  argo_io_channel_http.c

workflows/*.json            # DELETED all JSON workflow templates
```

**Result**: Code reduced from ~15,953 lines to ~8,735 lines.

---

## What Exists Now

### Current Architecture (Hybrid)

```
arc CLI (process manager concepts, NOT fully implemented)
  ↓ HTTP REST API (still exists)
argo-daemon (HTTP server, still exists)
  ↓ fork/exec (workflow executors DELETED)
[NO WORKFLOW EXECUTOR - deleted]
  ↓
AI providers (still exist, 9 providers working)
```

**Working Components:**
1. **Daemon with HTTP server** (`src/daemon/`)
   - `argo_daemon.c` - Core daemon
   - `argo_http_server.c` - HTTP server (~350 lines)
   - `argo_daemon_api_routes.c` - REST API handlers
   - `argo_registry.c` - CI registry (tracks CIs, not workflows)
   - `argo_lifecycle.c` - Resource lifecycle
   - `argo_workflow_registry.c` - NEW (Phase 3) - tracks workflow state

2. **Foundation** (`src/foundation/`)
   - Error, logging, HTTP client, JSON, socket utilities
   - Config loader (Phase 1) - KEY=VALUE format
   - Isolated environment (Phase 2) - `argo_env_t` for clean child processes
   - Daemon client (Phase 1b) - dynamic URL construction

3. **Providers** (`src/providers/`)
   - 9 working AI providers (claude, openai, gemini, ollama, etc.)
   - All API integrations functional

4. **Arc CLI** (`arc/`)
   - Command structure exists
   - Some commands still reference deleted workflow code (broken)
   - HTTP client for daemon communication

### Missing Components (From Pivot Plan)

1. ❌ **AI Daemon** (`src/ai_daemon/`)
   - Socket-based AI daemon NOT created
   - `argo-ai-daemon` binary NOT built
   - Context tracking NOT implemented

2. ❌ **argo-ai CLI**
   - Simple CLI wrapper for AI queries NOT created
   - Would connect to socket-based daemon

3. ❌ **Bash Workflows**
   - No bash workflow scripts created yet
   - Would replace JSON workflows (already deleted)

4. ❌ **Arc Simplification**
   - Arc still has HTTP-based workflow commands
   - Process management NOT implemented
   - Some arc commands broken (reference deleted APIs)

---

## Current Capabilities

### ✅ What Works

- **Daemon starts and runs**: `bin/argo-daemon --port 9876`
- **HTTP server**: Listens on port 9876, handles requests
- **CI Registry**: Tracks CI instances (separate from workflows)
- **Workflow Registry** (NEW): Tracks workflow execution state (Phase 3)
- **Config Loading** (NEW): Three-tier config hierarchy (Phase 1)
- **Isolated Environment** (NEW): `argo_env_t` for child processes (Phase 2)
- **All AI providers**: 9 providers work via direct API calls
- **Arc CLI**: Basic structure exists

### ❌ What Doesn't Work

- **Workflow execution**: No workflow executor (deleted)
- **Arc workflow commands**: Reference deleted APIs
- **JSON workflows**: Deleted, not replaced with bash yet
- **AI daemon sockets**: Not created (plan only)
- **Process management**: Not implemented in arc

---

## Phases Completed

### Phase 1: Config Hierarchy ✅ (Completed)
- Three-tier config: `~/.argo/config/`, `<project>/.argo/config/`, `workflows/config/`
- KEY=VALUE format (not shell scripts as originally planned)
- Functions: `argo_config()`, `argo_config_get()`, `argo_config_reload()`
- 7 tests passing

### Phase 1b: Remote Daemon Support ✅ (Completed)
- Dynamic URL construction: `argo_get_daemon_url()`
- Configuration precedence: Config → env → defaults
- Programming guideline added (Check #12)
- Full documentation

### Phase 2: Isolated Environment ✅ (Completed)
- `argo_env_t` structure for environment management
- Clean environment passing to child processes
- `argo_spawn_with_env()` using execve()
- 10 tests passing

### Phase 3: Workflow Registry ✅ (Completed)
- `workflow_registry_t` for persistent workflow tracking
- States: PENDING, RUNNING, COMPLETED, FAILED, ABANDONED
- JSON persistence capability
- 11 tests passing

### Phase 4: Daemon-Private Registry ✅ (Already Achieved)
- CI registry only used within daemon (no external usage)
- No refactoring needed

---

## Implementation Decision

**Current Direction**: Keep hybrid architecture, NOT full Unix pivot.

**Rationale**:
1. **Workflow deletion was intentional** - old JSON engine was too complex
2. **HTTP daemon is good** - works well, well-tested, production-ready
3. **Socket-based daemon NOT needed** - HTTP provides same benefits
4. **Bash workflows CAN be added** - without deleting HTTP daemon
5. **Incremental evolution** - add bash support alongside existing architecture

**Path Forward**:
1. Keep HTTP daemon (working well)
2. Fix broken arc commands (reference deleted APIs)
3. Add bash workflow support (new feature)
4. Keep AI providers as-is (working)
5. Document current architecture accurately

---

## Next Steps (If Continuing Pivot)

If user wants to complete the full Unix pivot as originally planned:

### Phase 5: AI Daemon (Not Started)
- [ ] Create `src/ai_daemon/` directory
- [ ] Implement Unix socket server
- [ ] Add context tracking
- [ ] Build `argo-ai-daemon` binary
- [ ] Build `argo-ai` CLI wrapper

### Phase 6: Bash Workflows (Not Started)
- [ ] Create example bash workflows
- [ ] Implement `arc start workflow.sh`
- [ ] Add PID tracking in registry
- [ ] Implement `arc list`, `arc logs`, `arc stop`

### Phase 7: Arc Simplification (Not Started)
- [ ] Remove HTTP workflow commands
- [ ] Implement process management
- [ ] Simplify to shell script runner

**Estimated Effort**: 2-3 days (if resuming pivot)

---

## Documentation Status

### ✅ Accurate Documentation
- `CLAUDE.md` - Current architecture well-documented
- `docs/plans/unix-workflow-pivot.md` - Pivot plan (aspirational)
- `docs/plans/PIVOT_STATUS.md` - THIS FILE (current state)

### ⚠️ Needs Update
- `docs/guide/DAEMON_ARCHITECTURE.md` - References deleted workflow executor
- `docs/guide/WORKFLOW.md` - References deleted JSON workflows
- `docs/guide/WORKFLOW_CONSTRUCTION.md` - References deleted APIs
- Project `README.md` - May reference deleted features

### ❌ Broken References
- Any docs referring to:
  - `argo_workflow_executor` binary
  - `libargo_workflow.a` library
  - JSON workflow templates
  - HTTP I/O channel for workflows
  - Workflow step implementations

---

## Conclusion

The Unix pivot **deleted** the old workflow engine successfully, but **did not implement** the new socket-based architecture. Current state is a **minimal daemon** with AI providers, suitable for building either:

1. **Option A**: Complete the Unix pivot (socket daemons, bash workflows)
2. **Option B**: Build new workflow system on existing HTTP daemon
3. **Option C**: Use as-is for direct AI provider access only

**Recommendation**: Option B - leverage working HTTP daemon, add bash workflow support incrementally.

---

*This document tracks the actual implementation status vs. the original pivot plan.*
