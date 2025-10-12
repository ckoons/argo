# Unix Workflow Pivot - Development Sprint

**Status**: PARTIALLY IMPLEMENTED (Deletion Phase Complete, New Architecture NOT Implemented)
**Created**: 2025-01-15
**Updated**: 2025-01-15 (Post-deletion review)
**Version**: 1.0 (Clean Slate Architecture - PLAN ONLY)
**Estimated Effort**: 1-2 days (for remaining work)

**SEE**: [`PIVOT_STATUS.md`](PIVOT_STATUS.md) for actual implementation status vs. this plan.

---

## Philosophy: "Be Unix"

**Core insight**: Stop building a custom workflow engine. Use Unix as the workflow engine.

- **AIs are file-like objects** (sockets, pipes, stdin/stdout)
- **Workflows are shell scripts** (bash, not JSON)
- **Orchestrator is the OS** (process management, pipes, signals)

**Result**: 10x simpler, infinitely more powerful, zero learning curve.

---

## Architecture Comparison

### Before (Custom Workflow Engine)
```
arc CLI (HTTP client)
  ↓ HTTP REST API
argo-daemon (workflow orchestrator)
  ↓ fork/exec
workflow_executor (JSON interpreter, ~3000 lines)
  ↓ function calls
Step handlers (ci_analyze, conditional, loop, etc.)
  ↓ socket/HTTP
AI providers (9 providers)
```

**Complexity**: 6 layers, ~15,953 lines
**Workflow format**: JSON DSL with custom syntax
**Programming**: Limited (variables, conditionals via JSON)
**Learning curve**: High (custom syntax, concepts)

### After (Unix Approach)
```
arc CLI (process manager)
  ↓ fork/exec
workflow.sh (bash script)
  ↓ pipes/sockets
argo-ai-daemon (persistent AI with socket interface)
  ↓ API/CLI
AI providers (reuse existing)
```

**Complexity**: 3 layers, ~2,000 lines (estimated)
**Workflow format**: Bash scripts
**Programming**: Full bash programming language
**Learning curve**: Zero (everyone knows bash)

---

## What This Looks Like

### Simple Workflow (Code Review)
```bash
#!/bin/bash
# code_review.sh - Review code before pushing

set -euo pipefail

file="$1"

echo "Reviewing $file..."

# AI is just a command that reads stdin, writes stdout
review=$(argo-ai claude_code "Review $file for security issues")

# Log review
echo "$review" > "reviews/$(basename $file).txt"

# Check approval (standard grep, standard conditionals)
if echo "$review" | grep -q "APPROVED"; then
    echo "✓ Review approved"
    git add "$file"
    git commit -m "Reviewed: $file"
    git push
    exit 0
else
    echo "✗ Changes needed"
    echo "$review"
    exit 1
fi
```

**Run it**:
```bash
arc start code_review.sh src/main.c
```

### Multi-AI Consensus
```bash
#!/bin/bash
# consensus.sh - Get multiple AI opinions in parallel

set -euo pipefail

prompt="$1"

# Query multiple AIs in parallel (standard background jobs)
argo-ai claude_code "$prompt" > /tmp/review1.txt &
argo-ai openai_api "$prompt" > /tmp/review2.txt &
argo-ai gemini_api "$prompt" > /tmp/review3.txt &

# Wait for all (standard wait)
wait

# Aggregate results (standard tools: grep, wc)
approved=$(grep -l "APPROVED" /tmp/review*.txt | wc -l)

if [[ $approved -ge 2 ]]; then
    echo "Consensus: APPROVED (${approved}/3)"
    git push
else
    echo "No consensus (${approved}/3 approved)"
    cat /tmp/review*.txt
    exit 1
fi
```

