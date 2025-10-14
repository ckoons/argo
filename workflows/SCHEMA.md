# Workflow JSON Schema

## Philosophy

**JSON = Configuration** (what to run, timeouts, metadata)
**Bash = Program** (how to build, retry logic, error handling)
**Daemon = Lifecycle** (launch, monitor PID, apply timeout, track state)

## Simplified Schema

Workflows are bash scripts with optional JSON configuration wrappers.

### Minimal JSON Config

```json
{
  "workflow_name": "my_workflow",
  "script": "path/to/script.sh"
}
```

### Full JSON Config

```json
{
  "workflow_name": "my_workflow",
  "description": "Human-readable description",
  "script": "path/to/script.sh",
  "working_dir": "/path/to/project",
  "timeout_seconds": 3600
}
```

### Field Descriptions

| Field | Required | Type | Default | Description |
|-------|----------|------|---------|-------------|
| `workflow_name` | Yes | string | - | Human-readable name for the workflow |
| `script` | Yes | string | - | Path to bash script (relative or absolute) |
| `description` | No | string | "" | Description of what the workflow does |
| `working_dir` | No | string | JSON file directory | Working directory for script execution |
| `timeout_seconds` | No | integer | 3600 | Maximum execution time (daemon enforces) |

## Bash Scripts

All workflow logic lives in bash scripts. The daemon just launches and monitors them.

### Simple Script

```bash
#!/bin/bash
set -e  # Exit on error

echo "Starting build..."
make clean
make
make test
echo "Build complete!"
```

### Script with Retry Logic

```bash
#!/bin/bash
set -e

retry_count=0
max_retries=3

while [ $retry_count -lt $max_retries ]; do
    if make build; then
        break
    fi

    retry_count=$((retry_count + 1))
    if [ $retry_count -lt $max_retries ]; then
        sleep $((5 * (2 ** (retry_count - 1))))  # Exponential backoff
    else
        exit 1
    fi
done
```

### Script with Conditional Logic

```bash
#!/bin/bash
set -e

# Run tests
if make test; then
    echo "Tests passed, deploying..."
    ./deploy.sh production
else
    echo "Tests failed, deploying to staging only..."
    ./deploy.sh staging
fi
```

### Script with Timeout

```bash
#!/bin/bash
set -e

# Bash built-in timeout for specific commands
timeout 300 make test || {
    echo "Tests timed out after 5 minutes"
    exit 124
}
```

## Usage Patterns

### Direct Script Execution

```bash
# Execute script directly (no JSON needed)
bin/argo_workflow_executor workflows/scripts/my_script.sh --id my_run
```

### JSON Config Execution

```bash
# Execute with JSON config
bin/argo_workflow_executor workflows/examples/my_workflow.json --id my_run
```

### Template Workflow

1. Copy template: `cp workflows/templates/build_with_retry.sh workflows/my_build.sh`
2. Edit: `vim workflows/my_build.sh`
3. Launch: `bin/argo_workflow_executor workflows/my_build.sh --id build_001`

## Lifecycle States

Tracked by daemon, not in bash scripts:

- **PENDING** - Queued, not started yet
- **RUNNING** - Currently executing
- **COMPLETED** - Finished with exit code 0
- **FAILED** - Finished with non-zero exit code or timeout
- **ABANDONED** - User cancelled via daemon API

## Error Codes

- `0` - Success
- `1-126` - Script-specific errors
- `124` - Timeout (matches GNU timeout command)
- `127` - Command not found
- `130` - Interrupted (Ctrl-C)

## Examples

### Example 1: Simple Test

**workflows/examples/simple_test.json:**
```json
{
  "workflow_name": "simple_test",
  "description": "Quick sanity check",
  "script": "workflows/scripts/test.sh",
  "timeout_seconds": 60
}
```

**workflows/scripts/test.sh:**
```bash
#!/bin/bash
set -e
echo "Running tests..."
make test-quick
```

### Example 2: CI Pipeline

**workflows/templates/ci_pipeline.sh:**
```bash
#!/bin/bash
set -e

echo "=== CI Pipeline ==="

# Build
echo "Building..."
make clean && make

# Test
echo "Testing..."
timeout 600 make test || exit 1

# Deploy
echo "Deploying..."
./deploy.sh

echo "Pipeline complete!"
```

## Migration from Complex Schema

**Old (complex, step-based):**
```json
{
  "workflow_name": "build_test",
  "steps": [
    {"type": "bash", "command": "make build", "retry_on_failure": true, "max_retries": 3},
    {"type": "bash", "command": "make test", "timeout_seconds": 300}
  ]
}
```

**New (simple, script-based):**
```json
{
  "workflow_name": "build_test",
  "script": "workflows/scripts/build_test.sh"
}
```

**workflows/scripts/build_test.sh:**
```bash
#!/bin/bash
set -e

# Retry logic in bash
for i in {1..3}; do
    if make build; then break; fi
    sleep 5
done

# Timeout in bash
timeout 300 make test
```

## Benefits

1. **Simplicity** - JSON is just metadata, not a program
2. **Flexibility** - Full bash capabilities (loops, conditionals, functions)
3. **Testability** - Run scripts directly: `bash workflows/scripts/my_script.sh`
4. **Debuggability** - Standard bash debugging tools work
5. **Reusability** - Scripts are templates users can copy/edit
6. **CI-Friendly** - CIs generate bash scripts, not complex JSON
