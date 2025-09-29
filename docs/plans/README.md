© 2025 Casey Koons All rights reserved

# Argo Development Plans

This directory contains the strategic planning and sprint documentation for the Argo project - a C-based implementation of key Tekton components designed to test market fitness with AI IDE makers.

## Document Structure

### Core Planning Documents

- **architecture.md** - Technical architecture and design decisions
- **market-strategy.md** - Market positioning and adoption strategy
- **sprint-1-foundation.md** - Foundation sprint (Weeks 1-2)
- **sprint-2-core.md** - Core features sprint (Weeks 3-4)
- **sprint-3-integration.md** - Integration sprint (Weeks 5-6)
- **sprint-4-demo.md** - Demo and packaging sprint (Weeks 7-8)

### Supporting Documents

- **component-mapping.md** - Tekton to Argo component mapping
- **api-design.md** - API and protocol specifications
- **testing-strategy.md** - Testing and validation approach
- **release-plan.md** - Release and distribution strategy

## Sprint Overview

### Sprint 1: Foundation (Weeks 1-2)
- Port aish to C
- Basic socket communication
- Message protocol implementation
- Unit test framework

### Sprint 2: Core Features (Weeks 3-4)
- Terma session management
- Inter-AI communication
- Basic merge negotiation protocol
- Integration tests

### Sprint 3: Integration (Weeks 5-6)
- AI provider abstraction
- Claude Code optimizations
- Library packaging (libargo)
- LSP server implementation

### Sprint 4: Demo & Release (Weeks 7-8)
- VSCode extension
- Demo applications
- Documentation
- Market outreach

## Success Metrics

1. **Technical**
   - < 10,000 lines of C code
   - Zero runtime dependencies
   - < 100ms message latency
   - 100% test coverage for core functions

2. **Market**
   - 3+ AI IDE maker conversations
   - 1+ proof of concept integration
   - Demonstrable 50% reduction in merge conflicts
   - Clear ROI documentation

3. **Adoption**
   - Working VSCode extension
   - Published LSP specification
   - Open source release plan
   - China market compatibility verified

## Key Principles

1. **Simplicity First** - Every line of code must justify its existence
2. **Protocol Over Implementation** - Define interfaces, not implementations
3. **Show, Don't Tell** - Working demos over documentation
4. **No Vendor Lock-in** - Must work with any AI provider
5. **Unix Philosophy** - Small, composable tools

## Reading Order

For new team members:
1. Start with **architecture.md** for technical overview
2. Read **market-strategy.md** for business context
3. Review current sprint document
4. Check **component-mapping.md** for Tekton knowledge transfer

## Status Tracking

Each sprint document includes:
- Objectives and deliverables
- Task breakdown with estimates
- Dependencies and risks
- Completion criteria
- Actual vs planned outcomes

## Notes

- All plans are living documents - update as we learn
- Focus on demonstrable value, not perfect code
- Prioritize adoption over features
- Keep Casey's philosophy: "Better lazy solutions than complex ones"