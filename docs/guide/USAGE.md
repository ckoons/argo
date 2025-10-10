# Argo Usage Guide

© 2025 Casey Koons. All rights reserved.

## Overview

This guide covers using the three core Argo executables: `argo-daemon`, `argo_workflow_executor`, and `arc`.

## Architecture

```
arc (terminal client)
    ↓ HTTP REST API
argo-daemon (background service)
    ↓ fork/exec + HTTP progress
argo_workflow_executor (background process per workflow)
    ↓ HTTP I/O channel
AI Providers (Claude, OpenAI, etc.)
```

## Prerequisites

- macOS or Linux
- curl installed (for HTTP communication)
- Workflow templates in `workflows/templates/`

## Quick Start

### 1. Start the Daemon

```bash
# Start daemon on default port (9876)
bin/argo-daemon &

# Or specify port
bin/argo-daemon --port 8080 &

# Or use environment variable
export ARGO_DAEMON_PORT=8080
bin/argo-daemon &
```

**Daemon will:**
- Listen for HTTP requests on specified port
- Fork workflow executors on demand
- Auto-remove completed workflows via SIGCHLD
- Log to stderr (redirect to file if needed)

### 2. Use Arc CLI

```bash
# Start a workflow
arc start simple_test test1

# List workflows
arc list

# Check workflow status
arc status simple_test_test1

# Stop a workflow
arc abandon simple_test_test1
```

## Component Details

### argo-daemon

**Purpose**: Background service that manages workflow execution.

**Usage**:
```bash
argo-daemon [OPTIONS]
```

**Options**:
- `--port PORT` - Listen on PORT (default: 9876 or ARGO_DAEMON_PORT env)
- `--help` - Show help message

**Features**:
- HTTP REST API server on specified port
- Workflow registry and lifecycle management
- Automatic cleanup of completed workflows (SIGCHLD handler)
- Progress tracking via HTTP callbacks
- Interactive workflow I/O via HTTP message passing

**REST API Endpoints**:
```
POST   /api/workflow/start         # Start new workflow
GET    /api/workflow/list          # List all workflows
GET    /api/workflow/status/{id}   # Get workflow status
DELETE /api/workflow/abandon/{id}  # Terminate workflow
POST   /api/workflow/progress/{id} # Progress update (from executor)
POST   /api/workflow/pause/{id}    # Pause workflow (stub)
POST   /api/workflow/resume/{id}   # Resume workflow (stub)
POST   /api/workflow/output/{id}   # Executor writes output
GET    /api/workflow/output/{id}   # Arc reads output
POST   /api/workflow/input/{id}    # Arc writes user input
GET    /api/workflow/input/{id}    # Executor polls for input
GET    /api/health                 # Health check
GET    /api/version                # Daemon version
POST   /api/shutdown               # Shutdown daemon
```

**Log File**:
- Daemon logs to stderr (capture with: `bin/argo-daemon &> daemon.log &`)
- Location: `$HOME/.argo/logs/` (workflow logs)

**Signals**:
- `SIGTERM`, `SIGINT` - Graceful shutdown
- `SIGCHLD` - Auto-remove completed workflows

**Port Conflict Handling**:
- Daemon checks if port is in use on startup
- Sends shutdown request to existing daemon
- Waits 2 seconds for port to free
- Starts new daemon instance

### argo_workflow_executor

**Purpose**: Per-workflow background process that executes workflow steps.

**Usage**:
```bash
argo_workflow_executor <workflow_id> <template_path> <branch>
```

**Arguments**:
- `workflow_id` - Unique identifier (format: template_instance)
- `template_path` - Path to JSON workflow template
- `branch` - Git branch name (for context)

**Example**:
```bash
bin/argo_workflow_executor simple_test_test1 \
    workflows/templates/simple_test.json \
    main
```

**Features**:
- Runs as background process (NO stdin/stdout/stderr to terminal)
- HTTP I/O channel for interactive workflows
- Progress reporting to daemon via HTTP POST
- Signal handling (SIGTERM/SIGINT for graceful stop)
- Safety limits (max steps, max log size)
- Automatic exit on workflow completion

**I/O Model**:
- All output goes to stderr (redirected to log file by daemon)
- User interaction via HTTP I/O channel (not direct terminal)
- Uses `fprintf(stderr, ...)` for all logging

**Safety Features**:
- Maximum step count: 1000 steps
- Maximum log file size: 10 MB
- Infinite loop prevention
- Automatic termination on errors

**Exit Codes**:
- `0` - Workflow completed successfully
- `1` - Workflow failed (step error or validation failure)
- `2` - Stopped by signal (SIGTERM/SIGINT)

**Log File**:
- Location: `$HOME/.argo/logs/<workflow_id>.log`
- Format: Plain text with step execution details
- Preserved after workflow completion

