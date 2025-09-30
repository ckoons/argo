# Argo
© 2025 Casey Koons. All rights reserved.

**Automated Companion Intelligence (CI) Workflow with Parallel Development and Merge Negotiation**

Argo is a minimal C library (<10,000 lines) that coordinates multiple Companion Intelligences (CIs) in software development workflows. It provides deterministic control while CIs provide creative contribution.

## Status: Sprint 0 - CI Foundation (95% Complete)

Current: 1,054 meaningful lines (10.5% of budget)

### What Works Now
- ✅ **8 CI Providers**: Ollama, Claude (socket/code/API), OpenAI, Gemini, Grok, DeepSeek, OpenRouter
- ✅ **Error System**: Human-readable messages with TYPE:NUMBER encoding
- ✅ **Configuration**: Optional local model overrides
- ✅ **Tooling**: Model update script, diet-aware line counter

### Core Principles
1. **Deterministic vs Creative**: Argo controls workflow, CIs provide solutions
2. **Sprint = Branch**: Each sprint is an atomic Git branch
3. **No Mocks**: Working with real CIs from day one
4. **Provider Agnostic**: Works with any model/API
5. **Lean Code**: Stay under 10,000 meaningful lines

## Quick Start

```bash
# Build
make clean && make

# Test providers (costs real money!)
build/test_api_calls --yes-i-want-to-spend-money claude
build/test_api_calls --yes-i-want-to-spend-money openai
build/test_api_calls --yes-i-want-to-spend-money gemini

# Update model defaults
make update-models

# Check code size
./scripts/count_core.sh
```

## Documentation
- [Sprint 0 Plan](docs/plans/sprint-0-ci-foundation.md) - Current sprint details
- [Architecture](docs/plans/architecture.md) - System design

## Next: Sprint 1
- Multi-CI parallel development
- Automated merge negotiation
- Workflow controller
