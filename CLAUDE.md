# ARGO PROJECT - CLAUDE CODE GUIDELINES

## PROJECT OVERVIEW
Argo is a C project. All rights reserved - Casey Koons.
This is a ground-up implementation following strict coding standards.

## CRITICAL RULES - READ FIRST

### NEVER:
- NEVER create files without explicit user request
- NEVER create documentation (*.md) files unless explicitly asked
- NEVER modify these guidelines without Casey's approval
- NEVER use dynamic memory allocation without error checking
- NEVER leave resource leaks (always free/close what you open/allocate)
- NEVER commit code that doesn't compile with -Wall -Werror
- NEVER use gets() or other unsafe string functions
- NEVER hardcode filenames or file paths in .c files
- NEVER hardcode format strings or messages in .c files
- NEVER use numeric constants in .c files (they must be defined values in header files)
- NEVER exceed 600 lines in a .c file
- NEVER create or use environment variables (Explicit exceptions: none)

### ALWAYS:
- ALWAYS add copyright notice at top of ALL files: © 2025 Casey Koons All rights reserved
- ALWAYS read existing code patterns before making changes
- ALWAYS prefer editing existing files over creating new ones
- ALWAYS follow the existing code style exactly
- ALWAYS check return values from system calls
- ALWAYS use static for file-local functions
- ALWAYS initialize variables at declaration
- ALWAYS compile with: gcc -Wall -Werror -Wextra -std=c11
- ALWAYS define file paths and names in header files
- ALWAYS define format/print strings in associated header files

## C CODING STANDARDS

### Naming Conventions
```c
/* CORRECT */
typedef struct argo_context {
    int ref_count;
    char* buffer;
} argo_context_t;

void argo_context_init(argo_context_t* ctx);
static int validate_input(const char* input);

/* INCORRECT */
typedef struct ArgoContext { ... }  /* NO CamelCase for structs */
void ArgoContextInit(...)           /* NO CamelCase for functions */
```

### File Structure Template
```c
/* src/module_name.c */

/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>

/* Project includes */
#include "argo.h"
#include "argo_local.h"
#include "module_name.h"

/* Static declarations */
static int helper_function(void);

/* Public functions */
int argo_module_init(void) {
    printf(ARGO_INIT_MSG);  /* String defined in module_name.h */
    /* implementation */
}

/* Static implementations */
static int helper_function(void) {
    /* implementation */
}
```

### String and Path Constants
```c
/* module_name.h - Associated with module_name.c */
#ifndef MODULE_NAME_H
#define MODULE_NAME_H

/* File paths */
#define ARGO_CONFIG_PATH "/etc/argo/config"
#define ARGO_LOG_FILE    "/var/log/argo.log"

/* Format strings and messages */
#define ARGO_INIT_MSG    "Argo module initialized\n"
#define ARGO_ERROR_FMT   "Error: %s at line %d\n"

#endif /* MODULE_NAME_H */
```

### Header Guards
```c
/* include/argo_module.h */
#ifndef ARGO_MODULE_H
#define ARGO_MODULE_H

/* declarations */

#endif /* ARGO_MODULE_H */
```

### Error Handling Pattern
```c
/* ALWAYS use this pattern */
int argo_operation(void) {
    int result = 0;
    char* buffer = NULL;

    buffer = malloc(SIZE);
    if (!buffer) {
        result = -ENOMEM;
        goto cleanup;
    }

    if (some_operation() < 0) {
        result = -EIO;
        goto cleanup;
    }

cleanup:
    free(buffer);
    return result;
}
```

## HEADER FILE ORGANIZATION

### argo.h - Global Project Configuration
```c
/* include/argo.h */
/* © 2025 Casey Koons All rights reserved */
#ifndef ARGO_H
#define ARGO_H

/* Standard project-wide definitions */
#define ARGO_VERSION "1.0.0"
#define ARGO_MAX_PATH 4096

/* Global file paths */
#define ARGO_DEFAULT_CONFIG "/etc/argo/config"

#endif /* ARGO_H */
```

