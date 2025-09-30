© 2025 Casey Koons All rights reserved

# Argo Scripts

Utility scripts for maintaining and configuring argo.

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