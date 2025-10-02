# Argo Initialization

© 2025 Casey Koons All rights reserved

## Overview

Every Argo application must initialize the library before using any Argo functions, and cleanup when done. This document describes the initialization lifecycle and proper usage patterns.

## Basic Usage

```c
#include "argo_init.h"

int main(void) {
    /* Initialize Argo */
    if (argo_init() != ARGO_SUCCESS) {
        return 1;  /* Init handles its own cleanup on failure */
    }

    /* Application work using Argo APIs */
    run_application();

    /* Cleanup */
    argo_exit();
    return 0;
}
```

## Initialization Sequence

`argo_init()` performs the following steps:

### 1. Environment Loading (`argo_loadenv`)
- Loads system environment variables
- Searches for `.env.argo` (upward from cwd or from `$ARGO_ROOT`)
- Loads `~/.env` (optional)
- Loads `.env.argo` (required)
- Loads `.env.argo.local` (optional)
- Expands `${VAR}` references
- Sets `ARGO_ROOT` environment variable

### 2. Configuration (`argo_config`)
- Future: Creates `.argo/` directory structure
- Future: Loads `.argo/config.json`
- Currently: Stub (returns success)

## Cleanup Sequence

`argo_exit()` performs cleanup in reverse order:

### 1. Configuration Cleanup
- Frees configuration structures
- Currently: Stub

### 2. Environment Cleanup
- Frees all environment variables
- Resets environment state

## Important Notes

### What argo_init() Does NOT Do

- **Does not** change current working directory
- **Does not** start socket servers
- **Does not** create provider instances
- **Does not** initialize registries

These are application responsibilities.

### Failure Handling

If `argo_init()` fails at any step, it automatically cleans up any previously initialized subsystems before returning. The caller can simply check the return value and exit:

```c
if (argo_init() != ARGO_SUCCESS) {
    return 1;  /* Already cleaned up */
}
```

### Re-initialization

You can call `argo_init()` multiple times (e.g., to reload configuration):

```c
argo_init();
/* ... work ... */
argo_exit();

/* Later... */
argo_init();  /* Fresh start */
/* ... more work ... */
argo_exit();
```

### Idempotent Cleanup

`argo_exit()` is safe to call multiple times and safe to call even if `argo_init()` failed.

## Global State

All global state is tracked in `include/argo_globals.h`. Subsystems that allocate global resources provide cleanup functions.

Current global state:
- Environment variables (`argo_env`, cleaned by `argo_freeenv()`)
- Configuration (future, cleaned by `argo_config_cleanup()`)

## Application-Managed Resources

The following must be cleaned up by the application:

```c
int main(void) {
    if (argo_init() != ARGO_SUCCESS) return 1;

    /* Application creates these */
    socket_server_init("my-ci");
    ci_registry_t* registry = registry_create();

    /* Application work */
    run_application();

    /* Application cleans up its resources */
    socket_server_cleanup();
    registry_destroy(registry);

    /* Finally, cleanup library */
    argo_exit();
    return 0;
}
```

## Test Harnesses

Test harnesses (not counted in diet) use `argo_init()`/`argo_exit()` as their foundation. Each harness implements its own `run_*()` function:

### Terminal Harness
```c
int main(void) {
    if (argo_init() != ARGO_SUCCESS) return 1;
    run_terminal_interface();  /* Interactive CLI */
    argo_exit();
    return 0;
}
```

### Service Harness
```c
int main(void) {
    if (argo_init() != ARGO_SUCCESS) return 1;
    run_daemon_loop();  /* Background service */
    argo_exit();
    return 0;
}
```

### Workflow Harness
```c
int main(void) {
    if (argo_init() != ARGO_SUCCESS) return 1;
    run_workflow("test.yaml");  /* Execute workflow */
    argo_exit();
    return 0;
}
```

## See Also

- `include/argo_init.h` - Public API
- `include/argo_globals.h` - Global state registry
- `include/argo_config.h` - Configuration subsystem
- `include/argo_env_utils.h` - Environment subsystem
