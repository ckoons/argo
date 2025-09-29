© 2025 Casey Koons All rights reserved

# Argo Technical Architecture

## Overview

Argo is a minimal C implementation of Tekton's core inter-CI communication and parallel development capabilities, designed for embedding in AI IDEs. We use "CI" (Companion Intelligence) throughout as these are companions in development, not artificial constructs.

## Design Principles

1. **Zero Dependencies** - Pure C with only system libraries
2. **Protocol-First** - JSON messages over Unix sockets
3. **Embeddable** - Single library with clean C API
4. **Composable** - Unix philosophy of small tools
5. **CI-Agnostic** - Works with any LLM provider
6. **Defensive Coding** - Anticipate and handle all failure modes
7. **Deterministic Control** - CIs provide input, never control flow

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    AI IDE Layer                         │
│  (Cursor, Windsurf, Continue.dev, VSCode + Extension)   │
└────────────┬────────────────────────┬───────────────────┘
             │                        │
     ┌───────▼──────┐        ┌───────▼──────┐
     │   LSP Server │        │   libargo    │
     │   (argo-lsp) │        │  (Direct C)  │
     └───────┬──────┘        └───────┬──────┘
             │                        │
     ┌───────▼────────────────────────▼───────────────────┐
     │        Deterministic Control Layer                  │
     ├──────────────────────────────────────────────────── │
     │ • Message Router (argo_router)                      │
     │ • Session Manager (argo_session)                    │
     │ • Workflow Controller (argo_workflow)               │
     │ • Error Handler (argo_error)                        │
     └────────────┬────────────────────────────────────────┘
                  │ Interleaved Execution
                  │
     ┌────────────▼────────────────────────────────────────┐
     │           Creative CI Layer                         │
     ├──────────────────────────────────────────────────── │
     │ • CI Providers (Claude, GPT, Local)                 │
     │ • Memory Management (sunset/sunrise)                │
     │ • Negotiation Proposals                             │
     └──────────────────────────────────────────────────── ┘
```

### Layer Separation

**Deterministic Layer** (Argo Core):
- Controls all workflows and state transitions
- Manages Git branches and merges
- Routes messages between CIs
- Handles all errors defensively
- Never delegates control to CIs

**Creative Layer** (CI Agents):
- Proposes solutions and approaches
- Writes code and tests
- Evaluates conflicts
- Provides creative input
- Never controls infrastructure

## Core Components

### 1. Message Router (`src/argo_router.c`)
- Routes JSON messages between CI sessions
- Manages socket connections
- Implements structured message protocol

### 2. Session Manager (`src/argo_session.c`)
- Tracks active AI development sessions
- Manages parallel branch contexts
- Coordinates inter-session communication

### 3. Merge Negotiator (`src/argo_merge.c`)
- Detects merge conflicts
- Orchestrates AI negotiation
- Applies consensus resolutions

### 4. Workflow Controller (`src/argo_workflow.c`)
- Implements 7-phase development workflow
- Manages sprint branches
- Controls deterministic/CI interleaving
- Enforces sprint=branch philosophy

### 5. CI Provider Interface (`src/argo_ci.c`)
- Abstract interface for all CI providers
- Special optimizations for Claude Code (Max account)
- Manages provider configurations and connections

### 6. CI Registry (`src/argo_registry.c`)
- Tracks all active CIs and their roles
- Maps names to socket ports
- Manages CI discovery and status

### 7. Logging System (`src/argo_log.c`)
- Structured logging with levels
- Writes to `.argo/logs/` directory
- Can be enabled/disabled at runtime

## Data Structures

### Core Context
```c
typedef struct argo_context {
    int socket_fd;
    char session_id[ARGO_SESSION_ID_LEN];
    argo_session_t* sessions;
    argo_ai_provider_t* provider;
    void* user_data;
} argo_context_t;
```

### Message Format
```c
typedef struct argo_message {
    char id[ARGO_MSG_ID_LEN];
    char from[ARGO_SESSION_ID_LEN];
    char to[ARGO_SESSION_ID_LEN];
    char type[ARGO_MSG_TYPE_LEN];
    char* payload;
    size_t payload_len;
    time_t timestamp;
} argo_message_t;
```

### Session Structure
```c
typedef struct argo_session {
    char id[ARGO_SESSION_ID_LEN];
    char branch[ARGO_BRANCH_NAME_LEN];
    pid_t pid;
    int socket_fd;
    argo_session_state_t state;
    struct argo_session* next;
} argo_session_t;
```

## Protocol Specification

### Message Protocol (JSON over Socket)
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

### Merge Negotiation Protocol
```json
{
  "type": "merge_request",
  "payload": {
    "branch_a": "feature-1",
    "branch_b": "feature-2",
    "conflicts": [
      {
        "file": "main.c",
        "line_start": 45,
        "line_end": 67,
        "content_a": "...",
        "content_b": "..."
      }
    ]
  }
}
```

## API Design

### Initialization
```c
argo_context_t* argo_init(const char* config_path);
void argo_cleanup(argo_context_t* ctx);
```

### Session Management
```c
argo_session_t* argo_session_create(argo_context_t* ctx, const char* branch);
int argo_session_destroy(argo_context_t* ctx, const char* session_id);
argo_session_t* argo_session_find(argo_context_t* ctx, const char* session_id);
```

### Messaging
```c
int argo_send(argo_context_t* ctx, argo_message_t* msg);
argo_message_t* argo_receive(argo_context_t* ctx, int timeout_ms);
int argo_broadcast(argo_context_t* ctx, const char* type, const char* payload);
```

### Merge Operations
```c
argo_merge_result_t* argo_merge_detect(const char* branch_a, const char* branch_b);
int argo_merge_negotiate(argo_context_t* ctx, argo_merge_result_t* conflicts);
int argo_merge_apply(argo_merge_result_t* resolution);
```

### AI Provider Interface
```c
int argo_ai_connect(argo_context_t* ctx, argo_ai_provider_t* provider);
char* argo_ai_query(argo_context_t* ctx, const char* prompt);
int argo_ai_stream(argo_context_t* ctx, const char* prompt,
                   argo_stream_callback_t callback);
