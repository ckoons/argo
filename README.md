# Argo - Automated Resolution of GitHub Obstacles
© 2025 Casey Koons. All rights reserved.

**Multi-CI Workflow Orchestration with Parallel Development and Merge Negotiation**

Argo is a minimal C library (<10,000 lines) that coordinates multiple Companion Intelligences (CIs) in software development workflows. Like Jason's ship helped the Argonauts overcome obstacles to reach the Golden Fleece, Argo helps CIs overcome GitHub obstacles - merge conflicts, CI failures, and coordination problems. It provides deterministic control while CIs provide creative contribution.

## Status: Sprint 1 - Multi-CI Workflows (Complete)

Current: 2,950 meaningful lines (29% of budget)

### What Works Now
- ✅ **8 CI Providers**: Ollama, Claude (socket/code/API), OpenAI, Gemini, Grok, DeepSeek, OpenRouter
- ✅ **Common Utilities**: JSON parsing, HTTP helpers, API abstractions
- ✅ **Error System**: Human-readable messages with TYPE:NUMBER encoding
- ✅ **Configuration**: Optional local model overrides
- ✅ **Tooling**: Model update script, diet-aware line counter
- ✅ **CI Registry**: Track multiple CIs with roles, status, and statistics
- ✅ **Lifecycle Manager**: CI state transitions, task assignment, heartbeats
- ✅ **Memory Manager**: Sunset/sunrise protocol, digest generation
- ✅ **Message Routing**: Inter-CI communication with threading and metadata
- ✅ **Workflow Controller**: 7-phase workflow state machine, auto-assignment
- ✅ **Merge Negotiation**: Conflict tracking, CI proposals, resolution selection
- ✅ **Error Reporting**: Single standard routine with severity-based routing
- ✅ **Workflow JSON**: Reusable workflow definitions in `argo/workflows/{category}/{event}/`
- ✅ **Session Lifecycle**: `run_workflow()` - complete create→execute→destroy
- ✅ **Checkpoints**: Workflow pause/resume with state save/restore
- ✅ **Testing**: 62/62 tests passing (100% pass rate)

### Core Principles
1. **"What you don't build, you don't debug"** - Reuse over rewrite
2. **Simple beats clever** - Follow patterns that work
3. **Deterministic vs Creative**: Argo controls workflow, CIs provide solutions
4. **Sprint = Branch**: Each sprint is an atomic Git branch
5. **No Mocks**: Working with real CIs from day one
6. **Provider Agnostic**: Works with any model/API
7. **Lean Code**: Stay under 10,000 meaningful lines

## Quick Start

```bash
# Build everything
make clean && make

# Run fast tests (no API calls)
make test-quick

# Test specific components
make test-registry     # CI registry and messaging
make test-memory       # Memory management
make test-lifecycle    # CI lifecycle and state
make test-providers    # Provider system
make test-messaging    # Inter-CI messaging
make test-workflow     # Workflow orchestration

# Check code size (diet-aware counting)
./scripts/count_core.sh

# Test API providers (costs real money!)
build/test_api_calls --yes-i-want-to-spend-money claude
build/test_api_calls --yes-i-want-to-spend-money openai
build/test_api_calls --yes-i-want-to-spend-money gemini
```

## Architecture

Argo follows a strict **deterministic/creative separation**:

**Deterministic Layer** (Argo Controls):
- Workflow state machine and phase transitions
- CI registry and lifecycle management
- Message routing between CIs
- Task assignment based on roles
- Merge conflict detection and orchestration

**Creative Layer** (CIs Contribute):
- Code implementation
- Merge conflict resolution proposals
- Test writing and execution
- Design decisions within assigned tasks

## Documentation
- [Architecture](docs/plans/architecture.md) - System design and protocols
- [Sprint 0 Plan](docs/plans/sprint-0-ci-foundation.md) - Provider infrastructure
- [CLAUDE.md](CLAUDE.md) - Development collaboration guide

## Next: Sprint 2 - Real CI Integration
- Load and execute workflow JSON definitions
- Connect to real CI providers (Claude, GPT-4, Gemini)
- End-to-end workflow execution with live CIs
- Git operations (branch creation, commits, merges)