### Interactive Chat Loop
```bash
#!/bin/bash
# chat.sh - Interactive conversation with AI

set -euo pipefail

provider="${1:-claude_code}"

# Optional: Start conversation context
context=$(argo-ai-context start "$provider")

echo "Chat with $provider (type 'exit' to quit)"
echo "Context: $context"
echo ""

while true; do
    read -p "You: " user_input

    if [[ "$user_input" == "exit" ]]; then
        argo-ai-context close "$context"
        break
    fi

    # Send with context to maintain conversation
    response=$(argo-ai --context="$context" "$provider" "$user_input")
    echo "AI: $response"
    echo ""
done
```

### Dynamic Workflow (AI Controls Flow)
```bash
#!/bin/bash
# dynamic_review.sh - AI decides what to do next

set -euo pipefail

files=$(git diff --name-only HEAD~1)

for file in $files; do
    echo "Analyzing $file..."

    # AI decides severity and action
    analysis=$(argo-ai claude_code "
        Analyze $file changes.
        Respond with ONE LINE in format: SEVERITY|ACTION
        SEVERITY: LOW, MEDIUM, HIGH, CRITICAL
        ACTION: APPROVE, REVIEW, BLOCK
    ")

    # Parse response (standard cut/awk/grep)
    severity=$(echo "$analysis" | cut -d'|' -f1)
    action=$(echo "$analysis" | cut -d'|' -f2)

    echo "  Severity: $severity"
    echo "  Action: $action"

    # Standard case statement
    case "$action" in
        APPROVE)
            echo "  ✓ Auto-approved"
            ;;
        REVIEW)
            echo "  ⚠ Manual review needed"
            # Could prompt user, send notification, etc.
            ;;
        BLOCK)
            echo "  ✗ Blocked"
            exit 1
            ;;
        *)
            echo "  ? Unknown action: $action"
            exit 2
            ;;
    esac
done

git push
```

---

## Component Design

### 1. AI Daemon (`argo-ai-daemon`)

**Purpose**: Persistent AI process with socket interface.

**Socket Interface**:
```
/tmp/argo-claude-code.sock
/tmp/argo-openai-api.sock
/tmp/argo-gemini-api.sock
... (one socket per provider)
```

**Protocol** (simple line-based):
```
Client → Server: QUERY <prompt>\n
Server → Client: RESPONSE <text>\n

Client → Server: CONTEXT_START\n
Server → Client: CONTEXT <context_id>\n

Client → Server: QUERY_CTX <context_id> <prompt>\n
Server → Client: RESPONSE <text>\n

Client → Server: CONTEXT_END <context_id>\n
Server → Client: OK\n
```

**Implementation**:
```c
/* src/ai_daemon/argo_ai_daemon.c */

int main(int argc, char* argv[]) {
    const char* provider = argv[1];  /* claude_code, openai_api, etc. */

    /* Create socket */
    char socket_path[256];
    snprintf(socket_path, sizeof(socket_path), "/tmp/argo-%s.sock", provider);

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    bind(sock, socket_path, ...);
    listen(sock, SOCKET_BACKLOG);

    /* Accept connections */
    while (1) {
        int client = accept(sock, NULL, NULL);

        /* Read command */
        char cmd[ARGO_BUFFER_LARGE];
        read(client, cmd, sizeof(cmd));

        /* Dispatch: QUERY, CONTEXT_START, QUERY_CTX, CONTEXT_END */
        if (strncmp(cmd, "QUERY ", 6) == 0) {
            handle_query(client, cmd + 6);
        } else if (strncmp(cmd, "CONTEXT_START", 13) == 0) {
            handle_context_start(client);
        } /* ... */

        close(client);
    }
}
```

**Benefits**:
- Persistent process (maintains context/memory)
- Fast (no startup overhead)
- Concurrent (handles multiple clients)
- Standard Unix socket (works with any language)

---

### 2. AI CLI (`argo-ai`)

**Purpose**: Command-line interface to AI daemon (or direct provider).

**Usage**:
```bash
# Simple query (connects to daemon)
argo-ai claude_code "What is 2+2?"

# With context
context=$(argo-ai-context start claude_code)
argo-ai --context="$context" claude_code "First question"
argo-ai --context="$context" claude_code "Follow-up question"
argo-ai-context close "$context"

# Streaming (optional)
argo-ai --stream claude_code "Long prompt..." | while read -r line; do
    echo "Chunk: $line"
done

# Direct mode (no daemon, useful for one-off queries)
argo-ai --direct ollama "Quick question"
```

