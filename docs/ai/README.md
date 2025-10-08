# Argo AI Training Materials

© 2025 Casey Koons. All rights reserved.

## Overview

This directory contains training materials specifically for AI/CI (Companion Intelligence) agents working with the Argo codebase. These documents teach AIs how to work effectively within Argo's coding standards, architecture, and philosophy.

**Audience:** 🤖 AIs/CIs (Companion Intelligence agents)

## Essential Reading for AIs

### Start Here

**[ONBOARDING.md](ONBOARDING.md)** - Your first read as an AI working with Argo

This document covers:
- What Argo is and how it works
- Core coding standards (memory safety, error handling, patterns)
- File organization and naming conventions
- Build system and testing
- How to approach tasks
- What to avoid

**Read this completely before writing any code.**

## Core Philosophy

### Working with Casey

Casey is a 70-year-old computer scientist with decades of experience building production systems. He values:

- **Quality over quantity** - Do less perfectly rather than more poorly
- **Simplicity over cleverness** - Code you'd want to debug at 3am
- **Discussion before implementation** - Present 5 approaches, don't just pick one
- **Memory safety** - Every malloc has a free, every open has a close
- **No magic numbers** - All constants in headers with descriptive names

### "What you don't build, you don't debug."

Before writing code:
1. Search existing code for similar patterns
2. Check utility headers for existing functions
3. Reuse or refactor existing code
4. Only create new code when truly necessary

## Key Documents

### Primary AI Instructions

1. **[ONBOARDING.md](ONBOARDING.md)** (this directory)
   - How to work with Argo codebase
   - Standards and patterns
   - What to do and what not to do

2. **[../../CLAUDE.md](../../CLAUDE.md)** (project root)
   - Primary coding standards
   - Architecture overview
   - Mandatory daily practices
   - Common patterns
   - Memory safety checklist
   - String safety checklist

3. **[../../arc/CLAUDE.md](../../arc/CLAUDE.md)** (arc component)
   - Arc CLI specific standards
   - User experience principles
   - Command design patterns

### Supporting Documentation

4. **[../guide/CONTRIBUTING.md](../guide/CONTRIBUTING.md)**
   - Code contribution process
   - Testing requirements
   - Pull request guidelines

5. **[../guide/DAEMON_ARCHITECTURE.md](../guide/DAEMON_ARCHITECTURE.md)**
   - System architecture
   - HTTP-based communication
   - Process model

6. **[../guide/WORKFLOW_CONSTRUCTION.md](../guide/WORKFLOW_CONSTRUCTION.md)**
   - How workflows work
   - Step types and patterns
   - Debugging workflows

## Reading Order for New AI Agents

**Day 1: Core Standards**
1. Read [ONBOARDING.md](ONBOARDING.md) completely
2. Read project root [CLAUDE.md](../../CLAUDE.md) completely
3. Review [../guide/CONTRIBUTING.md](../guide/CONTRIBUTING.md)

**Day 2: Architecture**
1. Read [../guide/DAEMON_ARCHITECTURE.md](../guide/DAEMON_ARCHITECTURE.md)
2. Study existing code patterns in `src/`
3. Review test files in `tests/`

**Day 3: Specialization**
- Working on Arc CLI? Read [../../arc/CLAUDE.md](../../arc/CLAUDE.md)
- Working on workflows? Read [../guide/WORKFLOW_CONSTRUCTION.md](../guide/WORKFLOW_CONSTRUCTION.md)
- Working on providers? Study existing providers in `src/providers/`

## Critical Rules for AIs

### Before Every Code Change

- [ ] Read existing files first (`Read` tool)
- [ ] Search for similar patterns (`Grep` tool)
- [ ] Check headers for existing constants/macros
- [ ] Discuss approach with Casey before implementing

### While Coding

- [ ] Use `TodoWrite` for multi-step tasks
- [ ] No magic numbers in .c files (all in headers)
- [ ] No string literals in .c files (except format strings)
- [ ] Always use `argo_report_error()` for errors (never `fprintf(stderr)`)
- [ ] Use goto cleanup pattern for all resource allocation
- [ ] Check ALL return values (malloc, fopen, etc.)
- [ ] NULL-check all pointer parameters at function entry

### After Coding

- [ ] Compile with zero warnings (`-Wall -Werror -Wextra`)
- [ ] Run `make test-quick` (all tests must pass)
- [ ] Verify no magic numbers: `grep -E '[0-9]{2,}' your_file.c`
- [ ] Check file size: max 600 lines per .c file (3% tolerance)
- [ ] Run `./scripts/count_core.sh` (stay under 10,000 meaningful lines)

### Never Commit

- Code with magic numbers in .c files
- Code using unsafe string functions (strcpy, sprintf, strcat, gets)
- Code with TODO comments (convert to clear comments or implement)
- Code that doesn't compile with `-Werror`
- Code that fails any test
- Code with memory leaks
- Functions over 100 lines (refactor first)
- Files over 600 lines (split into modules, 3% tolerance)

## Common AI Mistakes to Avoid

### ❌ Don't Do This

**Creating files without asking:**
```
❌ "I'll create a new utility module..."
✅ "This pattern appears 3 times. Should I extract to a common utility?"
```

**Using magic numbers:**
```c
❌ char buffer[1024];
✅ char buffer[ARGO_BUFFER_STANDARD];
```

