# Output Standardization Proposal

## Problem Statement

Currently, different parts of the system use different output methods:
- **Tests:** Use `fprintf()` directly (acceptable - ancillary code)
- **Library code (src/):** Mix of `LOG_INFO/LOG_ERROR` and direct `fprintf(stderr)`
- **CLI (arc/):** Direct `fprintf(stderr, ...)` for user messages
- **Workflow executor (bin/):** Direct `fprintf(stdout/stderr)` to log files
- **UI (argo-term):** Various output methods

**Goal:** Create a single, consistent output interface for all user-facing components.

---

## Proposed Solution

### 1. **Arc CLI Output** (`arc/include/arc_output.h`)

Create standardized macros for arc commands:

```c
/* © 2025 Casey Koons All rights reserved */

#ifndef ARC_OUTPUT_H
#define ARC_OUTPUT_H

#include <stdio.h>

/* User information messages (goes to stderr for consistency) */
#define ARC_INFO(fmt, ...) \
    fprintf(stderr, fmt, ##__VA_ARGS__)

/* Error messages with context */
#define ARC_ERROR(fmt, ...) \
    fprintf(stderr, "Error: " fmt, ##__VA_ARGS__)

/* Warning messages */
#define ARC_WARN(fmt, ...) \
    fprintf(stderr, "Warning: " fmt, ##__VA_ARGS__)

/* Success messages */
#define ARC_SUCCESS(fmt, ...) \
    fprintf(stderr, fmt, ##__VA_ARGS__)

/* Status/table output (may go to stdout if desired) */
#define ARC_STATUS(fmt, ...) \
    printf(fmt, ##__VA_ARGS__)

#endif /* ARC_OUTPUT_H */
```

**Usage in arc commands:**
```c
// Before:
fprintf(stderr, "Error: Workflow not found: %s\n", workflow_name);
fprintf(stderr, "Started workflow: %s\n", workflow_id);

// After:
ARC_ERROR("Workflow not found: %s\n", workflow_name);
ARC_SUCCESS("Started workflow: %s\n", workflow_id);
```

### 2. **Workflow Executor Output** (`bin/`)

The executor writes to stdout/stderr which are redirected to log files.
This is **intentional** and should remain as direct fprintf():

```c
// Executor logs - these go to workflow log files
fprintf(stdout, "Step %d/%d: %s\n", ...);  // KEEP AS-IS
fprintf(stderr, "Failed to open template: %s\n", ...);  // KEEP AS-IS
```

**Rationale:** Log files need raw output, not formatted with prefixes.

### 3. **Library Code** (`src/`)

Use existing argo logging:
- `LOG_INFO()` - Informational logging
- `LOG_ERROR()` - Error logging (non-fatal)
- `argo_report_error()` - Error codes with context

**For child process errors** (like fork failures), direct fprintf is acceptable:
```c
// Child process in fork - can't use logging system
fprintf(stderr, "Failed to execute: %s\n", strerror(errno));
```

### 4. **UI Code** (`ui/argo-term`)

Should use similar pattern to arc:
```c
#include "ui_output.h"  // Similar macros to arc_output.h

UI_INFO("Connection established\n");
UI_ERROR("Failed to connect: %s\n", error);
UI_STATUS("Workflow progress: %d%%\n", percent);
```

---

## Implementation Plan

### Phase 1: Arc CLI (Immediate)
1. Create `arc/include/arc_output.h` with macros
2. Update all arc command files to use macros:
   - `workflow_start.c`
   - `workflow_pause.c`
   - `workflow_resume.c`
   - `workflow_abandon.c`
   - `workflow_status.c`
   - `workflow_list.c`

### Phase 2: Verify bin/ (No changes needed)
- Confirm executor output is intentionally raw for log files
- Document that bin/ executables use direct fprintf

### Phase 3: Review src/ (Audit)
- Ensure all library code uses `LOG_*` or `argo_report_error()`
- Only exception: child process error messages

### Phase 4: UI Code (Future)
- Create `ui/include/ui_output.h` when needed
- Apply to argo-term when developed

---

## Benefits

✅ **Single point of control** - Change output format in one place
✅ **Consistent prefixes** - "Error:", "Warning:", etc.
✅ **Easy debugging** - Can add timestamps, colors, etc. in macros
✅ **Clear separation** - CLI vs logs vs library
✅ **Grep-friendly** - All errors have "Error:" prefix
✅ **Future-proof** - Can add log levels, file output, etc.

---

## Example Refactor

**Before** (`arc/src/workflow_start.c`):
```c
fprintf(stderr, "Error: Failed to initialize argo\n");
fprintf(stderr, "Error: Template not found: %s\n", template_name);
fprintf(stderr, "  Try: arc workflow list template\n");
fprintf(stderr, "Started workflow: %s\n", workflow_id);
fprintf(stderr, "Logs: ~/.argo/logs/%s.log\n", workflow_id);
```

**After**:
```c
#include "arc_output.h"

ARC_ERROR("Failed to initialize argo\n");
ARC_ERROR("Template not found: %s\n", template_name);
ARC_INFO("  Try: arc workflow list template\n");
ARC_SUCCESS("Started workflow: %s\n", workflow_id);
ARC_INFO("Logs: ~/.argo/logs/%s.log\n", workflow_id);
```

---

## Open Questions

1. **Should ARC_STATUS go to stdout or stderr?**
   - Option A: stderr (consistent with all arc output)
   - Option B: stdout (pipeable for scripts)
   - **Recommendation:** stderr for consistency, add ARC_OUTPUT for stdout

2. **Should we add color support?**
   - Can detect TTY and add ANSI colors
   - Add later as enhancement

3. **Should macros include newlines automatically?**
   - Pro: Shorter calls, less forgetting \n
   - Con: Less flexible for multi-part messages
   - **Recommendation:** Keep explicit \n for flexibility

---

## Non-Goals

❌ **Not changing test code** - Tests can use fprintf
❌ **Not changing log output** - Executor writes raw to log files
❌ **Not changing existing LOG_* macros** - Those stay for library code

---

## Implementation Estimate

- **Phase 1 (Arc CLI):** ~30 minutes
  - Create header: 5 min
  - Update 6 command files: 25 min
  - Test: verify output unchanged

- **Total:** < 1 hour for full arc standardization

---

## Decision Needed

**Casey, please confirm:**

1. ✅ Arc CLI should use `ARC_*` macros - **Yes/No?**
2. ✅ Executor logs stay as raw fprintf - **Correct?**
3. ✅ Library code uses `LOG_*` / `argo_report_error()` - **Correct?**
4. ❓ Should I implement Arc CLI macros now?

Once confirmed, I'll implement Phase 1 immediately.
