# Argo Daemon Architecture

© 2025 Casey Koons. All rights reserved.

## Overview

The Argo daemon (`argo-daemon`) is the central orchestration service that manages workflow execution, CI registry, and resource lifecycle. It provides a REST API over HTTP for communication between `arc` CLI, workflow executors, and future tools.

**Design Philosophy:** Loosely-coupled message passing that works identically for localhost and distributed systems.

## Architecture Pattern

```
arc CLI  <--HTTP-->  argo-daemon  <--fork/exec-->  workflow_executor (background)
   ↓                      ↓                               ↓
Commands            Orchestration                   Step Execution
User I/O            State Management                CI Interaction
                    I/O Queue (HTTP)                Progress Reporting (HTTP)
                         ↑                               ↓
                         └────────HTTP I/O Channel───────┘
```

**Communication:**
- `arc → daemon`: HTTP REST API (POST, GET, DELETE)
- `daemon → executor`: fork/exec process creation + signals (SIGTERM, SIGINT)
- `executor → daemon`: HTTP progress updates + I/O channel reads/writes
- `arc ↔ daemon ↔ executor`: Interactive workflow I/O via HTTP message passing

**Background Process Architecture:**
- **Daemon**: Background service, NO stdin/stdout/stderr (logs only)
- **Executor**: Background process, NO stdin/stdout/stderr (logs + HTTP I/O channel)
- **Arc CLI**: Terminal-facing only, handles ALL user interaction

### HTTP I/O Channel Architecture

**Problem:** Interactive workflows need user input/output, but executor runs in background (no stdin/stdout).

**Solution:** HTTP-based I/O channel that routes all user interaction through daemon:

```
User (terminal)
    ↓ stdin
arc CLI
    ↓ HTTP POST /api/workflow/input/{id}
argo-daemon (I/O queue)
    ↓ HTTP GET /api/workflow/input/{id}
workflow_executor (polls with HTTP)
    ↓ io_channel_read_line()
Workflow Step (user_ask, ci_ask, etc)
    ↓ generates response
    ↓ io_channel_write_str() → HTTP POST /api/workflow/output/{id}
argo-daemon (I/O queue)
    ↓ HTTP GET /api/workflow/output/{id}
arc CLI
    ↓ stdout
User (terminal)
```

**Implementation:**
- `io_channel_t` abstraction supports: sockets, socketpairs, null device, **HTTP**
- Executor creates HTTP I/O channel on startup: `io_channel_create_http(daemon_url, workflow_id)`
- All workflow steps use `io_channel_write_str()` / `io_channel_read_line()`
- HTTP channel polls daemon with 100ms delays, 30-second timeout
- Buffered writes reduce HTTP overhead
- Non-blocking reads return `E_IO_WOULDBLOCK` when no data available

**Files:**
- `include/argo_io_channel.h` - Base I/O channel interface
- `src/workflow/argo_io_channel.c` - Dispatch to type-specific implementations
- `include/argo_io_channel_http.h` - HTTP-specific interface
- `src/workflow/argo_io_channel_http.c` - CURL-based HTTP I/O (343 lines)

### Why HTTP Message Passing?

1. **Location Independence:** Works identically on localhost or between machines
2. **Loose Coupling:** No shared memory, clean API boundaries
3. **Fault Isolation:** Executor crash doesn't kill daemon
4. **Observability:** Log all HTTP requests for debugging
5. **Language Agnostic:** Future executors in Python/Go/Rust
6. **Testability:** Easy to mock HTTP endpoints
7. **Background Execution:** Executor can run detached with no terminal access

## Process Model

### Daemon Lifecycle

```
System Boot (optional)
    ↓
arc command → check daemon running → start if needed
    ↓
argo-daemon (persistent process)
    - Listens on HTTP port (default: 9876)
    - Loads persistent state (registry, workflows)
    - Handles API requests
    - Forks workflow executors on demand
```

### Workflow Execution

