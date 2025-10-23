# Argo Workflow Test Framework

© 2025 Casey Koons All rights reserved

## Overview

Comprehensive testing framework for Argo parallel development workflows. Provides unit tests, integration tests, and end-to-end tests for all workflow components.

## Structure

```
tests/
├── test_framework.sh       # Core testing framework
├── mock_ci.sh             # Mock AI provider for testing
├── run_all_tests.sh       # Execute all test suites
├── test_state_manager.sh  # Unit tests for state-manager.sh
├── fixtures/              # Test data and sample files
│   ├── sessions/         # Sample session contexts
│   ├── designs/          # Sample design documents
│   └── builds/           # Sample build artifacts
└── README.md             # This file
```

## Test Framework Features

### Core Functions

**Test Execution:**
- `test_start(name)` - Begin a test case
- `test_pass()` - Mark test as passed
- `test_fail(reason)` - Mark test as failed with reason
- `run_test(function, [name])` - Execute test with setup/teardown
- `test_summary()` - Print results summary

**Assertions:**
- `assert_equals(expected, actual, [msg])` - Compare values
- `assert_not_empty(value, [msg])` - Check non-empty
- `assert_empty(value, [msg])` - Check empty
- `assert_file_exists(path, [msg])` - Verify file exists
- `assert_dir_exists(path, [msg])` - Verify directory exists
- `assert_file_contains(file, string, [msg])` - Check file content
- `assert_success(exit_code, [msg])` - Check success (0)
- `assert_failure(exit_code, [msg])` - Check failure (non-zero)
- `assert_json_equals(file, path, expected, [msg])` - Compare JSON field
- `assert_matches(value, pattern, [msg])` - Regex match

**Test Isolation:**
- `test_setup()` - Create isolated test environment (auto-called)
- `test_teardown()` - Clean up test environment (auto-called)
- Each test gets unique temporary directory (`$TEST_TMP_DIR`)
- Tests cannot interfere with each other

**Mocking:**
- `mock_function(name, return_val, [output])` - Mock bash function
- `mock_command(name, script)` - Mock command in PATH
- `mock_git_repo([dir])` - Create test git repository

**Fixtures:**
- `load_fixture(name, [dir])` - Load test fixture content
- `copy_fixture(name, dest, [dir])` - Copy fixture to location

## Mock CI Tool

The `mock_ci.sh` script simulates AI provider responses for testing without requiring actual AI APIs:

```bash
# Use in tests
export PATH="/path/to/tests:$PATH"
response=$(mock_ci "Generate architecture")
```

**Features:**
- Pattern-based response matching
- Configurable responses via JSON file
- Simulated delays and failures
- Default responses for common prompts

**Response Types:**
- Architecture documents
- Requirements specifications
- Test suites
- Code generation
- Code fixes
- Task decomposition

## Running Tests

### Run All Tests

```bash
./tests/run_all_tests.sh
```

### Run Individual Test Suite

```bash
./tests/test_state_manager.sh
./tests/test_git_helpers.sh
```

### Run Specific Test

```bash
# Modify test file to run only specific tests
bash -c 'source tests/test_framework.sh; source lib/state-manager.sh; run_test test_create_session; test_summary'
```

## Test Suites

### test_state_manager.sh (22 tests, 21 passing)

Tests all state management functions:

**Session Management:**
- Session creation and ID generation
- Session existence checks
- Session directory paths
- Get or create session logic

**Context Management:**
- Save/load full context
- Save/load individual fields
- Phase updates
- Context validation

**Builder State Management:**
- Builder initialization
- Status updates
- State persistence
- Assistance flags
- Builder listing
- Statistics aggregation

**Session Lifecycle:**
- Session archiving
- Session deletion
- Session listing
- Latest session retrieval

**Error Handling:**
- Nonexistent session errors
- Nonexistent builder errors
- Invalid field access

**Known Issues:**
- `test_list_sessions` occasionally fails due to glob pattern edge case (21/22 = 95% pass rate)

## Writing Tests

### Basic Test Structure

