# Arc Test Pyramid

Complete testing infrastructure for the Arc CLI, organized from fast unit tests to comprehensive integration tests.

**IMPORTANT**: All arc tests require the **argo-daemon** to be running. Tests automatically start/stop the daemon using `daemon_helper.sh`.

## Test Layers

### Layer 1: Unit Tests (Fast, Always Run)
**File**: `test_arc_unit.c`
**Command**: `make test-arc-unit`
**Runtime**: < 1 second

Tests arc functionality at the function level:
- Environment utility functions (`arc_get_effective_environment`)
- Registry filtering operations
- Workflow environment tagging
- Persistence with environment field

**When to run**: Every build, part of `make test-quick`

### Layer 2: CLI Tests (Medium, Always Run)
**File**: `test_arc_cli.sh`
**Command**: `make test-arc-cli`
**Runtime**: ~2-3 seconds

Tests command-line interface by executing arc binary:
- Command parsing and help text
- Error handling and user messages
- Environment filtering via `--env` flag and `ARC_ENV`
- Command defaulting (workflow subcommands without prefix)

**Environment**: Uses `ARC_ENV=test` for isolation

**When to run**: Every build, part of `make test-quick`

### Layer 3: Workflow Integration Tests (Heavier, Manual/Nightly)
**File**: `test_arc_workflows.sh`
**Command**: `make test-arc-workflow`
**Runtime**: ~10-15 seconds

Tests complete workflow lifecycle with real workflow execution:
- Starting workflows in test environment
- Workflow listing and status
- Environment filtering with multiple workflows
- Pause/resume/abandon operations
- Registry persistence

**Requires**: Valid workflow template (`simple_test` or similar)

**When to run**: Before releases, nightly builds, manual testing

### Layer 4: Background/Attach Tests (Heaviest, Manual Only)
**File**: `test_arc_background.sh`
**Command**: `make test-arc-background`
**Runtime**: ~15-20 seconds

Tests background process management and attach/detach:
- Background workflow execution
- Process tracking (PID management)
- Switch between workflows
- Attach infrastructure (connection, cursor tracking)
- Concurrent workflow execution
- Log file generation

**Requires**:
- Valid workflow template
- Background process support
- Terminal for attach testing

**When to run**: Manual testing only, before major releases

## Running Tests

### Standard Testing (CI)
```bash
make test-arc              # Unit + CLI tests (fast)
```

### Full Testing (Pre-release)
```bash
make test-arc-full         # Unit + CLI + Workflow tests
```

### Complete Testing (Manual)
```bash
make test-arc-all          # All tests including background
```

### Individual Layers
```bash
make test-arc-unit         # Just unit tests
make test-arc-cli          # Just CLI tests
make test-arc-workflow     # Just workflow tests
make test-arc-background   # Just background tests
```

## Daemon Requirement

All arc tests now communicate with workflows through the **argo-daemon** HTTP API.

### Daemon Helper (`daemon_helper.sh`)

Test scripts use `daemon_helper.sh` to automatically manage the daemon:

**Functions provided:**
- `start_test_daemon()` - Start daemon on port 9876
- `stop_test_daemon()` - Stop daemon gracefully
- `is_daemon_running()` - Check daemon status
- `wait_for_daemon()` - Wait for daemon to be ready
- `daemon_status()` - Show daemon info

**Automatic behavior:**
- Tests start daemon before running
- Tests stop daemon on exit (cleanup)
- Daemon health checked before tests proceed
- Logs written to `/tmp/argo_test_daemon.log`

### Architecture Change

**Before (Direct Access)**:
```
arc commands → registry/orchestrator (direct)
```

**Now (Daemon-Based)**:
```
arc commands → HTTP → argo-daemon → registry/orchestrator
                           ↓
                    workflow_executor
```

All workflow operations (`start`, `status`, `abandon`, etc.) now go through the daemon's REST API.

## Test Environment Isolation

All tests use `ARC_ENV=test` to ensure:
- Test workflows don't pollute production/dev registries
- Clean separation of test data
- Easy cleanup after tests
- Repeatable test runs
- Daemon runs in isolated test mode

## Test Results

Each test suite reports:
- Total tests run
- Tests passed
- Tests failed

Exit codes:
- `0` = All tests passed
- `1` = One or more tests failed

## Adding New Tests

### Unit Tests
Add test functions to `test_arc_unit.c`:
```c
static void test_new_feature(void) {
    printf("Testing: New feature description             ");

    // Test implementation
    if (condition) {
        printf("✓\n");
    } else {
        printf("✗ (error detail)\n");
        return;
    }
}
```

### CLI Tests
Add test cases to `test_arc_cli.sh`:
```bash
test_start "New CLI feature"
output=$($ARC_BIN some command 2>&1 || true)
if output_contains "$output" "expected text"; then
    test_pass
else
    test_fail "Expected 'expected text' in output"
fi
```

### Workflow Tests
Add test cases to `test_arc_workflows.sh` following the same pattern.

### Background Tests
Add test cases to `test_arc_background.sh` following the same pattern.

## Makefile Integration

Arc tests are integrated into the main argo Makefile:

```bash
# From main argo directory
make test-quick            # Includes test-arc (unit + CLI)
make test-arc-workflow     # Workflow integration
make test-arc-full         # Full arc testing
```

## Test Pyramid Philosophy

```
                    /\
                   /  \      Background/Attach Tests (Manual)
                  /    \     - Slowest, most comprehensive
                 /------\    - Real process management
                /        \
               /  Workflow \
              /  Integration\ Workflow Tests (Nightly)
             /--------------\ - Real workflow execution
            /                \
           /   CLI Tests       \ CLI Tests (Always)
          /--------------------\ - Command parsing
         /                      \
        /      Unit Tests         \ Unit Tests (Always)
       /--------------------------\ - Fast, isolated
```

**Build from bottom up**: Each layer builds on the one below.
**Run from top down**: Higher layers catch integration issues the lower layers miss.

## The Pyramid is Built! 🏛️

*Like Khufu's Great Pyramid, built to last the ages.*
