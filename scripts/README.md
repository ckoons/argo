© 2025 Casey Koons All rights reserved

# Argo Scripts

Utility scripts for maintaining and inspecting argo. All utilities are written in C and link against `libargo_core.a` for direct access to argo data structures.

## C Utilities

### argo_monitor

Real-time registry monitoring - displays CI status, ports, and heartbeats.

**Usage:**
```bash
./scripts/argo_monitor              # One-time snapshot
./scripts/argo_monitor --watch      # Continuous refresh
./scripts/argo_monitor --role builder  # Filter by role
./scripts/argo_monitor --json       # JSON output
```

**Features:**
- Lists all registered CIs with status
- Shows port allocations and heartbeat age
- Filters by role
- JSON output for scripting

### argo_memory_inspect

Memory digest inspection - displays memory items, breadcrumbs, and size information.

**Usage:**
```bash
./scripts/argo_memory_inspect                    # Full digest
./scripts/argo_memory_inspect --type FACT        # Filter by type
./scripts/argo_memory_inspect --min-relevance 0.8  # High relevance only
./scripts/argo_memory_inspect --size             # Size info only
./scripts/argo_memory_inspect --json             # JSON output
```

**Features:**
- Shows memory items with relevance scores
- Displays breadcrumbs and sunset/sunrise notes
- Calculates size vs. context limit
- Type filtering (FACT, DECISION, APPROACH, ERROR, SUCCESS, BREADCRUMB, RELATIONSHIP)

### argo_update_models (stub)

Model update utility - will query APIs and generate model defaults.

**Note:** Currently a placeholder. Use `make update-models` (bash script) until full C implementation is complete.

## Build

All C utilities are built automatically with:
```bash
make          # Builds everything including scripts
make scripts  # Builds just the scripts
```

Scripts are compiled to `build/` and symlinked from `scripts/` for easy access.

## Development Scripts

### count_core.sh

Diet-aware line counter for tracking the 10K line budget.

## update_models.sh

Queries API providers and generates `include/argo_local_models.h` with current model defaults.

### Usage

```bash
# From project root
make update-models

# Or directly
./scripts/update_models.sh
```

### What it does

1. Queries each API provider for available models
2. Selects the latest/recommended model for each provider
3. Generates `include/argo_local_models.h` with the results
4. Updates verification date

### Requirements

- API keys must be configured in `include/argo_api_keys.h`
- `curl` and `python3` must be available
- Network access to API providers

### Output

Creates `include/argo_local_models.h` which overrides the built-in defaults in `include/argo_api_providers.h`.

This file is git-ignored, so your model preferences stay local.

### After running

Rebuild argo to use the new models:
```bash
make clean && make
```