**Note**: This is an internal component. Users should NOT run this directly - use `arc start` instead, which communicates with daemon to spawn executors.

### arc

**Purpose**: Terminal-facing CLI for interacting with Argo daemon.

**Usage**:
```bash
arc <command> [arguments]
```

**Prerequisites**:
- Daemon must be running: `bin/argo-daemon &`
- Default daemon URL: `http://localhost:9876`

**Commands**:

#### arc help [command]
Show help information.

```bash
arc help              # General help
arc help start        # Help for start command
arc help workflow     # Help for workflow commands
```

#### arc workflow start [template] [instance] [branch] [--env env]
Start a new workflow.

```bash
arc start simple_test test1              # Start on main branch
arc start simple_test test2 develop      # Start on develop branch
arc start simple_test test3 --env prod   # Start in prod environment
```

**Workflow ID Format**: `<template>_<instance>`

#### arc workflow list [active|template] [--env env]
List workflows or templates.

```bash
arc list              # List all active workflows and templates
arc list active       # List only active workflows
arc list template     # List only available templates
arc list --env test   # List workflows in test environment
```

#### arc workflow status [workflow_name]
Show workflow status.

```bash
arc status                        # Status of all workflows
arc status simple_test_test1      # Status of specific workflow
```

#### arc workflow states
Show all possible workflow states.

```bash
arc states
```

#### arc workflow attach [workflow_name]
Attach to workflow output (for interactive workflows).

```bash
arc attach simple_test_test1
```

#### arc workflow pause [workflow_name]
Pause workflow at next checkpoint.

```bash
arc pause simple_test_test1
```

**Note**: Currently a stub - returns success but doesn't actually pause.

#### arc workflow resume [workflow_name]
Resume paused workflow.

```bash
arc resume simple_test_test1
```

**Note**: Currently a stub - returns success but doesn't actually resume.

#### arc workflow abandon [workflow_name]
Stop and remove workflow.

```bash
arc abandon simple_test_test1
```

**Behavior**:
- Sends SIGTERM to workflow executor
- Removes workflow from registry
- Preserves log file

#### arc switch [workflow_name]
Set active workflow context for terminal (future feature).

```bash
arc switch simple_test_test1
```

**Environment Filtering**:
- Use `--env <env>` flag on most commands
- Or set `ARC_ENV` environment variable
- Environments: `test`, `dev`, `stage`, `prod` (default: `dev`)

**Shortcuts**:
Arc supports omitting `workflow` for common commands:

```bash
arc start simple_test test1      # Same as: arc workflow start simple_test test1
arc list                          # Same as: arc workflow list
arc status simple_test_test1      # Same as: arc workflow status simple_test_test1
arc abandon simple_test_test1     # Same as: arc workflow abandon simple_test_test1
```

**Exit Codes**:
- `0` - Success
- `1` - User error (bad arguments, workflow not found, etc.)
- `2` - System error (daemon not running, network error, etc.)

**Error Messages**:
Arc provides helpful error messages:

```bash
$ arc start nonexistent test1
Error: Template not found: nonexistent
Try: arc list template

$ arc status foo
Error: Workflow not found: foo
Try: arc list active

$ arc status
Error: Failed to connect to daemon: http://localhost:9876
Make sure daemon is running: argo-daemon
```

## Typical Workflows

### Development Workflow

```bash
# 1. Start daemon (once per development session)
bin/argo-daemon --port 9876 &

# 2. Start a workflow
arc start simple_test dev1

# 3. Monitor progress
arc status simple_test_dev1

# 4. View log file
tail -f ~/.argo/logs/simple_test_dev1.log

# 5. Stop workflow if needed
arc abandon simple_test_dev1

# 6. Shutdown daemon when done
curl -X POST http://localhost:9876/api/shutdown
```

### Testing Workflow

```bash
# Start daemon
bin/argo-daemon --port 9876 &

# Run test workflow
arc start simple_test test1 --env test

# Wait for completion (workflow auto-removes when done)
sleep 2

# Check logs
cat ~/.argo/logs/simple_test_test1.log

# Verify completion
if grep -q "Workflow completed successfully" ~/.argo/logs/simple_test_test1.log; then
    echo "Test passed"
else
    echo "Test failed"
    exit 1
fi
```

### Production Deployment

```bash
# 1. Start daemon with production settings
export ARC_ENV=prod
bin/argo-daemon --port 9876 >> /var/log/argo/daemon.log 2>&1 &

# 2. Start production workflow
arc start deploy release_v1 --env prod

# 3. Monitor via API
curl -s http://localhost:9876/api/workflow/status?workflow_name=deploy_release_v1

# 4. Workflow auto-removes on completion
# Check log for results
cat ~/.argo/logs/deploy_release_v1.log
```