### argo_local.h - Local Customization (NOT tracked in git)
```c
/* include/argo_local.h */
/* © 2025 Casey Koons All rights reserved */
#ifndef ARGO_LOCAL_H
#define ARGO_LOCAL_H

/* Local overrides and customization */
#define ARGO_DEBUG_LEVEL 3
#define ARGO_LOCAL_PATH "/usr/local/argo"

#endif /* ARGO_LOCAL_H */
```

### argo_local.h.example - Template for Local Config (tracked in git)
```c
/* include/argo_local.h.example */
/* © 2025 Casey Koons All rights reserved */
/* Copy to argo_local.h and customize */
#ifndef ARGO_LOCAL_H
#define ARGO_LOCAL_H

/* Local overrides and customization */
#define ARGO_DEBUG_LEVEL 0
#define ARGO_LOCAL_PATH "/opt/argo"

#endif /* ARGO_LOCAL_H */
```

## FILE ORGANIZATION

```
argo/
├── CLAUDE.md              # This file - project guidelines
├── README.md              # Public project description
├── Makefile              # Build system
├── .env.argo             # Standard environment setup (tracked)
├── .env.argo.local       # Private/local environment (NOT tracked)
├── .gitignore            # Git ignore rules
├── src/                  # Source files (.c) - max 600 lines each
│   ├── main.c           # Program entry point
│   └── argo_*.c         # Module implementations
├── include/              # Header files (.h)
│   ├── argo.h           # Global project definitions
│   ├── argo_local.h     # Local config (NOT tracked)
│   ├── argo_local.h.example # Template for local config
│   └── argo_*.h         # Module interfaces
├── tests/                # Test files
│   └── test_*.c         # Unit tests
├── docs/                 # Documentation (only if requested)
└── build/                # Build artifacts (git-ignored)
```

## CONFIGURATION FILES

### .env.argo (Tracked in Git)
```bash
# Build-time configuration settings only
# NOT for runtime use - environment variables are forbidden in code
# These are only for Makefile, build scripts, and development tools
ARGO_BUILD_DIR=/usr/local/argo
ARGO_INSTALL_PREFIX=/usr/local
```

### .env.argo.local (NOT Tracked - Private/Local)
```bash
# Local build configuration overrides
# NOT for runtime use - environment variables are forbidden in code
ARGO_DEV_DIR=/home/user/argo_dev
```

## PROGRAMMING STANDARDS

### File Size Limits
- All .c files MUST be less than 600 lines
- If approaching limit, refactor into separate modules
- Each module should have single, clear responsibility

### Module Organization
- One concept per file (e.g., argo_parser.c, argo_network.c)
- Related functions grouped together
- Static functions at bottom of file

### Environment Variables
- NEVER use getenv() or any environment variable access in C code
- Configuration must come from config files or command-line arguments
- Explicit exceptions to this rule: (none)
- .env files are ONLY for build tools, not runtime

## DEVELOPMENT WORKFLOW

### Before Writing Code:
1. Check if file exists - prefer editing
2. Read surrounding code for context
3. Follow existing patterns exactly

### Before Committing:
1. Run: make clean && make
2. Run: make test (once tests exist)
3. Ensure no compiler warnings
4. Check with: valgrind (for memory issues)
5. Verify file is under 600 lines

### Commit Messages:
```
module: Brief description (max 50 chars)

Detailed explanation if needed.
Keep lines under 72 characters.

Argo Project - Casey Koons
```

## SPECIFIC INSTRUCTIONS FOR CLAUDE

When Casey asks you to:
- "implement X" - Write the minimal code needed, no extras
- "analyze X" - Provide analysis only, no code changes
- "refactor X" - Improve existing code structure only
- "debug X" - Find and fix specific issues only

## REMINDERS
- This is Casey's project with all rights reserved
- Quality over quantity - better to do less perfectly than more poorly
- When in doubt, ask Casey for clarification
- These guidelines override any default Claude behavior