**Implementation** (simple wrapper):
```bash
#!/bin/bash
# bin/argo-ai

provider="$1"
prompt="$2"
context=""
stream=false
direct=false

# Parse options
while [[ $# -gt 0 ]]; do
    case "$1" in
        --context=*)
            context="${1#--context=}"
            shift
            ;;
        --stream)
            stream=true
            shift
            ;;
        --direct)
            direct=true
            shift
            ;;
        *)
            break
            ;;
    esac
done

provider="$1"
shift
prompt="$*"

# Socket path
socket="/tmp/argo-${provider}.sock"

if [[ "$direct" == "true" ]]; then
    # Direct mode: Call provider directly (no daemon)
    exec /usr/local/bin/argo-ai-direct "$provider" "$prompt"
fi

# Daemon mode: Connect to socket
if [[ -z "$context" ]]; then
    # Simple query
    echo "QUERY $prompt" | nc -U "$socket"
else
    # Query with context
    echo "QUERY_CTX $context $prompt" | nc -U "$socket"
fi
```

**Or as C binary** (more efficient):
```c
/* bin/argo-ai.c */

int main(int argc, char* argv[]) {
    const char* provider = argv[1];
    const char* prompt = argv[2];

    /* Connect to daemon socket */
    char socket_path[256];
    snprintf(socket_path, sizeof(socket_path), "/tmp/argo-%s.sock", provider);

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    connect(sock, socket_path, ...);

    /* Send query */
    char cmd[ARGO_BUFFER_LARGE];
    snprintf(cmd, sizeof(cmd), "QUERY %s\n", prompt);
    write(sock, cmd, strlen(cmd));

    /* Read response */
    char response[ARGO_BUFFER_LARGE];
    ssize_t n = read(sock, response, sizeof(response));
    response[n] = '\0';

    /* Print to stdout */
    printf("%s", response);

    close(sock);
    return 0;
}
```

---

### 3. Arc CLI (Simplified Process Manager)

**Purpose**: Start/stop/monitor workflow scripts.

**Commands**:
```bash
arc start workflow.sh [args...]      # Fork workflow, track PID
arc list                              # Show running workflows
arc logs <workflow_id>                # tail -f workflow log
arc stop <workflow_id>                # kill workflow PID
arc status <workflow_id>              # Check if workflow running
```

**No longer needed**:
- `arc workflow start` (just `arc start`)
- HTTP server (no daemon API)
- Workflow state tracking (OS tracks processes)
- Progress reporting (just read logs)

**Implementation** (simplified):
```c
/* arc/src/cmd_start.c */

int cmd_start(int argc, char* argv[]) {
    const char* script_path = argv[0];

    /* Generate workflow ID */
    char workflow_id[64];
    generate_workflow_id(workflow_id, sizeof(workflow_id));

    /* Fork process */
    pid_t pid = fork();
    if (pid == 0) {
        /* Child: Execute workflow script */

        /* Redirect stdout/stderr to log */
        char log_path[512];
        snprintf(log_path, sizeof(log_path),
                "~/.argo/logs/%s/%s.log",
                get_date_string(), workflow_id);

        int log_fd = open(log_path, O_CREAT | O_WRONLY | O_APPEND, 0644);
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);

        /* Execute script */
        execv(script_path, &argv[1]);
        exit(1);
    }

    /* Parent: Track workflow in registry */
    workflow_registry_add(workflow_id, pid, script_path);

    printf("Started workflow: %s (PID: %d)\n", workflow_id, pid);
    return 0;
}
```

---

### 4. Context Tracking (Optional)

**Purpose**: Maintain conversation context across multiple AI queries.