```

## Port Management

### Dynamic Port Assignment
Ports are assigned based on `ARGO_CI_BASE_PORT` from `.env.argo_local`:
- Builder CIs: BASE+0 to BASE+9
- Coordinator CIs: BASE+10 to BASE+19
- Requirements CIs: BASE+20 to BASE+29
- Analysis CIs: BASE+30 to BASE+39
- Reserved: BASE+40 to BASE+49

`ARGO_CI_PORT_MAX=50` defines the spacing between instances.

## File Organization

```
argo/
├── .argo/                   # Runtime data (git-ignored)
│   └── logs/               # Log files
│       ├── argo.log        # Current log
│       └── argo-*.log      # Daily archives
├── include/
│   ├── argo.h              # Main public API
│   ├── argo_types.h        # Type definitions
│   ├── argo_protocol.h     # Protocol constants
│   ├── argo_ci.h           # CI provider interface
│   ├── argo_ci_defaults.h  # Model defaults
│   ├── argo_error.h        # Error definitions
│   ├── argo_log.h          # Logging system
│   ├── argo_memory.h       # Memory management
│   ├── argo_registry.h     # CI registry
│   └── argo_local.h        # Local configuration
├── src/
│   ├── argo_router.c       # Message routing
│   ├── argo_session.c      # Session management
│   ├── argo_merge.c        # Merge negotiation
│   ├── argo_ai.c           # AI provider interface
│   ├── argo_socket.c       # Socket utilities
│   ├── argo_json.c         # JSON parsing
│   └── main.c              # CLI entry point
├── providers/
│   ├── claude.c            # Claude-specific optimizations
│   ├── openai.c            # OpenAI provider
│   └── ollama.c            # Local model provider
├── tests/
│   ├── test_router.c       # Router tests
│   ├── test_session.c      # Session tests
│   ├── test_merge.c        # Merge tests
│   └── test_integration.c  # End-to-end tests
└── tools/
    ├── argo-lsp/           # LSP server
    └── argo-cli/           # Command line tools
```

## Performance Targets

- Message latency: < 10ms local, < 100ms network
- Memory footprint: < 10MB base, < 1MB per session
- Concurrent sessions: 100+ per instance
- Merge resolution: < 5 seconds for typical conflicts

## Security Considerations

1. **No API Keys in Code** - All credentials via config files
2. **Socket Permissions** - Unix socket with user-only access
3. **Message Validation** - All inputs sanitized
4. **Rate Limiting** - Built-in DoS protection
5. **Audit Logging** - Optional message logging

## Integration Points

### Git Integration
- Direct libgit2 integration (optional)
- Shell command fallback
- Branch management automation

### IDE Integration Options
1. **Direct Library** - Link libargo.a
2. **LSP Server** - Connect via Language Server Protocol
3. **CLI Tools** - Subprocess execution
4. **WebAssembly** - Browser-based IDEs

## Claude Code Optimizations

Special features for Claude Max accounts:
```c
typedef struct argo_claude_config {
    int enable_artifacts;
    int parallel_sessions;
    int use_projects;
    const char* api_key;
} argo_claude_config_t;
```

## Testing Strategy

1. **Unit Tests** - Each component isolated
2. **Integration Tests** - Component interactions
3. **Protocol Tests** - Message format validation
4. **Performance Tests** - Latency and throughput
5. **Chaos Tests** - Network failures, crashes

## Future Considerations

- **Plugin System** - Dynamic provider loading
- **Clustering** - Multi-machine coordination
- **Persistence** - Session state recovery
- **Analytics** - Merge success metrics
- **ML Integration** - Conflict pattern learning