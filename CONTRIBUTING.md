# Contributing to Argo

© 2025 Casey Koons. All rights reserved.

Thank you for your interest in contributing to Argo! This document provides guidelines for contributing code, tests, and documentation.

## Quick Start

1. Read [CLAUDE.md](CLAUDE.md) - our comprehensive programming standards
2. Fork the repository
3. Create a feature branch
4. Make your changes following our standards
5. Run tests: `make clean && make && make test-quick`
6. Submit a pull request

## Code Standards

### File Headers

All files must include the copyright header:
```c
/* © 2025 Casey Koons All rights reserved */
```

### Coding Style

- **Language**: C11 standard
- **Compiler flags**: `-Wall -Werror -Wextra -std=c11`
- **Naming**: snake_case for all functions and variables
- **Line length**: Reasonable (no hard limit, use judgment)
- **Function size**: Keep under 100 lines
- **File size**: Maximum 600 lines per .c file (3% tolerance acceptable)

### Memory Safety

**NEVER use unsafe functions**:
- ❌ `gets`, `strcpy`, `sprintf`, `strcat`
- ✅ Use `strncpy` + null termination, `snprintf`, `strncat`

**String operations must**:
- Always provide size limits
- Explicitly null-terminate
- Use offset tracking for multiple operations

**Example**:
```c
size_t offset = 0;
offset += snprintf(buf + offset, sizeof(buf) - offset, "Part 1: %s\n", part1);
offset += snprintf(buf + offset, sizeof(buf) - offset, "Part 2: %s\n", part2);
```

### Resource Management

**Use goto cleanup pattern for ALL functions that allocate resources**:

```c
int my_function(void) {
    int result = ARGO_SUCCESS;
    char* buffer = NULL;
    FILE* fp = NULL;

    buffer = malloc(SIZE);
    if (!buffer) {
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    fp = fopen(path, "r");
    if (!fp) {
        result = E_SYSTEM_FILE;
        goto cleanup;
    }

    /* ... do work ... */

cleanup:
    free(buffer);
    if (fp) fclose(fp);
    return result;
}
```

### Error Handling

**ONLY use `argo_report_error()` for ALL errors**:
```c
argo_report_error(E_SYSTEM_MEMORY, "function_name", "additional context");
```

**Never use**:
- `fprintf(stderr, ...)` for errors
- `LOG_ERROR()` for errors (it's for informational logging only)

### Constants

**NEVER use numeric constants or string literals in .c files**:
- ❌ Magic numbers in code
- ✅ ALL constants in headers (`argo_limits.h`, module headers)
- ❌ String literals in code (except format strings)
- ✅ ALL strings in headers

**Common constants**:
- `ARGO_PATH_MAX` (512) - for file paths
- `ARGO_DIR_PERMISSIONS` (0755) - directory creation
- `ARGO_FILE_PERMISSIONS` (0644) - file creation
- `ARGO_BUFFER_*` - various buffer sizes

See `include/argo_limits.h` for complete list.

## Testing Requirements

### Before Committing

Run the full test suite:
```bash
make clean && make && make test-quick
```

**All 34 tests must pass** (as of this writing).

### Writing Tests

1. Follow existing test patterns (see `tests/test_*.c`)
2. Test file naming: `test_<module>.c`
3. Use the TEST/PASS/FAIL macros
4. Include positive and negative test cases
5. Test NULL parameter handling
6. Test boundary conditions

**Example test**:
```c
static void test_my_function(void) {
    TEST("My function description");

    int result = my_function(valid_input);
    if (result != ARGO_SUCCESS) {
        FAIL("Function failed with valid input");
        return;
    }

    PASS();
}
```

### Adding Tests to Makefile

1. Add test target variable (e.g., `MY_TEST_TARGET = $(BUILD_DIR)/test_my_module`)
2. Add build rule following existing patterns
3. Add test execution target
4. Add to `test-quick` target

## Line Count Budget

Argo maintains a strict line count budget of **10,000 meaningful lines of code**.

Check current count:
```bash
./scripts/count_core.sh
```

As of this writing: **3,217 / 10,000 lines (32%)**

When adding code:
- Prefer refactoring existing code over adding new code
- Reuse existing utilities
- Keep functions small and focused
- Consider whether the feature is truly needed

## Pre-Commit Checklist

Before committing, verify:

- [ ] Copyright header on all new files
- [ ] No unsafe string functions (strcpy, sprintf, etc.)
- [ ] No magic numbers in .c files
- [ ] No TODO comments (convert to clear notes or implement)
- [ ] goto cleanup pattern used for resource allocations
- [ ] All functions under 100 lines
- [ ] All files under 600 lines (3% tolerance)
- [ ] Compiles with `-Wall -Werror -Wextra`
- [ ] All tests pass (`make test-quick`)
- [ ] No memory leaks (check with valgrind if uncertain)
- [ ] Line count budget respected

## Never Commit

Do not commit code with:
- Magic numbers in .c files
- Unsafe string functions
- TODO comments
- Compiler warnings (`-Werror` prevents this)
- Failing tests
- Memory leaks
- Functions over 100 lines
- Files over 600 lines (except 3% tolerance)
- Resource allocations without goto cleanup

## Pull Request Process

1. **Description**: Clearly describe what and why
2. **Testing**: Show test results in PR description
3. **Line count**: Include before/after line count if significant
4. **Standards**: Confirm adherence to all coding standards
5. **Review**: Address all review comments
6. **Tests**: Ensure CI passes (if applicable)

## Code Review Focus Areas

Reviewers should pay special attention to:

1. **Memory safety**: Proper allocations, frees, no leaks
2. **Error handling**: All error paths properly handled
3. **Resource cleanup**: goto cleanup pattern used correctly
4. **Constants**: No magic numbers or string literals
5. **String operations**: Safe functions, null termination
6. **Testing**: Adequate test coverage
7. **Documentation**: Clear comments for complex logic
8. **Simplicity**: Code is as simple as possible

## Architecture Guidelines

Before adding new features, consider:

1. **Search existing code** for similar patterns
2. **Check utility headers** for existing functions
3. **Reuse or refactor** existing code when possible
4. **Only create new code** when truly necessary

**Remember**: "What you don't build, you don't debug."

## Common Utilities

Before creating new utilities, check:

- `argo_ci_common.h` - Provider macros, inline helpers
- `argo_api_common.h` - HTTP requests, API authentication
- `argo_json.h` - JSON parsing
- `argo_http.h` - HTTP operations
- `argo_socket.h` - Socket configuration
- `argo_error.h` - Error codes
- `argo_limits.h` - All numeric constants
- `argo_file_utils.h` - File operations

## Questions?

- Read [CLAUDE.md](CLAUDE.md) for detailed programming standards
- Check existing code for examples
- Ask questions in issues or discussions

## Philosophy

> **"Simple beats clever. Follow patterns that work."**

Quality over quantity. Code you'd want to debug at 3am. Tests catch bugs, simplicity prevents them.

---

This is a collaboration. We build something maintainable, debuggable, and actually useful.
