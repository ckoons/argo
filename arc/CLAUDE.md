# Building Arc CLI with Claude

© 2025 Casey Koons. All rights reserved.

## What Arc Is

Arc is the command-line interface for Argo, managing AI interactions and workflows. Like Tekton's `aish`, arc provides a Unix-friendly way to interact with the Argo multi-AI coordination system.

**Budget: 5,000 lines** (src/ only)

## Inherits Core Standards

Arc follows all standards from `/CLAUDE.md`:
- Memory management (check returns, free everything)
- Error reporting (use `argo_report_error()` exclusively)
- No magic numbers (all constants in headers)
- File size limits (max 600 lines)
- Code organization (one purpose per module)

## Arc-Specific Principles

### User Experience First
- **Clear output** - Users should immediately understand what happened
- **Helpful errors** - Guide users to solutions, not just problems
- **Progress feedback** - Long operations show progress
- **Sensible defaults** - Works out of the box, customizable when needed

### Unix Philosophy
- **Do one thing well** - Each command has a clear purpose
- **Composable** - Output works as input to other tools
- **Silent on success** - Only speak when there's something to say
- **Exit codes matter** - 0=success, 1=user error, 2=system error

### Command Design
```c
/* Good command structure */
int cmd_workflow_run(int argc, char** argv) {
    /* Parse args */
    /* Validate inputs */
    /* Execute with progress */
    /* Report results */
    /* Return exit code */
}
```

## Code Patterns

### Argument Parsing
```c
/* Use getopt for consistency */
#include <getopt.h>

static struct option long_options[] = {
    {"verbose", no_argument, 0, 'v'},
    {"output", required_argument, 0, 'o'},
    {0, 0, 0, 0}
};

int c;
while ((c = getopt_long(argc, argv, "vo:", long_options, NULL)) != -1) {
    switch (c) {
        case 'v': verbose = 1; break;
        case 'o': output_file = optarg; break;
    }
}
```

### Progress Display
```c
/* For long operations */
void show_progress(int current, int total) {
    int percent = (current * 100) / total;
    fprintf(stderr, "\rProgress: %d%% [%d/%d]", percent, current, total);
    if (current == total) fprintf(stderr, "\n");
}
```

### Error Messages
```c
/* User errors (exit 1) */
fprintf(stderr, "Error: workflow not found: '%s'\n", workflow_id);
fprintf(stderr, "Try: arc workflow list\n");
return 1;

/* System errors (exit 2) */
argo_report_error(E_SYSTEM_MEMORY, "cmd_workflow_run", "allocation failed");
return 2;
```

## Command Structure

```
arc/
├── src/
│   ├── main.c              # Entry point, dispatch commands
│   ├── cmd_workflow.c      # Workflow commands
│   ├── cmd_ci.c           # CI management commands
│   ├── cmd_config.c       # Configuration commands
│   └── utils.c            # Shared utilities
├── include/
│   ├── arc_commands.h     # Command definitions
│   └── arc_utils.h        # Utility functions
└── tests/
    └── test_commands.c    # Command tests
```

## Command Naming

```bash
# Verb-noun pattern
arc workflow run <workflow>
arc workflow list
arc ci start <name>
arc ci status
arc config get <key>
arc config set <key> <value>
```

## Output Formatting

### Human-Readable (default)
```c
printf("Workflow: %s\n", name);
printf("Status:   %s\n", status);
printf("Progress: %d/%d tasks\n", completed, total);
```

### JSON Mode (`--json`)
```c
if (json_mode) {
    printf("{\"name\":\"%s\",\"status\":\"%s\"}\n", name, status);
} else {
    printf("Workflow: %s (status: %s)\n", name, status);
}
```

## Testing Commands

```c
/* Test with mock Argo core */
static ci_registry_t* mock_registry;
static workflow_controller_t* mock_workflow;

void setup_test(void) {
    argo_init();
    mock_registry = registry_create();
    /* ... */
}

void test_cmd_workflow_run(void) {
    char* argv[] = {"arc", "workflow", "run", "test.json"};
    int result = cmd_workflow_run(4, argv);
    assert(result == 0);
}
```

## Configuration Files

Arc reads configuration from:
1. `~/.argorc` - User preferences
2. `./.arc/config` - Project-specific settings

```c
/* Format: KEY=VALUE */
API_KEY=xxx
DEFAULT_MODEL=claude
VERBOSE=1
```

## Reminders

- Commands should be **fast** - under 100ms startup
- Help text is **documentation** - keep it accurate
- Tab completion friendly - predictable naming
- Color output only if **stdout is a TTY**
- Respect **standard env vars** (NO_COLOR, TERM, etc.)

---

*Arc makes Argo accessible. Simple, fast, Unix-friendly.*