```bash
#!/bin/bash
# © 2025 Casey Koons All rights reserved

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="$SCRIPT_DIR/../lib"

# Load test framework
source "$SCRIPT_DIR/test_framework.sh"

# Load module to test
source "$LIB_DIR/your_module.sh"

# Write test functions
test_your_function() {
    local result=$(your_function "arg")
    assert_equals "expected" "$result" "Function output" || return 1
}

# Run tests
main() {
    echo "Running your_module tests..."
    run_test test_your_function
    test_summary
}

main
```

### Test Isolation

Each test runs in an isolated environment:
- Unique temporary directory (`$TEST_TMP_DIR`)
- Isolated sessions directory (`$SESSIONS_DIR`)
- Isolated archives directory (`$ARCHIVES_DIR`)
- Automatic cleanup after test

### Assertions Best Practices

1. **Always provide descriptive messages:**
   ```bash
   assert_equals "expected" "$actual" "User ID should match"
   ```

2. **Check exit codes:**
   ```bash
   your_function "$arg"
   assert_success $? "Function should succeed with valid input"
   ```

3. **Verify file operations:**
   ```bash
   assert_file_exists "$output_file" "Output file should be created"
   assert_file_contains "$output_file" "expected content" "File should contain result"
   ```

4. **Test error cases:**
   ```bash
   your_function "" 2>/dev/null
   assert_failure $? "Function should fail with empty input"
   ```

## Test Fixtures

Located in `fixtures/` directory:

**Sessions:**
- `sample_context.json` - Complete project context

**Designs:**
- `sample_requirements.md` - Sample requirements document
- `sample_architecture.md` - Sample architecture design
- `sample_tasks.json` - Task decomposition example

**Using Fixtures:**

```bash
# Load fixture content
REQUIREMENTS=$(load_fixture "designs/sample_requirements.md")

# Copy fixture to test location
copy_fixture "sessions/sample_context.json" "$TEST_TMP_DIR/context.json"
```

## Coverage

### Current Coverage (Phase 1-4)

✅ **state-manager.sh** - 21/22 tests (95%)
- Session management
- Context management
- Builder state management
- Session lifecycle

⏳ **git-helpers.sh** - Pending
- Git operations
- Branch management
- GitHub integration

⏳ **Integration Tests** - Pending
- Workflow handoffs
- Builder coordination
- End-to-end workflows

### Future Coverage (Phase 5-6)

- Builder failure handling tests
- Merge conflict resolution tests
- GitHub integration tests
- Performance tests
- Load tests

## Continuous Integration

### Pre-commit Tests

```bash
# Run before committing
make test-workflows
```

### Full Test Suite

```bash
# Run all tests
./tests/run_all_tests.sh
```

### Test Exit Codes

- `0` - All tests passed
- `1` - One or more tests failed

## Troubleshooting

### Tests Fail Due to Isolation Issues

**Problem:** Tests interfere with each other

**Solution:** Ensure `test_setup()` creates unique directories:
```bash
TEST_TMP_DIR=$(mktemp -d /tmp/argo_test.XXXXXX)
```

### Mock CI Not Found

**Problem:** `mock_ci` command not in PATH

**Solution:** Export tests directory to PATH:
```bash
export PATH="/path/to/workflows/tests:$PATH"
```

### Fixture Not Found

**Problem:** `load_fixture` can't find file

**Solution:** Use correct relative path from fixtures directory:
```bash
load_fixture "sessions/sample_context.json"  # Correct
load_fixture "sample_context.json"           # Wrong
```

## Contributing Tests

When adding new functionality:

1. **Write tests first** (TDD approach)
2. **Test happy path** and error cases
3. **Use descriptive test names** (`test_function_behavior_condition`)
4. **Keep tests focused** (one assertion per test ideally)
5. **Document known issues** in test comments
6. **Update this README** with new test coverage

## Performance

- State manager tests: ~2-3 seconds
- Mock CI overhead: ~10ms per call
- Typical test: 50-100ms

## Author

Casey Koons - 2025-10-23

## License

© 2025 Casey Koons All rights reserved

---

Part of the Argo parallel development workflow system