```
arc workflow start <template> <instance>
    ↓
HTTP POST /api/workflow/start
    ↓
Daemon validates, creates workflow entry
    ↓
Daemon fork() + execv(argo_workflow_executor)
    ↓
Executor connects back to daemon via HTTP
    ↓
Executor loads template, executes steps
    ↓
Executor reports progress via HTTP POST
    ↓
Executor exits, daemon updates workflow status
```

## Component Responsibilities

### Daemon (Persistent Service)

**Manages:**
- CI Registry (system-wide CI tracking)
- Lifecycle Manager (resource management)
- Workflow Registry (active workflows)
- Process Management (fork/monitor executors)

**Does NOT:**
- Execute workflow steps (that's executor's job)
- Call CI providers directly (executor does this)
- Block waiting for workflows (async via messaging)

### Executor (Per-Workflow Background Process)

**Background Service:** Executor runs completely detached from terminal with NO stdin/stdout/stderr access.

**Manages:**
- Workflow Controller (workflow-specific state)
- Step Execution (call CI, capture output)
- Progress Reporting (HTTP to daemon)
- HTTP I/O Channel (for interactive workflows)
- Cleanup on completion

**Does NOT:**
- Use stdin/stdout/stderr (all I/O via HTTP channel or logs)
- Manage other workflows (isolated per-workflow)
- Persist registry/lifecycle (daemon's job)
- Handle pause/resume signals (daemon sends SIGSTOP/SIGCONT)
- Output to terminal directly (only arc CLI does this)

**I/O Patterns:**
- **Startup**: Minimal diagnostics to stderr (before becoming background)
- **Runtime**: All status via LOG_INFO/LOG_ERROR → log files
- **Interactive Steps**: All user I/O via io_channel (HTTP) → daemon → arc CLI
- **Errors**: argo_report_error() → log files only

## Library Structure

### libargo_core.a (Foundation)

**Contents:**
- Error handling (`argo_error.c`)
- HTTP client (`argo_http.c`)
- JSON parsing (`argo_json.c`)
- Socket utilities (`argo_socket.c`)
- All CI providers (`argo_*_api.c`)
- Common utilities (`argo_api_common.c`, `argo_ci_common.c`)

**Used by:** Daemon, Executor, Arc

### libargo_daemon.a (Daemon-Specific)

**Contents:**
- HTTP server (`argo_http_server.c`)
- Daemon core (`argo_daemon.c`)
- Registry (`argo_registry.c`, `argo_registry_persistence.c`)
- Lifecycle (`argo_lifecycle.c`, `argo_lifecycle_monitoring.c`)
- API handlers (`argo_daemon_api.c`)
- Orchestrator (`argo_orchestrator.c`)

**Used by:** Daemon only

### libargo_workflow.a (Executor-Specific)

**Contents:**
- Workflow controller (`argo_workflow.c`)
- Workflow loader (`argo_workflow_loader.c`)
- Workflow executor (`argo_workflow_executor.c`)
- Step implementations (`argo_workflow_steps_*.c`)
- Checkpoint/resume (`argo_workflow_checkpoint.c`)
- Persona support (`argo_workflow_persona.c`)
- **I/O Channel** (`argo_io_channel.c`, `argo_io_channel_http.c`)

**Used by:** Executor only

**Key Feature:** All workflow steps use `io_channel_t` abstraction for I/O, enabling background execution without terminal access.

### Arc CLI (Minimal)

**Contents:**
- Command parsing (`arc/src/*.c`)
- HTTP calls to daemon
- Output formatting

**Links:** `libargo_core.a` only (HTTP client)

## REST API Design

### Daemon Endpoints

#### Registry API

```
GET  /api/registry/ci              List all CIs
POST /api/registry/ci              Register new CI
GET  /api/registry/ci/{name}       Get CI details
DELETE /api/registry/ci/{name}     Unregister CI
```

#### Lifecycle API

```
POST /api/workflow/start           Start new workflow
GET  /api/workflow/list            List all workflows
GET  /api/workflow/status/{id}     Get workflow status
POST /api/workflow/pause/{id}      Pause workflow (SIGSTOP)
POST /api/workflow/resume/{id}     Resume workflow (SIGCONT)
DELETE /api/workflow/abandon/{id}  Abandon workflow (SIGTERM)
```

#### Executor Communication API

```
POST /api/workflow/progress/{id}       Executor reports progress
POST /api/workflow/{id}/step/start     Executor reports step start
POST /api/workflow/{id}/step/complete  Executor reports step completion
POST /api/workflow/{id}/step/output    Executor sends step output
GET  /api/workflow/{id}/messages       Executor polls for daemon messages (long-poll)
POST /api/workflow/{id}/message        Executor sends message to daemon
```

#### Interactive Workflow I/O API (HTTP I/O Channel)

```
POST /api/workflow/output/{id}         Executor writes output to daemon queue
GET  /api/workflow/output/{id}         Arc reads output from daemon queue
POST /api/workflow/input/{id}          Arc writes user input to daemon queue
GET  /api/workflow/input/{id}          Executor polls for user input (non-blocking)
```

**Flow:**
1. Workflow step calls `io_channel_write_str()` → HTTP POST to `/api/workflow/output/{id}`
2. Arc CLI polls GET `/api/workflow/output/{id}` → displays to user
3. User types input in arc → HTTP POST to `/api/workflow/input/{id}`
4. Workflow step polls GET `/api/workflow/input/{id}` with retry logic → receives input

#### Daemon Management

```
GET  /api/health                   Health check (daemon running)
GET  /api/version                  Daemon version and binary hash
POST /api/daemon/reload            Trigger daemon auto-update
POST /api/daemon/shutdown          Graceful shutdown
```

### Request/Response Format

All requests/responses use JSON with standard format:

**Request:**
```json
{
  "workflow_id": "code_review_test_1",
  "template": "code_review",
  "branch": "main",
  "environment": "dev"
}
```

**Success Response:**
```json
{
  "status": "success",
  "data": {
    "workflow_id": "code_review_test_1",
    "pid": 12345,
    "state": "running"
  }
}
```

**Error Response:**
```json
{
  "status": "error",
  "error": {
    "code": 1004,
    "message": "Template not found",
    "details": "Template 'code_review' does not exist"
  }
}
```

## Implemented REST API

### Core Workflow Operations

**POST /api/workflow/start**
- Start new workflow from template
- Body: `{"template":"name","instance":"id","branch":"main","environment":"dev"}`
- Returns: `{"status":"success","workflow_id":"...","environment":"..."}`
- Errors: 404 (template not found), 409 (duplicate), 500 (internal)

**GET /api/workflow/list**
- List all active workflows
- Returns: `{"workflows":[{"workflow_id":"...","status":"...","pid":123}]}`

**GET /api/workflow/status/{id}**
- Get status of specific workflow
- Returns: `{"workflow_id":"...","status":"...","pid":123,"template":"..."}`
- Errors: 404 (not found)

**DELETE /api/workflow/abandon/{id}**
- Terminate workflow and remove from registry
- Returns: `{"status":"success","workflow_id":"...","action":"abandoned"}`
- Errors: 404 (not found), 500 (kill failed)

**POST /api/workflow/progress/{id}**
- Executor reports progress (called by executor, not arc)
- Body: `{"current_step":2,"total_steps":4,"step_name":"..."}`
- Returns: `{"status":"success","workflow_id":"..."}`
- Logged to daemon stderr: `[PROGRESS] workflow_id: step X/Y (step_name)`

### Future Endpoints (Stubs)

**POST /api/workflow/pause/{id}** - Pause workflow (stub, returns success)
**POST /api/workflow/resume/{id}** - Resume workflow (stub, returns success)

### Health & Info

**GET /api/health** - Daemon health check
**GET /api/version** - Daemon version info

## Current Implementation Status

### ✅ Fully Implemented

- **Daemon core**: HTTP server (350 lines, zero dependencies), registry, lifecycle
- **REST API**: All workflow operations (start, list, status, abandon, progress)
- **Executor**: Complete workflow execution with step-by-step progress reporting
- **Arc integration**: workflow_start, workflow_status, workflow_abandon use HTTP
- **Bi-directional messaging**: Executor→Daemon progress via HTTP POST
- **Process management**: fork/exec workflow executors, signal handling
- **Error handling**: Consistent HTTP status codes, JSON error responses
- **HTTP I/O Channel**: Complete implementation for interactive workflows (543 lines)
  - Base abstraction: `io_channel_t` supporting sockets, socketpairs, null, HTTP
  - HTTP implementation: CURL-based with polling, buffering, timeout handling
  - All workflow steps converted: user_ask, user_choose, display, ci_ask, ci_ask_series, ci_present, ci_chat
  - Background-only execution: NO stdin/stdout/stderr in runtime loop

### 🔄 Partial/Stub Implementation

- **Pause/Resume**: API endpoints return success but don't actually pause workflows
- **workflow_list**: Arc command still uses direct registry access (not yet HTTP)
- **Message queues**: Conceptual design documented, not implemented (HTTP sufficient for MVP)

### 📋 Design-Only (Future Work)

- **Long-polling**: For real-time updates (documented, not implemented)
- **WebSocket upgrade**: For streaming progress (design phase)
- **Distributed daemon**: Multi-host coordination (architectural notes only)

## Messaging Design (Reference)

```c
typedef struct {
    char id[37];           /* UUID */
    char type[32];         /* Message type */
    char payload[4096];    /* JSON data */
    time_t timestamp;
} message_t;

typedef struct {
    message_t* messages;
    size_t count;
    size_t capacity;
    pthread_mutex_t lock;
} message_queue_t;
```

### Message Types

**Daemon → Executor:**
- `pause` - Pause workflow execution
- `resume` - Resume workflow execution
- `abort` - Abort workflow immediately
- `update_config` - Update configuration

**Executor → Daemon:**
- `step_start` - Step execution started
- `step_output` - Step produced output
- `step_complete` - Step completed
- `workflow_complete` - Workflow finished
- `error` - Error occurred

### Long-Polling

Executor uses long-polling to reduce HTTP overhead:

```
GET /api/workflow/{id}/messages?timeout=30
```

- Blocks up to 30 seconds if no messages
- Returns immediately if messages available
- Prevents busy-polling while staying responsive

## Configuration

### Port Configuration

**Default:** 9876 (defined in `.env.argo`)

```bash
# .env.argo
ARGO_DAEMON_PORT=9876
```

**Override:**
```bash
argo-daemon --port 9999
```

**Discovery:**
```c
uint16_t port = getenv("ARGO_DAEMON_PORT") ? atoi(getenv("ARGO_DAEMON_PORT")) : 9876;
```

### Authentication (Future)

Structure in place, implementation deferred:

```c
typedef struct {
    char token[64];
    time_t expires;
    char user[32];
} auth_token_t;

#ifdef ARGO_AUTH_ENABLED
    /* Validate auth token */
#else
    /* No auth for now */
#endif
```

## Daemon Auto-Update

### Version Detection

On daemon startup, check if binary on disk differs from running binary:

```c
int daemon_check_version(void) {
    char running_hash[65];
    char disk_hash[65];

    get_file_hash("/proc/self/exe", running_hash);
    get_file_hash(DAEMON_BINARY_PATH, disk_hash);

    if (strcmp(running_hash, disk_hash) != 0) {
        return daemon_exec_update();
    }
    return ARGO_SUCCESS;
}
```

### Update Process

1. Save all persistent state
2. Fork new daemon with new binary
3. Old daemon shuts down gracefully
4. New daemon loads saved state
5. Seamless transition

**Trigger:**
```bash
make install-daemon  # Updates binary
curl -X POST http://localhost:9876/api/daemon/reload  # Explicit reload
```

## HTTP Server Implementation

### Why Build Our Own?

- Only need REST API (no static files, websockets)
- ~500 lines of code we fully control
- Zero dependencies (no mongoose, libmicrohttpd)
- Grow exactly as we need

### Server Structure

```c
typedef struct {
    int socket_fd;
    uint16_t port;
    route_t* routes;          /* Path → handler mappings */
    int running;
} http_server_t;

typedef int (*route_handler_fn)(http_request_t* req, http_response_t* resp);

typedef struct {
    char method[8];           /* GET, POST, DELETE */
    char path[256];           /* /api/workflow/start */
    route_handler_fn handler;
} route_t;
```

### Request Handling

1. Accept connection (blocking or select/poll)
2. Parse HTTP headers
3. Read request body (Content-Length bytes)
4. Route to handler based on method + path
5. Handler builds response JSON
6. Send HTTP response
7. Close connection

### Threading Model

**Thread-per-connection** for simplicity:
- Main thread: accept() connections
- Spawn thread per request
- Thread handles request and exits
- Works great for <100 concurrent requests

## State Persistence

### Registry State

**File:** `~/.argo/registry/ci_registry.json`

```json
{
  "cis": [
    {
      "name": "claude-code-1",
      "model": "claude-sonnet-4",
      "port": 45001,
      "socket_path": "/tmp/argo_claude_1.sock",
      "status": "running"
    }
  ]
}
```

### Workflow State

**File:** `~/.argo/workflows/registry/active_workflow_registry.json`

```json
{
  "workflows": [
    {
      "workflow_id": "code_review_test_1",
      "template": "code_review",
      "pid": 12345,
      "status": "running",
      "current_step": 2,
      "started": "2025-10-06T10:30:00Z"
    }
  ]
}
```

### Lifecycle State

**File:** `~/.argo/lifecycle/sessions.json`

```json
{
  "sessions": [
    {
      "session_id": "sess_123",
      "workflows": ["code_review_test_1"],
      "created": "2025-10-06T10:00:00Z"
    }
  ]
}
```

## Migration Path

### Phase 1: Library Split
- Refactor Makefile to build three libraries
- Verify all tests pass with new structure
- No functionality changes

### Phase 2: Basic Daemon
- Implement HTTP server
- Create daemon binary with health endpoint
- Verify `curl http://localhost:9876/health` works

### Phase 3: Daemon API
- Implement registry/lifecycle endpoints
- Wire to existing functions
- Arc uses HTTP instead of direct calls

### Phase 4: Async Messaging
- Add message queues
- Implement long-polling
- Executor reports progress

### Phase 5: Full Workflow
- Complete workflow execution through daemon
- End-to-end test with CI integration
- Production ready

## Benefits of This Design

1. **Scalability:** Run executors on different machines
2. **Fault Tolerance:** Executor crash doesn't kill daemon
3. **Observability:** HTTP logs show all interactions
4. **Testability:** Mock HTTP endpoints
5. **Flexibility:** Future executors in any language
6. **Security:** Add auth/TLS later without redesign
7. **Multi-tenancy:** Multiple users share daemon

## Estimated Scope

**New Code:**
- HTTP server: ~500 lines
- Daemon core: ~300 lines
- API handlers: ~800 lines
- Messaging: ~400 lines
- **Total new:** ~2000 lines

**Modified Code:**
- Arc commands: ~300 lines
- Executor main: ~200 lines
- **Total modified:** ~500 lines

**Deleted Code:**
- Direct orchestrator calls: ~200 lines

**Net Addition:** ~2300 lines (within 10k budget)

## Testing Strategy

### Phase 1 Tests
- `make test-quick` passes (all existing tests)

### Phase 2 Tests
- Daemon starts and responds to health check
- Daemon shutdown works cleanly

### Phase 3 Tests
- Registry API (CRUD operations)
- Lifecycle API (start/pause/resume/abandon)
- Error handling (invalid requests)

### Phase 4 Tests
- Message queue operations
- Long-polling behavior
- Concurrent message handling

### Phase 5 Tests
- Full workflow execution
- Multi-workflow concurrency
- Fault tolerance (executor crashes)
- State persistence across daemon restarts

---

*This architecture makes Argo truly distributed and production-ready while maintaining simplicity and clarity.*
