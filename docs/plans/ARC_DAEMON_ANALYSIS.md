# Arc/Daemon Integration Analysis

© 2025 Casey Koons. All rights reserved.

**Date**: 2025-01-15
**Updated**: 2025-10-12
**Status**: ✅ IMPLEMENTATION COMPLETE

---

## Question

Should we update arc/ to use the new argo-daemon code, or do we need to complete the argo-daemon first?

---

## Analysis Summary

### Current State

**argo-daemon:**
- ✅ **Builds successfully**: `bin/argo-daemon` compiles cleanly
- ✅ **HTTP server works**: `argo_http_server.c` (~350 lines, production-ready)
- ✅ **Basic infrastructure exists**: daemon core, registry, lifecycle
- ❌ **NO workflow API routes**: Deleted during Unix pivot, not replaced yet
- ❌ **NO workflow execution**: Old executor deleted, new bash approach not implemented

**arc CLI:**
- ❌ **Does NOT build**: Multiple compilation errors
- ⚠️ **Mixed state**: Some commands use HTTP (good), some use deleted APIs (broken)
- ⚠️ **References deleted code**: `workflow_instance_t`, `workflow_registry_get_workflow()`

---

## Detailed Findings

### 1. What Arc Commands Exist

**HTTP-Based Commands** (Ready for daemon integration):
```
arc/src/workflow_start.c        ✅ Uses arc_http_post()
arc/src/workflow_status.c       ✅ Uses arc_http_get()
arc/src/workflow_list.c         ✅ Uses arc_http_get()
arc/src/workflow_pause.c        ✅ Uses arc_http_post()
arc/src/workflow_resume.c       ✅ Uses arc_http_post()
arc/src/workflow_states.c       ✅ Uses arc_http_get()
arc/src/workflow_attach.c       ✅ Uses arc_http_post() (for input)
```

**Broken Commands** (Reference deleted APIs):
```
arc/src/cmd_switch.c            ❌ Uses old workflow_registry API
arc/src/workflow_attach.c       ❌ Uses workflow_instance_t (deleted type)
```

### 2. What Daemon API Routes Exist

**Current** (`src/daemon/argo_daemon_api_routes.c`):
```c
GET /api/registry/ci            ✅ Implemented (returns empty list)
```

**Missing** (Arc expects these):
```c
POST   /api/workflow/start           ❌ NOT implemented (arc calls this)
GET    /api/workflow/list            ❌ NOT implemented
GET    /api/workflow/status/{id}     ❌ NOT implemented
POST   /api/workflow/pause/{id}      ❌ NOT implemented
POST   /api/workflow/resume/{id}     ❌ NOT implemented
DELETE /api/workflow/abandon/{id}    ❌ NOT implemented
POST   /api/workflow/input/{id}      ❌ NOT implemented (attach uses this)
GET    /api/workflow/output/{id}     ❌ NOT implemented
```

**Comment in daemon code**:
```c
/* TODO: Unix pivot - workflow API routes removed, will be replaced with bash-based workflows */
```

### 3. What's Broken

**Arc Build Errors** (7 errors in `cmd_switch.c`):
```c
// Error 1-2: API signature mismatch
workflow_registry_create(path)      // Old API (deleted)
workflow_registry_create()          // New API (Phase 3)

workflow_registry_load(reg)         // Old API (deleted)
workflow_registry_load(reg, path)   // New API (Phase 3)

// Error 3-7: Deleted types
workflow_instance_t                 // Type deleted (no replacement)
workflow_registry_get_workflow()    // Function deleted (no replacement)
```