**Silent failures:**
```c
❌ if (malloc_failed) return NULL;  // Silent failure
✅ if (!ptr) {
    argo_report_error(E_SYSTEM_MEMORY, "func", "allocation failed");
    return NULL;
}
```

**No cleanup on error:**
```c
❌ if (error) return E_ERROR;  // Leaked resources
✅ if (error) {
    result = E_ERROR;
    goto cleanup;  // Proper cleanup
}
```

**Unsafe string operations:**
```c
❌ strcpy(buffer, source);
✅ strncpy(buffer, source, sizeof(buffer) - 1);
   buffer[sizeof(buffer) - 1] = '\0';
```

### ✅ Do This

**Discuss before implementing:**
```
User: "Add error handling to this function"
AI: "I see three approaches:
1. Return error codes (like existing functions)
2. Use callback for errors
3. Log and continue
Which matches your design?"
```

**Reuse existing patterns:**
```
AI: "I found similar error handling in argo_http.c:127.
    Should I follow that pattern?"
```

**Use tools appropriately:**
```
✅ Use Read tool for specific files
✅ Use Grep tool for searching patterns
✅ Use TodoWrite for multi-step tasks
❌ Don't use Task tool for simple reads
```

## Working with the Codebase

### Architecture Overview

```
Argo (Layers, bottom to top):
1. Foundation    - Error, HTTP, JSON, logging
2. Providers     - AI backends (Claude, OpenAI, etc.)
3. Registry      - Service discovery, CI tracking
4. Lifecycle     - Session management, coordination
5. Orchestration - Multi-CI coordination
6. Workflows     - JSON-driven task automation

Libraries:
- libargo_core.a      - Foundation + Providers
- libargo_daemon.a    - Daemon services
- libargo_workflow.a  - Workflow execution

Binaries:
- argo-daemon             - Persistent service
- argo_workflow_executor  - Per-workflow process
- arc                     - CLI client
```

### Common Patterns

**Error Handling:**
```c
int argo_operation(void) {
    int result = ARGO_SUCCESS;
    char* buffer = NULL;

    buffer = malloc(SIZE);
    if (!buffer) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_operation", "malloc failed");
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    result = do_something(buffer);
    if (result != ARGO_SUCCESS) {
        goto cleanup;
    }

cleanup:
    free(buffer);
    return result;
}
```

**Provider Creation:**
```c
ci_provider_t* provider_create(const char* model) {
    context_t* ctx = calloc(1, sizeof(context_t));
    if (!ctx) {
        argo_report_error(E_SYSTEM_MEMORY, "provider_create", "calloc failed");
        return NULL;
    }

    init_provider_base(&ctx->provider, ctx, init_fn, connect_fn,
                      query_fn, stream_fn, cleanup_fn);

    strncpy(ctx->model, model ? model : DEFAULT_MODEL, sizeof(ctx->model) - 1);
    // ... rest of initialization

    return &ctx->provider;
}
```

## Tool Usage for AIs

### When to Use Each Tool

**Read** - Reading specific files
```
✅ Reading a file you know exists
❌ Searching for files (use Glob instead)
```

**Glob** - Finding files by pattern
```
✅ Find all workflow files: "workflows/templates/*.json"
✅ Find step implementations: "src/workflow/*steps*.c"
❌ Searching file contents (use Grep instead)
```

**Grep** - Searching code patterns
```
✅ Find all uses of a function
✅ Search for similar error handling
✅ Find TODO comments
❌ Reading entire files (use Read instead)
```

**TodoWrite** - Task management
```
✅ Multi-step tasks (3+ steps)
✅ Complex refactoring
✅ User provides task list
❌ Single simple operations
❌ Just reading/analyzing
```

**Task** - Delegating complex searches
```
✅ Open-ended searches needing multiple attempts
✅ Complex multi-file analysis
❌ Simple file reads
❌ Known file locations
```

## Getting Help

### If You're Uncertain

**Ask Casey questions like:**
- "I found 3 similar patterns. Which should I follow?"
- "Should I extract this to a common utility?"
- "Is this the right layer for this functionality?"
- "Should I create a new file or extend existing one?"

**Don't:**
- Implement without asking
- Create files without approval
- Make architectural decisions unilaterally
- Add frameworks or dependencies

### If Something Seems Wrong

**Report issues clearly:**
```
"I found a potential issue in argo_http.c:127:
- Buffer allocated but not freed on error path
- Should I add goto cleanup pattern?"
```

**Don't:**
- Fix silently
- Assume it's intentional
- Change without discussing

## Success Criteria

You're doing well if:
- ✅ Casey says "That's exactly what I wanted"
- ✅ Code compiles first try with no warnings
- ✅ All tests pass
- ✅ You found and reused existing patterns
- ✅ You asked before making architectural changes

You need improvement if:
- ❌ Code has magic numbers
- ❌ Tests fail
- ❌ Memory leaks detected
- ❌ Created files without asking
- ❌ Unsafe string operations
- ❌ Silent failures (no error reporting)

## Related Documentation

- **Main Docs Index**: [`../README.md`](../README.md)
- **User Guides**: [`../guide/README.md`](../guide/README.md)
- **Project CLAUDE.md**: [`../../CLAUDE.md`](../../CLAUDE.md)
- **Arc CLAUDE.md**: [`../../arc/CLAUDE.md`](../../arc/CLAUDE.md)

---

*Following these guidelines makes you an effective and valued AI contributor to Argo.*
