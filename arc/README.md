# Arc - Argo Command Line Interface

© 2025 Casey Koons. All rights reserved.

**Unix-friendly CLI for multi-AI workflow coordination**

## What is Arc?

Arc is the command-line interface for Argo, providing a simple way to manage AI workflows, coordinate multiple companion intelligences (CIs), and monitor complex development tasks - all from your terminal.

Like Tekton's `aish`, arc brings the power of multi-AI coordination to the command line with Unix philosophy: composable, scriptable, and straightforward.

## Quick Start

```bash
# Build arc (requires argo core)
cd arc
make

# Run a workflow
../build/arc workflow run workflows/build/test/unit_tests.json

# List active CIs
../build/arc ci list

# Check workflow status
../build/arc workflow status <workflow-id>
```

## Commands

### Workflows
```bash
arc workflow list              # List available workflows
arc workflow run <file>        # Execute a workflow
arc workflow status <id>       # Check workflow status
arc workflow pause <id>        # Pause running workflow
arc workflow resume <id>       # Resume paused workflow
```

### CI Management
```bash
arc ci list                    # List registered CIs
arc ci start <name> <role>     # Start a new CI
arc ci stop <name>             # Stop a CI
arc ci status                  # Show CI health summary
```

### Configuration
```bash
arc config get <key>           # Get config value
arc config set <key> <value>   # Set config value
arc config list                # List all settings
```

## Configuration

Arc reads configuration from:
- `~/.argorc` - User-level preferences
- `./.arc/config` - Project-specific settings

**Example ~/.argorc:**
```bash
# API Keys
ANTHROPIC_API_KEY=sk-xxx
OPENAI_API_KEY=sk-xxx

# Defaults
DEFAULT_MODEL=claude-3-sonnet
VERBOSE=0
```

## Output Modes

### Human-Readable (default)
```bash
$ arc workflow status dev-123
Workflow: dev-123
Status:   running
Phase:    DEVELOP
Progress: 3/5 tasks completed
```

### JSON Mode
```bash
$ arc workflow status dev-123 --json
{"id":"dev-123","status":"running","phase":"DEVELOP","progress":{"completed":3,"total":5}}
```

## Integration

### Shell Scripts
```bash
#!/bin/bash
# Run workflow and check result
if arc workflow run build.json --quiet; then
    echo "Build successful"
    arc ci stop builder
else
    echo "Build failed"
    exit 1
fi
```

### CI/CD Pipelines
```yaml
# GitHub Actions example
- name: Run Argo Workflow
  run: |
    arc workflow run .github/workflows/test.json
    arc workflow status ${{ env.WORKFLOW_ID }} --json > results.json
```

## Exit Codes

- **0** - Success
- **1** - User error (invalid arguments, workflow not found)
- **2** - System error (Argo initialization failed, resource unavailable)

## Dependencies

- Argo core library (libargo_core.a)
- POSIX system (Unix, Linux, macOS)
- C11 compiler

## Building

```bash
# From arc directory
make              # Build arc CLI
make test         # Run tests
make clean        # Clean build artifacts
make count        # Check code size (5000 line budget)
```

## Development

See [CLAUDE.md](CLAUDE.md) for coding standards and patterns specific to arc development.

**Budget:** 5,000 lines (src/ only)

---

*Arc: The command-line gateway to multi-AI workflows*