## Debugging

### Daemon Not Starting

```bash
# Check if port is in use
lsof -i :9876

# Kill existing daemon
pkill argo-daemon

# Start with verbose output
bin/argo-daemon --port 9876
```

### Workflow Not Starting

```bash
# Check daemon is running
curl -s http://localhost:9876/api/health

# Check template exists
ls -la workflows/templates/

# Check daemon logs
tail -20 ~/.argo/logs/daemon.log

# Try starting manually (for debugging)
bin/argo_workflow_executor test_debug \
    workflows/templates/simple_test.json \
    main 2>&1 | tee test_debug.log
```

### Workflow Stuck

```bash
# Check status
arc status workflow_name

# Check log file
tail -f ~/.argo/logs/workflow_name.log

# Check executor process
ps aux | grep argo_workflow_executor

# Force kill if needed
pkill -9 -f workflow_name

# Clean up registry
arc abandon workflow_name
```

### Arc Can't Connect

```bash
# Verify daemon is running
ps aux | grep argo-daemon

# Check daemon port
lsof -i :9876

# Test daemon directly
curl -s http://localhost:9876/api/health

# Check environment
echo $ARGO_DAEMON_PORT
```

## Log Files

### Daemon Logs
- Location: stderr (redirect to file: `bin/argo-daemon 2> daemon.log &`)
- Contains: Startup, shutdown, API requests, errors

### Workflow Logs
- Location: `~/.argo/logs/<workflow_id>.log`
- Contains: Step execution, progress, errors, completion status
- Format: Plain text with timestamps
- Preserved after workflow completion

### Log Rotation
Argo does NOT automatically rotate logs. Use system tools:

```bash
# Manual cleanup
find ~/.argo/logs -name "*.log" -mtime +7 -delete

# Or use logrotate
cat > /etc/logrotate.d/argo <<EOF
~/.argo/logs/*.log {
    daily
    rotate 7
    compress
    missingok
    notifempty
}
EOF
```

## Configuration

### Environment Variables

```bash
# Daemon port (default: 9876)
export ARGO_DAEMON_PORT=8080

# Default environment for workflows (default: dev)
export ARC_ENV=prod

# Default AI provider (default: claude_code)
export ARGO_DEFAULT_PROVIDER=claude_api
export ARGO_DEFAULT_MODEL=claude-sonnet-4-5

# API keys (required for API providers)
export ANTHROPIC_API_KEY=sk-ant-api03-...
export OPENAI_API_KEY=sk-proj-...
export GEMINI_API_KEY=...
export GROK_API_KEY=...
export DEEPSEEK_API_KEY=...
export OPENROUTER_API_KEY=...
```

### Configuration Files

**Project Configuration** (`.env.argo`):
```bash
# Committed to git - project defaults
ARGO_DEFAULT_PROVIDER=claude_code
ARGO_DEFAULT_MODEL=claude-sonnet-4-5
```

**Local Configuration** (`.env.argo.local`):
```bash
# NOT committed - developer-specific
ANTHROPIC_API_KEY=sk-ant-api03-...
ARGO_DEFAULT_PROVIDER=ollama  # Local override
```

## Advanced Usage

### Multiple Concurrent Workflows

```bash
# Start multiple workflows
arc start workflow1 instance1 &
arc start workflow1 instance2 &
arc start workflow2 instance1 &

# Monitor all
arc list active

# Check specific workflow
arc status workflow1_instance1
```

### Custom Daemon Ports

```bash
# Development daemon
bin/argo-daemon --port 9876 &

# Testing daemon
bin/argo-daemon --port 9877 &

# Production daemon
bin/argo-daemon --port 9878 &

# Use specific daemon
ARGO_DAEMON_PORT=9877 arc list
```

### Scripting with Arc

```bash
#!/bin/bash
set -e

# Start workflow
WORKFLOW_ID=$(arc start deployment prod_release | grep -o 'deployment_prod_release')

# Wait for completion (poll status)
while arc status "$WORKFLOW_ID" 2>&1 | grep -q "running"; do
    sleep 5
done

# Check result
if grep -q "Workflow completed successfully" ~/.argo/logs/$WORKFLOW_ID.log; then
    echo "Deployment succeeded"
    exit 0
else
    echo "Deployment failed"
    exit 1
fi
```

## See Also

- [Daemon Architecture](DAEMON_ARCHITECTURE.md) - How daemon executes workflows
- [Workflow Construction](WORKFLOW_CONSTRUCTION.md) - Building workflow templates
- [Arc Terminal Guide](ARC_TERMINAL.md) - Complete arc CLI reference
- [CLAUDE.md](../../CLAUDE.md) - Development standards

---

*Simple tools, powerful automation. Start daemon, run workflows, get results.*
