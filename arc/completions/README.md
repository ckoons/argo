# Arc Shell Completions

© 2025 Casey Koons. All rights reserved.

## Automatic Installation

Run `make install-all` or `make install-completion` from the project root, and completion will be automatically installed for your shell (bash, zsh, or fish).

## Manual Installation

### Bash

```bash
# Option 1: Source in current session
source arc/completions/arc.bash

# Option 2: Add to ~/.bashrc (permanent)
echo "source ~/projects/github/argo/arc/completions/arc.bash" >> ~/.bashrc

# Option 3: System-wide (requires sudo)
sudo cp arc/completions/arc.bash /usr/local/etc/bash_completion.d/arc
```

### Zsh

```bash
# Option 1: Manual install
mkdir -p ~/.zsh/completions
cp arc/completions/_arc ~/.zsh/completions/

# Add to ~/.zshrc:
fpath=(~/.zsh/completions $fpath)
autoload -U compinit && compinit

# Restart shell
exec zsh
```

### Fish

```bash
# Install
mkdir -p ~/.config/fish/completions
cp arc/completions/arc.fish ~/.config/fish/completions/

# Fish will automatically load completions on next shell start
```

## What Gets Completed

### Commands
- `arc st<TAB>` → expands to `start` or `status`
- `arc abandon<TAB>` → shows all workflow subcommands

### Template Names
- `arc start si<TAB>` → expands to `simple_` prefix
- `arc start simple_t<TAB>` → expands to `simple_test`

### Workflow IDs
- `arc abandon <TAB>` → shows all active workflows
- `arc status sim<TAB>` → expands to matching workflow IDs

## Testing

Run the test script to verify completions work:

```bash
cd arc/completions
./test_completion.sh
```

## Supported Shells

- ✅ Bash (4.0+)
- ✅ Zsh (5.0+)
- ✅ Fish (3.0+)

## How It Works

All completion scripts call `arc list` to get real-time data:
- Template names from the `TEMPLATES:` section
- Active workflow IDs from the `ACTIVE WORKFLOWS:` section

This ensures completions are always up-to-date with no caching issues.
