# Argo Workflows - OS-Based Orchestration System

© 2025 Casey Koons. All rights reserved.

**Argo Workflows** is a bash-based workflow orchestration system that coordinates multiple AI coding assistants (CI) in parallel development workflows using OS-level process management and a state-file relay pattern.

## Quick Start

### Prerequisites

- Bash 4.0+
- jq (JSON processor)
- git
- A CI tool (companion intelligence) accessible via the `ci` command

### Basic Usage

```bash
# Initialize a new project
cd your-project-directory
../templates/project_setup.sh "my-project" .

# Start a workflow
./start_workflow.sh "Build a calculator with add/subtract/multiply/divide"

# Monitor progress
./status.sh

# View detailed logs
tail -f .argo-project/logs/workflow.log

# Stop workflow (if needed)
kill $(cat .argo-project/orchestrator.pid)
```

## Architecture Overview

### State-File Relay Pattern

Argo uses a **state-file relay pattern** where control passes between "code" (the orchestrator) and "ci" (AI assistants) via a shared state file:

```
┌─────────────────────────────────────────┐
│ State File (.argo-project/state.json)  │
│  - phase: current workflow phase        │
│  - action_owner: who acts next          │
│  - action_needed: what action to take   │
└─────────────────────────────────────────┘
         ↑                    ↓
    read/write           read/dispatch
         │                    │
    ┌────┴────┐         ┌────┴────┐
    │   CI    │         │  Code   │
    │ (human/ │   ←───→ │ (orch-  │
    │   AI)   │  relay  │ estrator│
    └─────────┘         └─────────┘
```

### Core Components

1. **orchestrator.sh** - Polling daemon that monitors state and dispatches phase handlers
2. **start_workflow.sh** - Entry point that initializes state and launches orchestrator
3. **Phase Handlers** - Executable scripts in `phases/` that implement workflow steps
4. **State File** - JSON file containing workflow state, shared between components
5. **Logging** - Structured logs in `.argo-project/logs/`

### Workflow Phases

Argo Sprint 2 implements a 5-phase workflow:

```
init → design → execution → merge_build → storage
```

1. **init** (design_program.sh)
   - Gathers requirements from user query
   - Queries CI for high-level design
   - Creates initial design document

2. **design** (design_build.sh)
   - Validates design components
   - Creates builder entries for each component
   - Sets up git worktrees for parallel work

3. **execution** (execution.sh)
   - Spawns CI sessions for each builder in parallel
   - Monitors builder progress via PIDs
   - Waits for all builders to complete

4. **merge_build** (merge_build.sh)
   - Merges builder branches back to main
   - Detects and handles merge conflicts
   - Cleans up git worktrees

5. **storage** (terminal state)
   - Workflow complete
   - Results stored in git
   - Orchestrator exits

## Key Features

### Polling Daemon
- Simple, Unix-like design
- 5-second poll interval (configurable)
- Graceful shutdown on SIGTERM/SIGINT
- Responsive signal handling (1-second increments)

### Parallel Builder Execution
- Multiple AI sessions work simultaneously
- Git worktrees provide isolation
- PID-based progress monitoring
- Automatic resource cleanup

### State Management
- Atomic updates via temp file + rename
- Locking support (perl-based flock)
- JSON-based for easy inspection
- Checkpointing support

### Error Recovery
- Handler failures logged to state
- Retry mechanisms available
- Manual intervention supported
- State always consistent

## Directory Structure

```
workflows/
├── orchestrator.sh          # Main daemon
├── start_workflow.sh        # Workflow entry point
├── status.sh                # Status display
├── phases/                  # Phase handler scripts
│   ├── design_program.sh
│   ├── design_build.sh
│   ├── execution.sh
│   └── merge_build.sh
├── lib/                     # Shared libraries
│   ├── state_file.sh        # State management
│   └── logging_enhanced.sh  # Logging utilities
├── templates/               # Project templates
│   └── project_setup.sh     # Initialize .argo-project
└── tests/                   # Test suite (43 tests)
    ├── test_start_workflow.sh
    ├── test_orchestrator.sh
    ├── test_design_program.sh
    ├── test_design_build.sh
    ├── test_execution.sh
    └── test_merge_build.sh
```

## Documentation

- **[ARCHITECTURE.md](ARCHITECTURE.md)** - Technical deep-dive into the design
- **[USER_GUIDE.md](USER_GUIDE.md)** - Complete user guide with examples
- **[DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md)** - How to create phase handlers
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Common issues and solutions

## Testing

Argo includes comprehensive test coverage:

```bash
# Run all Phase 2 tests
cd tests
./run_phase2_tests.sh

# Run individual test suites
./test_orchestrator.sh
./test_execution.sh

# Test results: 43/43 passing (100%)
```

## Design Principles

1. **Simple over Clever** - Unix philosophy, straightforward bash
2. **Observable** - All state visible in JSON, logs human-readable
3. **Testable** - Full test coverage with isolated test runs
4. **Debuggable** - Easy to understand, easy to troubleshoot
5. **Resilient** - Graceful failures, state always consistent

## Example Workflow

```bash
# 1. User starts workflow
$ ./start_workflow.sh "Build a REST API for user management"
Workflow started!
Orchestrator PID: 12345

# 2. Orchestrator (code) executes design_program.sh
#    - Queries CI with user requirement
#    - CI responds with high-level design
#    - Sets action_owner="ci" for approval

# 3. User/CI approves design (updates state)
#    - Sets action_owner="code"
#    - Sets action_needed="design_build"

# 4. Orchestrator executes design_build.sh
#    - Creates builders: api-routes, database, auth
#    - Sets up 3 git worktrees
#    - Transitions to execution phase

# 5. Orchestrator executes execution.sh
#    - Spawns 3 parallel CI sessions
#    - Each builds their component
#    - Waits for all to complete

# 6. Orchestrator executes merge_build.sh
#    - Merges 3 branches to main
#    - Handles any conflicts
#    - Cleans up worktrees
#    - Transitions to storage

# 7. Orchestrator exits (phase=storage)
#    - Workflow complete
#    - Code committed to git
```

## Performance

- **Startup**: <100ms
- **Poll Cycle**: 5 seconds
- **Parallel Builders**: Up to 10 (configurable)
- **Memory**: ~5MB per orchestrator
- **Disk**: State files <100KB

## Limitations

- No distributed orchestration (single machine)
- No built-in scheduling/cron
- Bash-specific (not portable to sh/dash)
- Requires git for parallel builders

## Future Enhancements

- [ ] Distributed orchestration across machines
- [ ] Web-based monitoring dashboard
- [ ] More phase handler examples
- [ ] Integration with CI/CD pipelines
- [ ] Workflow templates library

## License

© 2025 Casey Koons. All rights reserved.

## Support

For issues or questions:
1. Check [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
2. Review test examples in `tests/`
3. Examine logs in `.argo-project/logs/`
4. Inspect state in `.argo-project/state.json`