**Root Cause**:
- Arc code written for OLD workflow_registry (pre-pivot)
- Phase 3 created NEW workflow_registry with different API
- No `workflow_instance_t` exists anymore (workflows don't have "instances" in new design)

### 4. What's Missing

**To make Arc work, daemon needs:**

1. **Workflow API Routes** (~300-500 lines needed):
   ```c
   POST   /api/workflow/start       // Start bash workflow script
   GET    /api/workflow/list        // List running PIDs
   GET    /api/workflow/status/{id} // Get workflow state from registry
   DELETE /api/workflow/abandon/{id}// Kill workflow PID
   POST   /api/workflow/input/{id}  // Queue user input (for interactive scripts)
   GET    /api/workflow/output/{id} // Poll output queue
   ```

2. **Workflow Execution** (bash-based):
   - Fork bash script (replace old JSON executor)
   - Track PID in workflow_registry
   - Stream logs to `~/.argo/logs/{id}.log`
   - Handle SIGTERM for abandon

3. **Arc Fixes** (~50-100 lines):
   - Remove `cmd_switch.c` references to deleted API
   - Remove `workflow_attach.c` references to `workflow_instance_t`
   - Use HTTP endpoints consistently

---

## Answer: What Should We Do?

### Option A: Complete Daemon First ⭐ **RECOMMENDED**

**Implement minimal workflow API in daemon, THEN fix arc.**

**Why:**
1. Arc expects daemon API to exist
2. Can't test arc without working daemon endpoints
3. Bash workflow approach is simpler than fixing old arc code
4. Clean break from old architecture

**Work Required:**
1. Implement daemon workflow API routes (~300 lines)
2. Add bash script execution (fork/exec)
3. Wire to workflow_registry (Phase 3)
4. Fix/remove broken arc commands

**Estimated Effort**: 4-6 hours

---

### Option B: Fix Arc First

**Fix arc compilation errors, make HTTP calls, hope daemon implements API later.**

**Why NOT:**
1. Arc will still fail at runtime (no daemon endpoints)
2. Wasted effort fixing code that references deleted concepts
3. Can't test anything until daemon is ready
4. Perpetuates confusion about deleted vs. new workflow_registry

**Not Recommended**: Blocks on daemon anyway.

---

### Option C: Delete Broken Arc Commands

**Remove `cmd_switch.c` and broken parts of `workflow_attach.c`.**

**Why:**
1. Quick cleanup (10 minutes)
2. Arc will compile
3. Removes confusion about deleted APIs
4. Can add back later with correct API

**Recommended as STEP 1** before Option A.

---

## Recommended Implementation Plan

### Phase 1: Cleanup (10 minutes)

**Delete or stub out broken arc commands:**
```bash
# Option 1: Delete entirely
rm arc/src/cmd_switch.c
rm arc/src/workflow_attach.c

# Option 2: Stub out broken sections
# Comment out workflow_registry usage in these files
```

**Result**: Arc compiles cleanly.

---

### Phase 2: Minimal Daemon Workflow API (4-6 hours)

**Implement in daemon:**

1. **POST /api/workflow/start** (~100 lines)
   ```c
   // Parse JSON: {"script": "path/to/workflow.sh", "args": [...]}
   // Fork bash script
   // Track PID in workflow_registry
   // Return workflow_id
   ```

2. **GET /api/workflow/list** (~50 lines)
   ```c
   // Call workflow_registry_list()
   // Return JSON array of workflows
   ```

3. **GET /api/workflow/status/{id}** (~50 lines)
   ```c
   // Call workflow_registry_find(id)
   // Check if PID still running
   // Return JSON with state
   ```

4. **DELETE /api/workflow/abandon/{id}** (~50 lines)
   ```c
   // Find workflow PID
   // kill(pid, SIGTERM)
   // Update workflow_registry state to ABANDONED
   ```

5. **Bash Script Execution** (~100 lines)
   ```c
   int daemon_execute_bash_script(const char* script_path,
                                  char** args,
                                  const char* workflow_id) {
       // Fork process
       // Redirect stdout/stderr to log file
       // execv(bash_path, [bash, script_path, ...args])
       // Parent: add to workflow_registry with PID
   }
   ```

**Result**: Daemon has working workflow API.

---

### Phase 3: Fix Arc Commands (2 hours)

1. **Remove deleted API usage**:
   - Delete `cmd_switch.c` (or rewrite to use HTTP)
   - Fix `workflow_attach.c` to not use `workflow_instance_t`

2. **Verify HTTP commands work**:
   - Test `arc workflow start workflow.sh`
   - Test `arc workflow list`
   - Test `arc workflow status <id>`
   - Test `arc workflow abandon <id>`

**Result**: Arc works end-to-end with bash workflows.

---

## What Stays The Same

**No changes needed for:**
- ✅ **HTTP server** - works perfectly
- ✅ **CI registry** - daemon-only, working
- ✅ **Workflow registry** - Phase 3, ready to use
- ✅ **Config system** - Phase 1, working
- ✅ **Isolated environment** - Phase 2, ready for bash scripts
- ✅ **AI providers** - all 9 working

---

## Code Size Impact

**Additions:**
- Daemon workflow API routes: ~300 lines
- Bash execution: ~100 lines
- **Total**: ~400 lines

**Deletions:**
- Broken arc commands: ~200-300 lines
- **Net**: ~100-200 lines added

**Still under budget**: 8,735 + 200 = ~8,935 lines (well under 10K)

---

## Testing Strategy

### Step 1: Test Daemon API
```bash
# Start daemon
bin/argo-daemon --port 9876

# Test workflow start
curl -X POST http://localhost:9876/api/workflow/start \
  -d '{"script":"workflows/hello.sh","args":[]}'

# Test workflow list
curl http://localhost:9876/api/workflow/list

# Test workflow status
curl http://localhost:9876/api/workflow/status/hello_1

# Test abandon
curl -X DELETE http://localhost:9876/api/workflow/abandon/hello_1
```

### Step 2: Test Arc CLI
```bash
# Test via arc
arc workflow start workflows/hello.sh
arc workflow list
arc workflow status hello_1
arc workflow abandon hello_1
```

---

## Risks & Mitigations

### Risk 1: Bash workflow approach not well-defined

**Mitigation**: Start with simplest possible implementation:
- Just fork bash script
- Track PID
- Stream logs
- Kill on abandon

Can enhance later (context, input/output queues, etc.)

### Risk 2: Arc expects features we haven't built

**Mitigation**: Implement only what arc actually uses:
- Start (POST)
- List (GET)
- Status (GET)
- Abandon (DELETE)

Skip pause/resume/attach for now.

### Risk 3: Breaking changes to arc

**Mitigation**:
- Document that this is post-pivot architecture
- Arc commands may change
- Consider this "alpha" quality for now

---

## Conclusion

**Answer**: **Complete daemon first** (Option A + Option C).

**Sequence**:
1. **Delete/stub broken arc commands** (10 min)
2. **Implement daemon workflow API** (4-6 hours)
3. **Fix remaining arc commands** (2 hours)
4. **Test end-to-end** (1 hour)

**Total**: 7-9 hours of work

**Why this order:**
- Arc can't work without daemon API
- Can test daemon API independently (curl)
- Clean break from old architecture
- Minimal code, maximum functionality

---

*Analysis complete. Ready to proceed with Option A when approved.*
