# Argo User Guides

© 2025 Casey Koons. All rights reserved.

## Overview

This directory contains user-facing documentation for Argo - guides, tutorials, and reference materials for working with the multi-AI coordination platform.

**Audience:** 👤 Users (human developers), 🔧 Contributors

## Quick Start Guide

### New to Argo?

**Read these in order:**

1. **[WORKFLOW.md](WORKFLOW.md)** - Understand workflow concepts
2. **[WORKFLOW_CONSTRUCTION.md](WORKFLOW_CONSTRUCTION.md)** - Build your first workflow
3. **[ARC_TERMINAL.md](ARC_TERMINAL.md)** - Use the Arc CLI

### Already Using Argo?

**Jump to what you need:**

- **Building workflows?** → [WORKFLOW_CONSTRUCTION.md](WORKFLOW_CONSTRUCTION.md)
- **Using Arc CLI?** → [ARC_TERMINAL.md](ARC_TERMINAL.md)
- **Contributing code?** → [CONTRIBUTING.md](CONTRIBUTING.md)
- **Understanding internals?** → [DAEMON_ARCHITECTURE.md](DAEMON_ARCHITECTURE.md)

## Documentation Files

### Core User Guides

#### [WORKFLOW_CONSTRUCTION.md](WORKFLOW_CONSTRUCTION.md)
**Complete guide to building Argo workflows**

- Workflow JSON structure
- All step types with examples
- Variable context system
- Branching and control flow
- Best practices
- Debugging workflows

**When to read:** Building or modifying workflows

---

#### [WORKFLOW.md](WORKFLOW.md)
**Workflow concepts and design patterns**

- What workflows are
- When to use workflows vs direct AI calls
- Workflow design philosophy
- Common patterns

**When to read:** Planning workflow architecture

---

#### [ARC_TERMINAL.md](ARC_TERMINAL.md)
**Arc CLI command reference**

- All `arc` commands
- Workflow management (start, attach, abandon)
- Configuration
- Terminal integration

**When to read:** Daily use of Argo via command line

---

### Architecture & Internals

#### [DAEMON_ARCHITECTURE.md](DAEMON_ARCHITECTURE.md)
**How Argo executes workflows**

- Daemon architecture
- HTTP-based message passing
- Workflow executor process model
- REST API endpoints
- Communication patterns

**When to read:** Understanding how Argo works internally, debugging execution issues

**Audience:** Users + Developers

---

#### [INITIALIZATION.md](INITIALIZATION.md)
**Argo startup and configuration**

- Initialization sequence
- Configuration loading
- Environment setup
- Provider initialization

**When to read:** Debugging startup issues, customizing configuration

**Audience:** Developers

---

### Contributing

#### [CONTRIBUTING.md](CONTRIBUTING.md)
**How to contribute code to Argo**

- Code standards (memory safety, error handling, naming)
- Build system (`make` targets)
- Testing requirements
- File organization rules
- Pull request process

**When to read:** Before writing code for Argo

**Audience:** Contributors

---

### Reference

#### [TERMINOLOGY.md](TERMINOLOGY.md)
**Glossary of Argo terms**

- CI (Companion Intelligence)
- Provider
- Workflow vs Template vs Instance
- Registry, Lifecycle, Orchestrator
- Step types

**When to read:** When encountering unfamiliar terms

**Audience:** Everyone

---

## Common Workflows

### I want to...

**...create a simple workflow**
1. Read [WORKFLOW_CONSTRUCTION.md](WORKFLOW_CONSTRUCTION.md) "Quick Start" section
2. Copy a template from `../../workflows/templates/`
3. Modify to your needs
4. Test with `arc start <workflow> <instance>`

**...understand what step type to use**
1. Check [WORKFLOW_CONSTRUCTION.md](WORKFLOW_CONSTRUCTION.md) "Step Types Reference"
2. Look at examples for each step type
3. Review example workflows in `../../workflows/templates/`

**...debug a failing workflow**
1. Check workflow logs: `cat ~/.argo/logs/<workflow_id>.log`
2. Review [WORKFLOW_CONSTRUCTION.md](WORKFLOW_CONSTRUCTION.md) "Debugging Workflows"
3. Verify step JSON syntax matches step type requirements

**...contribute a new feature**
1. Read [CONTRIBUTING.md](CONTRIBUTING.md) completely
2. Study [DAEMON_ARCHITECTURE.md](DAEMON_ARCHITECTURE.md) to understand system
3. Follow coding standards in project root `CLAUDE.md`
4. Write tests for your feature

**...understand how Argo works internally**
1. Start with [DAEMON_ARCHITECTURE.md](DAEMON_ARCHITECTURE.md)
2. Review [INITIALIZATION.md](INITIALIZATION.md) for startup sequence
3. Study code in `src/daemon/` and `src/workflow/`

## Documentation Standards

### What Goes in This Directory

**Include:**
- User-facing guides and tutorials
- Architecture and design documentation
- Contributing guidelines
- Reference materials
- Concept explanations

**Don't Include:**
- AI training materials (goes in `../ai/`)
- API reference (goes in `../api/`)
- Code examples (goes in `../examples/`)
- Planning documents (goes in `../plans/`)

### Writing Style

- **Clear and concise** - Respect reader's time
- **Task-oriented** - Help users accomplish goals
- **Examples-driven** - Show, don't just tell
- **Audience-aware** - Write for specific reader level

### File Organization

- **One topic per file** - Don't mix concepts
- **Cross-reference liberally** - Link to related docs
- **Keep current** - Update when code changes
- **Include copyright** - All files have copyright header

## Getting Help

### For Users

- **Workflow questions**: See [WORKFLOW_CONSTRUCTION.md](WORKFLOW_CONSTRUCTION.md)
- **CLI questions**: See [ARC_TERMINAL.md](ARC_TERMINAL.md)
- **Terminology**: See [TERMINOLOGY.md](TERMINOLOGY.md)

### For Contributors

- **Code standards**: See [CONTRIBUTING.md](CONTRIBUTING.md)
- **Architecture**: See [DAEMON_ARCHITECTURE.md](DAEMON_ARCHITECTURE.md)
- **AI instructions**: See project root `CLAUDE.md`

## Related Documentation

- **Main Docs Index**: [`../README.md`](../README.md)
- **AI Training**: [`../ai/README.md`](../ai/README.md)
- **Project README**: [`../../README.md`](../../README.md)
- **CLAUDE.md**: [`../../CLAUDE.md`](../../CLAUDE.md) (AI coding standards)

---

*These guides make Argo accessible and productive for all users.*
