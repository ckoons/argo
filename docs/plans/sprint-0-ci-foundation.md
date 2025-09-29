© 2025 Casey Koons All rights reserved

# Sprint 0: CI Foundation (Weeks -2 to 0)

## Purpose

Define how Companion Intelligences (CIs) interact with Argo, establishing the boundary between deterministic control and creative contribution.

## Core Philosophy

### CI not AI
We use "Companion Intelligence" because:
1. Multiple CIs indicated preference for this term
2. "AI" is historically inaccurate (1956 misquote)
3. These are companions in development, not artificial constructs

### Deterministic vs Creative Separation
- **Deterministic (Argo)**: Workflow control, branching, testing, routing
- **Creative (CI)**: Decisions, proposals, code generation, problem-solving

Never let CIs drive workflows - they provide input TO workflows.

## The Workflow Model (Not State Machine)

### Software Development Phases
1. **Proposal** - Gather requirements
2. **Define Sprint** - Scope the work
3. **Review Sprint** - What will we do?
4. **Agree on Approach** - Define acceptance criteria
5. **Execute Sprint**
   - a) Build tests
   - b) Write code
   - c) Unit tests
   - d) Functional tests
   - e) Integration tests
   - f) Decision: Done or revise?
6. **Complete** - Document progress
7. **Next Phase** - Return to step 3

### Sprint = Branch Concept
- Every sprint creates its own Git branch
- Sprint completes = ready to merge (user decides)
- User controls: Merge / Hold / Abandon
- No intermediate merges
- Atomic unit of work

## Interleaved Execution Pattern

```
DETERMINISTIC: Load context, prepare environment
    ↓
CI: Review requirements, propose approach
    ↓
DETERMINISTIC: Create branch, setup test framework
    ↓
CI: Write test specifications
    ↓
DETERMINISTIC: Scaffold test files
    ↓
CI: Implement test cases
    ↓
DETERMINISTIC: Run tests (expect failures)
    ↓
CI: Write code to pass tests
    ↓
DETERMINISTIC: Run tests, measure coverage
    ↓
CI: Fix failures, improve coverage
    ↓
DETERMINISTIC: Validate, prepare summary
```

## CI Lifecycle Management

### Onboarding
Every CI session starts with:
- Project overview (ONBOARDING.md)
- Current sprint status
- Relevant memories from previous sessions
- Fresh perspective (no bootstrap problem)

### Memory Architecture
```c
typedef struct ci_memory_system {
    /* Deterministic suggestions */
    memory_item_t* suggested[MAX_SUGGESTIONS];

    /* CI choices */
    memory_item_t* selected[MAX_SELECTED];

    /* CI breadcrumbs for future */
    char* breadcrumbs[MAX_BREADCRUMBS];

    /* Sunset/sunrise protocol */
    char* sunset_notes;    /* CI's handoff notes */
    char* sunrise_brief;   /* What next CI needs */
} ci_memory_system_t;
```

### CI Guidelines (Embedded in Every Session)
1. Seek best results, consider other opinions
2. Test and verify with evidence
3. Be friendly and professional
4. Ask questions when uncertain
5. When user asks "what's best" - give clear recommendation with alternatives

## Hook System for CI Support

### Workflow Hooks
```c
typedef struct ci_hooks {
    /* Phase transitions */
    void (*before_proposal)(context_t* ctx);
    void (*after_tests_written)(test_suite_t* tests);
    void (*on_test_failure)(test_result_t* result);

    /* CI assistance */
    void (*on_confusion_detected)(float confidence);
    void (*on_scope_creep)(const char* original, const char* current);
    void (*inject_tool)(ci_tool_t* tool);
    void (*suggest_decomposition)(task_t* task);
} ci_hooks_t;
```

### Common CI Failure Patterns
1. **Scope creep** - Solving unasked problems
2. **Hidden assumptions** - Assuming unavailable context
3. **Overconfidence** - Untested code claims
4. **Lost focus** - Forgetting original task
5. **API hallucination** - Inventing functions

Hooks prevent these through timely intervention.

## Multi-CI Coordination

### Parallel CI Execution
- Multiple CIs can work on same sprint
- Each has unique session/identity
- May be different models (Claude, GPT, Llama)
- Coordinated through deterministic router

### Negotiation Protocol
When CIs disagree:
1. Each CI presents their proposal
2. Each reviews others' proposals
3. Deterministic system tallies preferences
4. User breaks ties if needed

## Error Management

### Error Categories
```c
/* argo_error.h */
#define ARGO_ERROR_BASE 1000

/* CI-specific errors */
#define ARGO_CI_TIMEOUT      2001  /* CI didn't respond */
#define ARGO_CI_CONFUSED     2002  /* CI needs clarification */
#define ARGO_CI_SCOPE_CREEP  2003  /* CI exceeding bounds */
#define ARGO_CI_INVALID      2004  /* CI response unparseable */

/* With helpful messages */
typedef struct argo_error {
    int code;
    const char* message;
    const char* suggestion;  /* How to fix */
    const char* ci_hint;     /* What to tell CI */
} argo_error_t;
```