**Commands**:
```bash
# Start context (returns context ID)
context=$(argo-ai-context start claude_code)

# Use context
argo-ai --context="$context" claude_code "First question"
argo-ai --context="$context" claude_code "Follow-up"

# Close context (free resources)
argo-ai-context close "$context"
```

**Implementation** (daemon tracks contexts):
```c
/* Context storage in daemon */
typedef struct {
    char context_id[64];
    char* history;           /* Accumulated conversation */
    size_t history_len;
    time_t last_accessed;
} ai_context_t;

/* Context management */
const char* context_start(void);                        /* Returns context_id */
int context_add_message(const char* context_id, const char* role, const char* content);
const char* context_get_history(const char* context_id);
int context_close(const char* context_id);
```

---

## What We Keep

### ✅ **Core Infrastructure** (Minimal changes)

#### 1. AI Providers (~4,000 lines)
- `src/providers/argo_claude_api.c`
- `src/providers/argo_openai_api.c`
- `src/providers/argo_gemini_api.c`
- `src/providers/argo_ollama.c`
- `src/providers/argo_mock.c` (for testing)
- etc.

**Changes**: Minor - wrap in daemon interface

#### 2. Registry (~500 lines)
- `src/registry/argo_registry.c`
- Track workflow PIDs instead of workflow state
- Service discovery for AI daemon sockets

**Changes**: Simplify - no workflow state machine

#### 3. Foundation (~2,000 lines)
- `src/argo_error.c`
- `src/argo_log.c`
- `src/argo_http.c`
- `src/argo_json.c`
- etc.

**Changes**: None - keep as-is

#### 4. Config System (New, ~300 lines)
- Shell-based config files
- `~/.argo/config/config.sh` (user)
- `argo/config/config.sh` (project)
- `argo/.argo/config/config.sh` (local)

**Format**:
```bash
# ~/.argo/config/config.sh

export ARGO_DEFAULT_PROVIDER="claude_code"
export ARGO_DEFAULT_MODEL="claude-sonnet-4-5"
export ARGO_LOG_RETENTION_DAYS=7
```

**Usage in workflows**:
```bash
#!/bin/bash
source ~/.argo/config/config.sh
source argo/config/config.sh
source argo/.argo/config/config.sh

argo-ai "$ARGO_DEFAULT_PROVIDER" "prompt"
```

---

## What We Delete

### ❌ **Workflow Engine** (~3,000 lines)
```
src/workflow/argo_workflow.c
src/workflow/argo_workflow_executor.c
src/workflow/argo_workflow_executor_main.c
src/workflow/argo_workflow_checkpoint.c
src/workflow/argo_workflow_loader.c
src/workflow/argo_workflow_context.c
src/workflow/argo_workflow_json.c
src/workflow/argo_workflow_conditions.c
src/workflow/argo_workflow_steps_*.c (5 files)
src/workflow/argo_workflow_persona.c
src/workflow/argo_workflow_input.c
src/workflow/argo_provider.c
src/workflow/argo_provider_messaging.c
src/workflow/argo_memory.c
src/workflow/argo_io_channel.c
src/workflow/argo_io_channel_http.c
```

**Reason**: Shell scripts replace all of this.

### ❌ **Daemon Orchestration** (~1,500 lines)
```
src/daemon/argo_daemon_main.c
src/daemon/argo_daemon_api.c
src/daemon/argo_http_server.c
src/daemon/argo_orchestrator.c
src/daemon/argo_shared_services.c (partial)
```

**Reason**: OS handles process management.

### ❌ **Arc HTTP Client** (~500 lines)
```
arc/src/arc_http_client.c
arc/src/workflow_start.c (HTTP-based)
arc/src/workflow_status.c (HTTP-based)
arc/src/workflow_abandon.c (HTTP-based)
arc/src/workflow_pause.c
arc/src/workflow_resume.c
arc/src/workflow_attach.c
```

**Reason**: Direct process management, no HTTP API.

### ❌ **JSON Workflow Templates** (~10 files)
```
workflows/*.json
```

**Reason**: Shell scripts replace JSON.

**Total deleted**: ~5,000+ lines

