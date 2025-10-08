# Argo Documentation

© 2025 Casey Koons. All rights reserved.

## Overview

This directory contains all documentation for the Argo multi-AI coordination platform. Documentation is organized by audience and purpose.

## Documentation Structure

```
docs/
├── README.md                    # This file - documentation map
├── guide/                       # User guides and tutorials
│   └── README.md               # Guide to user documentation
├── ai/                         # AI/CI training materials
│   └── README.md               # Guide to AI onboarding
├── api/                        # API reference documentation
├── examples/                   # Example code and workflows
├── make/                       # Build system documentation
└── plans/                      # Design docs and planning
```

## Quick Navigation

### 👤 I'm a **User** (Human Developer)

**Start here:** [`guide/README.md`](guide/README.md)

Essential reading:
- **Getting Started**: How to install and use Argo
- **Workflow Construction**: Build automated AI workflows
- **Arc CLI**: Command-line interface guide
- **Daemon Architecture**: How Argo executes workflows

### 🤖 I'm a **CI** (Companion Intelligence)

**Start here:** [`ai/README.md`](ai/README.md)

Essential reading:
- **Onboarding**: How to work with Argo codebase
- **Contributing**: Standards and patterns
- **Architecture**: System design and data flow

### 🔧 I'm **Contributing Code**

**Start here:** [`guide/CONTRIBUTING.md`](guide/CONTRIBUTING.md)

Key references:
- **Code Standards**: Memory safety, error handling, naming conventions
- **Build System**: How to compile and test
- **Testing**: Test requirements and patterns

### 📚 I'm **Looking for Examples**

**Start here:** [`examples/`](examples/)

Find:
- Sample workflows
- Integration examples
- Usage patterns

## Core Documentation Files

### User Guides (docs/guide/)

| File | Purpose | Audience |
|------|---------|----------|
| [`WORKFLOW_CONSTRUCTION.md`](guide/WORKFLOW_CONSTRUCTION.md) | Complete guide to building workflows | Users |
| [`WORKFLOW.md`](guide/WORKFLOW.md) | Workflow concepts and design | Users |
| [`ARC_TERMINAL.md`](guide/ARC_TERMINAL.md) | Arc CLI reference | Users |
| [`DAEMON_ARCHITECTURE.md`](guide/DAEMON_ARCHITECTURE.md) | How daemon executes workflows | Users + Developers |
| [`INITIALIZATION.md`](guide/INITIALIZATION.md) | Startup and configuration | Developers |
| [`CONTRIBUTING.md`](guide/CONTRIBUTING.md) | How to contribute code | Contributors |
| [`TERMINOLOGY.md`](guide/TERMINOLOGY.md) | Glossary of terms | Everyone |

### AI Training Materials (docs/ai/)

| File | Purpose | Audience |
|------|---------|----------|
| [`ONBOARDING.md`](ai/ONBOARDING.md) | How AIs should work with Argo | AIs/CIs |

### API Reference (docs/api/)

Low-level API documentation for specific modules and interfaces.

### Examples (docs/examples/)

Working code samples and workflow templates.

### Build System (docs/make/)

Makefile organization and build system documentation.

### Planning (docs/plans/)

Design documents, architecture decisions, and future plans.

## Documentation Conventions

### File Naming

- **ALL_CAPS.md** - Major documentation files
- **lowercase.md** - Supporting documentation
- **README.md** - Directory index/guide

### Headers

All documentation files include:
```markdown
# Title

© 2025 Casey Koons. All rights reserved.

## Overview
...
```

### Audience Indicators

Documentation clearly indicates intended audience:
- 👤 Users
- 🤖 AIs/CIs
- 🔧 Contributors
- 📚 Examples

## Getting Help

### Users

1. Start with [`guide/README.md`](guide/README.md)
2. Check workflow examples in [`examples/`](examples/)
3. Review troubleshooting in workflow guides

### Developers

1. Read [`guide/CONTRIBUTING.md`](guide/CONTRIBUTING.md)
2. Study [`guide/DAEMON_ARCHITECTURE.md`](guide/DAEMON_ARCHITECTURE.md)
3. Follow patterns in existing code

### AIs/CIs

1. Read [`ai/ONBOARDING.md`](ai/ONBOARDING.md)
2. Study [`guide/CONTRIBUTING.md`](guide/CONTRIBUTING.md)
3. Follow coding standards religiously

## Maintenance

### Adding Documentation

**Before creating new docs:**
1. Check if existing doc can be extended
2. Ensure it fits the directory structure
3. Update this README with new file reference
4. Update relevant directory README

**Documentation Standards:**
- Clear, concise writing
- Examples for complex concepts
- Target specific audience
- Include copyright header
- Keep files focused (one topic)

### Updating Documentation

When updating docs:
- Keep examples current with code
- Update cross-references
- Verify links work
- Note breaking changes prominently

## Project Root Documentation

Important files in project root:
- **CLAUDE.md** - Primary AI instructions for coding with Argo
- **README.md** - Project overview and quick start
- **.env.argo** - Configuration template
- **Makefile** - Build system entry point

## See Also

- **Project README**: [`../README.md`](../README.md)
- **CLAUDE.md**: [`../CLAUDE.md`](../CLAUDE.md) (AI coding standards)
- **arc/CLAUDE.md**: [`../arc/CLAUDE.md`](../arc/CLAUDE.md) (Arc CLI standards)

---

*Well-organized documentation makes Argo accessible to everyone - humans and AIs alike.*