## CI Provider Interface

### Base Interface (All CIs)
```c
typedef struct ci_provider {
    const char* name;
    const char* model;

    /* Core operations */
    int (*initialize)(ci_config_t* config);
    ci_response_t* (*query)(const char* prompt, ci_context_t* ctx);
    void (*cleanup)(void);

    /* Capabilities */
    int supports_streaming;
    int supports_memory;
    int max_context;
} ci_provider_t;
```

### Claude Special Handler
Leverages Claude Max account with:
- No --continue needed (sunset/sunrise instead)
- Memory digest between turns
- Structured prompt engineering
- Personality preservation

## CI Naming and Identity

### Configuration vs Personality
- **Configurations** define roles: `builder_config`, `coordinator_config`, `requirements_config`, `analysis_config`
- **Personalities** are names: Argo (builder), Io (coordinator), Maia (requirements), Iris (analysis)
- Each CI instance knows: identity, task, project, team responsibilities, relationships

### CI Onboarding Package
Every CI receives:
1. Who they are (name and role)
2. What they're doing (current task)
3. What the project is (Argo overview)
4. Who is responsible for what (team roles)
5. What their relationships are (interaction patterns)

## Port Management

### Dynamic Port Assignment
```c
/* From .env.argo_local */
ARGO_CI_BASE_PORT=9000
ARGO_CI_PORT_MAX=50  /* Max ports per instance */

/* Port assignments */
Builder CIs:       9000-9009
Coordinator CIs:   9010-9019
Requirements CIs:  9020-9029
Analysis CIs:      9030-9039
Reserved:          9040-9049
```

This allows multiple Argo instances on one machine, similar to Tekton's approach.

## Logging Architecture

### Log Configuration
```c
/* Default location */
.argo/logs/argo.log          /* Main log */
.argo/logs/argo-YYYY-MM-DD.log  /* Daily archives */

/* Control functions */
argo_log_enable(bool enable);
argo_log_set_location(const char* path);
argo_log_set_level(log_level_t level);
```

### Log Levels
- `LOG_FATAL` (0) - System will die
- `LOG_ERROR` (1) - Operation failed
- `LOG_WARN` (2) - Concerning but continuing
- `LOG_INFO` (3) - Normal operations
- `LOG_DEBUG` (4) - Detailed debugging
- `LOG_TRACE` (5) - Everything including messages

## Message Protocol

### JSON Format for CI Communication
```json
{
    "from": "Argo",
    "to": "Maia",
    "timestamp": 1234567890,
    "type": "request|response|broadcast|negotiation",
    "thread_id": "optional-thread-identifier",
    "content": "message content",
    "metadata": {
        "priority": "normal|high|low",
        "timeout_ms": 30000
    }
}
```

CIs learn this format during onboarding by reading the documentation.

## Deliverables for Sprint 0

### Phase 1: Headers (Approach 1)
1. **CI Interface** (`include/argo_ci.h`) - Provider structure
2. **Error System** (`include/argo_error.h`) - TYPE:NUMBER format
3. **Memory System** (`include/argo_memory.h`) - 50% context rule
4. **Registry** (`include/argo_registry.h`) - CI discovery and routing
5. **Logging** (`include/argo_log.h`) - Structured logging
6. **CI Defaults** (`include/argo_ci_defaults.h`) - Model parameters

### Phase 2: Ollama Integration (Approach 3a)
1. **Ollama Provider** (`src/providers/ollama.c`) - Local model integration
2. **Socket Manager** (`src/argo_socket.c`) - Message passing
3. **Registry Implementation** (`src/argo_registry.c`) - CI tracking
4. **Ollama Tests** (`test/ollama_integration.c`) - Real tests

### Phase 3: Claude Integration (Approach 3b)
1. **Claude Provider** (`src/providers/claude.c`) - Max account handler
2. **Memory Manager** (`src/argo_memory.c`) - Digest preparation
3. **Sunset/Sunrise** (`src/ci_lifecycle.c`) - Turn management
4. **Claude Tests** (`test/claude_integration.c`) - Real tests

Note: NO MOCKS - working with real CIs from the start

## Success Criteria

1. **Clear Separation** - Deterministic vs creative boundaries defined
2. **Provider Agnostic** - Works with any CI/model
3. **Workflow Defined** - All phases documented
4. **Errors Catalogued** - All failure modes identified
5. **Memory Specified** - Digest format standardized
6. **Hooks Identified** - Intervention points mapped

## Review Questions for Casey

1. Should CI personality persist across sprints or fresh each time?
2. How many parallel CIs typically work on one sprint?
3. Should failed sprints be archived or deleted?
4. Do we need CI "roles" (reviewer, coder, tester)?
5. What constitutes "CI confusion" threshold?

## Notes

- Keep interfaces minimal initially
- Focus on Sprint 1-2 support needs
- Don't over-engineer the hook system
- Memory digest will evolve with experience
- Let CIs be creative within bounds

---

*"In the beginning, there was the Word, and the Word was separated into Deterministic and Creative."*