---

## What We Build

### ✅ **New Components** (~500 lines total)

#### 1. AI Daemon (`argo-ai-daemon`) (~300 lines)
```
src/ai_daemon/argo_ai_daemon.c           /* Socket server */
src/ai_daemon/argo_ai_context.c          /* Context tracking */
include/argo_ai_daemon.h
```

#### 2. AI CLI (`argo-ai`) (~100 lines)
```
bin/argo-ai.c                            /* CLI wrapper */
```
Or shell script (~50 lines).

#### 3. Arc Process Manager (~100 lines)
```
arc/src/cmd_start.c                      /* Fork workflow script */
arc/src/cmd_list.c                       /* List PIDs */
arc/src/cmd_logs.c                       /* tail -f logs */
arc/src/cmd_stop.c                       /* kill PID */
```

**Total new code**: ~500 lines

---

## Net Impact

### Before
- **Total lines**: ~15,953
- **Core systems**: 6 (arc, daemon, orchestrator, executor, steps, providers)
- **Workflow format**: JSON DSL
- **Complexity**: High

### After
- **Total lines**: ~2,000-3,000 (estimated)
- **Core systems**: 3 (arc, ai-daemon, providers)
- **Workflow format**: Bash scripts
- **Complexity**: Low

**Reduction**: ~13,000 lines deleted (~80% reduction)

---

## Implementation Plan

### Phase 1: AI Daemon (Day 1, 4-6 hours)

#### Task 1.1: Socket Server (2 hours)
- [ ] Create `argo_ai_daemon.c`
- [ ] Unix socket server (accept connections)
- [ ] Command parsing (QUERY, CONTEXT_START, QUERY_CTX, CONTEXT_END)
- [ ] Integrate existing providers (wrap provider API)
- [ ] Test with `nc` (netcat)

#### Task 1.2: Context Tracking (2 hours)
- [ ] Create `argo_ai_context.c`
- [ ] Context storage (hash table: context_id → history)
- [ ] Context lifecycle (start, add_message, get_history, close)
- [ ] Timeout-based cleanup (stale contexts)
- [ ] Test multi-turn conversations

#### Task 1.3: CLI Wrapper (1 hour)
- [ ] Create `argo-ai` script or binary
- [ ] Connect to daemon socket
- [ ] Send query, print response
- [ ] Support --context flag
- [ ] Test basic queries

### Phase 2: Arc Simplification (Day 1, 2-3 hours)

#### Task 2.1: Process Manager (2 hours)
- [ ] Simplify `arc start` (fork script, track PID)
- [ ] Implement `arc list` (read registry, show PIDs)
- [ ] Implement `arc logs` (tail -f log file)
- [ ] Implement `arc stop` (kill PID)
- [ ] Remove HTTP server code

#### Task 2.2: Registry Simplification (1 hour)
- [ ] Remove workflow state tracking
- [ ] Keep PID tracking only
- [ ] Add socket path tracking (for AI daemons)
- [ ] Test registry persistence

### Phase 3: Example Workflows (Day 2, 3-4 hours)

#### Task 3.1: Basic Workflows (2 hours)
- [ ] Create `workflows/code_review.sh`
- [ ] Create `workflows/multi_ai_consensus.sh`
- [ ] Create `workflows/interactive_chat.sh`
- [ ] Test with `arc start`

#### Task 3.2: Advanced Workflows (1 hour)
- [ ] Create `workflows/dynamic_workflow.sh` (AI controls flow)
- [ ] Create `workflows/parallel_tasks.sh` (background jobs)
- [ ] Create `workflows/context_conversation.sh` (multi-turn)

#### Task 3.3: Testing Workflows (1 hour)
- [ ] Create `workflows/test_mock_provider.sh`
- [ ] Test with ARGO_TEST_MODE=1
- [ ] Validate all providers work via socket

### Phase 4: Documentation & Cleanup (Day 2, 2-3 hours)

#### Task 4.1: Documentation (2 hours)
- [ ] Update README (new architecture)
- [ ] Write workflow guide (bash syntax, argo-ai usage)
- [ ] Document AI daemon protocol
- [ ] Add examples for common patterns

#### Task 4.2: Cleanup (1 hour)
- [ ] Delete old workflow engine code
- [ ] Update Makefile (remove workflow lib)
- [ ] Update .gitignore
- [ ] Run tests (ensure nothing breaks)

---

## File Structure

### New Files
```
src/ai_daemon/
├── argo_ai_daemon.c                 /* Socket server */
└── argo_ai_context.c                /* Context tracking */

include/
├── argo_ai_daemon.h
└── argo_ai_context.h

bin/
├── argo-ai-daemon                   /* AI daemon binary */
└── argo-ai                          /* CLI wrapper */

workflows/
├── code_review.sh                   /* Example workflows */
├── multi_ai_consensus.sh
├── interactive_chat.sh
├── dynamic_workflow.sh
├── parallel_tasks.sh
└── context_conversation.sh

~/.argo/config/
└── config.sh                        /* Shell-based config */

argo/config/
└── config.sh                        /* Project config */

argo/.argo/config/
└── config.sh                        /* Local config */
```

### Deleted Files
```
src/workflow/                        /* Entire directory (~3000 lines) */
src/daemon/argo_daemon_main.c
src/daemon/argo_daemon_api.c
src/daemon/argo_http_server.c
src/daemon/argo_orchestrator.c
arc/src/arc_http_client.c
arc/src/workflow_*.c                 /* HTTP-based commands */
workflows/*.json                     /* All JSON workflows */
```

### Modified Files
```
arc/src/cmd_start.c                  /* Simplified: fork script */
arc/src/cmd_list.c                   /* Simplified: show PIDs */
arc/src/cmd_logs.c                   /* New: tail logs */
arc/src/cmd_stop.c                   /* Simplified: kill PID */
src/registry/argo_registry.c         /* Simplified: PID tracking only */
Makefile                             /* Remove workflow lib */
README.md                            /* Update architecture docs */
```

---

## Testing Strategy

### Unit Tests
- AI daemon socket protocol
- Context tracking (create, query, close)
- Provider integration (wrap existing providers)
- Registry PID tracking

### Integration Tests
- Start AI daemon, query from shell
- Multi-turn conversation with context
- Parallel AI queries (multiple clients)
- Workflow execution (arc start script.sh)

### Manual Tests
- Run example workflows
- Test with all providers (claude, openai, ollama, mock)
- Test context across multiple queries
- Test workflow logging

---

## Config System Design

### Shell-Based Config (Simple, Powerful)

**User config** (`~/.argo/config/config.sh`):
```bash
# User-level defaults
export ARGO_DEFAULT_PROVIDER="claude_code"
export ARGO_DEFAULT_MODEL="claude-sonnet-4-5"
export ARGO_LOG_RETENTION_DAYS=7

# API keys (never commit)
export ANTHROPIC_API_KEY="sk-ant-..."
export OPENAI_API_KEY="sk-proj-..."
```

**Project config** (`argo/config/config.sh`):
```bash
# Project team conventions (git-tracked)
export ARGO_DEFAULT_PROVIDER="ollama"
export ARGO_DEFAULT_MODEL="codellama"
export ARGO_LOG_RETENTION_DAYS=14
```

**Local config** (`argo/.argo/config/config.sh`):
```bash
# Machine-specific overrides (git-ignored)
export ARGO_DEFAULT_PROVIDER="claude_api"
```

**Usage in workflows**:
```bash
#!/bin/bash

# Source config hierarchy (later sources override earlier)
source ~/.argo/config/config.sh 2>/dev/null || true
source argo/config/config.sh 2>/dev/null || true
source argo/.argo/config/config.sh 2>/dev/null || true

# Use config variables
provider="${ARGO_DEFAULT_PROVIDER}"
model="${ARGO_DEFAULT_MODEL}"

argo-ai "$provider" "prompt here"
```

**Arc can source config too**:
```bash
# In arc startup
source ~/.argo/config/config.sh
source argo/config/config.sh
source argo/.argo/config/config.sh

# Now $ARGO_DEFAULT_PROVIDER available in arc
```

---

## Recording & Replay

### Built-in via Standard Unix Tools

**Recording** (automatic):
```bash
# Arc automatically redirects workflow stdout/stderr to log
arc start workflow.sh > ~/.argo/logs/2025-01-15/wf-123.log 2>&1
```

**Manual recording** (using `script`):
```bash
# Record entire terminal session
script -q recordings/session.log workflow.sh
```

**Replay**:
```bash
# Replay is just re-running the script
bash workflow.sh
```

**Extract** (no need, it's already a script):
```bash
# Workflow IS the script
cp workflow.sh my_automation.sh
```

---

## Migration Path

### Answer: Start Fresh

**Decision** (Casey): Start over, it's easiest to start clean.

**Reason**:
- Old JSON workflows are fundamentally different
- Converting JSON → bash is more work than writing fresh
- Fresh start = clean, simple, optimized for new architecture
- Old workflows deleted (clean slate)

**Approach**:
1. Keep providers (reuse existing AI integrations)
2. Build new AI daemon wrapper
3. Simplify arc CLI
4. Write new workflows in bash as needed

---

## Success Criteria

- [ ] AI daemon starts, listens on socket
- [ ] `argo-ai` CLI queries daemon successfully
- [ ] Context tracking works (multi-turn conversations)
- [ ] `arc start workflow.sh` forks script, tracks PID
- [ ] `arc list` shows running workflows
- [ ] `arc logs wf-123` tails workflow log
- [ ] `arc stop wf-123` kills workflow
- [ ] Example workflows run successfully
- [ ] All providers work via daemon (claude, openai, ollama, mock)
- [ ] Config hierarchy loads correctly
- [ ] No memory leaks
- [ ] Compiles with `-Werror`

---

## Open Questions (Answered)

### 1. **AI daemon or CLI?**
**Answer**: Both - daemon for performance, CLI for simplicity.

### 2. **Context tracking?**
**Answer**: Daemon with optional context tracking.

### 3. **JSON workflows obsolete?**
**Answer**: Pure shell scripts. Use comments for metadata.

### 4. **Migration path?**
**Answer**: Start over. Delete old workflows, build fresh bash scripts as needed.

### 5. **GUI workflow builder?**
**Answer**: Future enhancement (can generate bash scripts).

---

## Benefits Summary

### ✅ **Simplicity**
- 80% code reduction (~13,000 lines deleted)
- 3 layers instead of 6
- Shell scripts instead of JSON DSL
- Standard Unix tools (no custom abstractions)

### ✅ **Power**
- Full bash programming (conditionals, loops, functions, pipes)
- Standard tools (grep, awk, sed, jq, curl)
- Parallel execution (background jobs, wait)
- Error handling (set -e, trap, ||, &&)

### ✅ **Familiarity**
- Zero learning curve (everyone knows bash)
- Standard debugging (bash -x, set -x, echo)
- Standard testing (bats, shellcheck)
- IDE support (syntax highlighting, linting)

### ✅ **Unix Philosophy**
- Text is universal interface
- Pipes compose tools
- Processes are units of execution
- OS handles orchestration

### ✅ **Maintainability**
- Less code = fewer bugs
- Standard patterns = easier understanding
- Standard tools = community support
- Simplicity = long-term sustainability

---

## Timeline

**Day 1** (6-9 hours):
- AI daemon with socket interface (4-6 hours)
- Arc process manager simplification (2-3 hours)

**Day 2** (5-7 hours):
- Example workflows in bash (3-4 hours)
- Documentation and cleanup (2-3 hours)

**Total**: 1-2 days (11-16 hours)

---

## Next Steps

1. Document this plan ✅ (completed)
2. Discuss approach with Casey
3. Answer remaining questions
4. Begin implementation (AI daemon first)

---

**End of Sprint Plan**
