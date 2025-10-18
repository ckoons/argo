# Arc Helper Functions

Convenience shell functions for workflow template management operations.

## What Are These?

Workflow templates are just directories containing `workflow.sh` files. You can always manage them with standard Unix commands (`rm`, `mv`, `cp`, `$EDITOR`).

These helpers provide:
- **Convenience** - Shorter commands with consistent interface
- **Auto-discovery** - Finds templates in local or global directories
- **Tab completion** - Shell completion for commands and template names
- **Help text** - Built-in documentation

## Installation

### Automatic Installation

```bash
make install-all
```

This installs helpers to `~/.local/share/arc/helpers/` and adds a source line to your shell RC file.

### Manual Installation

```bash
# Copy helpers
mkdir -p ~/.local/share/arc/helpers
cp arc/helpers/arc-helpers.sh ~/.local/share/arc/helpers/

# Add to your shell RC file (~/.bashrc or ~/.zshrc):
source ~/.local/share/arc/helpers/arc-helpers.sh

# Reload shell
exec $SHELL
```

## Usage

### Basic Commands

```bash
# List templates
arc-template ls

# Show template directory path
arc-template dir

# Remove template
arc-template rm old_template

# Rename template
arc-template mv old_name new_name

# Copy template (create from existing)
arc-template cp base_template my_new_template

# Edit template in $EDITOR
arc-template edit my_template

# Show help
arc-template help
```

### Short Alias

For convenience, a short alias `arct` is provided:

```bash
arct ls
arct rm old_template
arct cp base new_template
arct edit my_template
```

### Tab Completion

Tab completion works for both commands and template names (bash only):

```bash
arc-template <TAB>        # Shows: rm, mv, cp, edit, ls, dir, help
arc-template rm <TAB>     # Shows available templates
arc-template edit <TAB>   # Shows available templates
```

## Template Locations

Templates can be in two locations (searched in order):

1. **Project-local:** `./workflows/templates/` (current directory)
2. **User-global:** `~/.argo/workflows/templates/` (home directory)

Helpers automatically detect which location to use (local first, then global).

## Alternative: Standard Unix Commands

You can always use standard Unix commands directly:

```bash
# List templates
ls workflows/templates/
ls ~/.argo/workflows/templates/

# Remove template
rm -rf workflows/templates/my_template

# Rename template
mv workflows/templates/old workflows/templates/new

# Copy template
cp -r workflows/templates/base workflows/templates/new

# Edit template
vim workflows/templates/my_template/workflow.sh
$EDITOR workflows/templates/my_template/workflow.sh
```

## Examples

### Create a new template from existing one

```bash
# Copy base template
arc-template cp create_workflow my_custom_workflow

# Edit the new template
arc-template edit my_custom_workflow

# Test it
arc test my_custom_workflow

# Start workflow from new template
arc start my_custom_workflow instance_name
```

### Clean up old templates

```bash
# List all templates
arc-template ls

# Remove unused templates
arc-template rm old_template_v1
arc-template rm experimental_test
arc-template rm deprecated_workflow
```

### Rename template for clarity

```bash
# Rename template
arc-template mv workflow_test test_workflow

# Note: This doesn't affect already-running workflows
# Running workflows maintain their original names
```

## Customization

Since these are shell functions, you can override them in your shell RC file:

```bash
# Example: Add confirmation prompt to rm
arc-template() {
    if [ "$1" = "rm" ]; then
        echo "Really remove template: $2? (y/N)"
        read -r confirm
        [ "$confirm" = "y" ] || return 1
    fi
    # Call original function
    command arc-template "$@"
}
```

## Philosophy

**Arc manages workflow state. The filesystem manages files.**

These helpers are optional conveniences. The underlying operations are simple filesystem commands. Choose whichever approach you prefer:

- **Helpers:** Consistent interface, auto-discovery, tab completion
- **Raw commands:** Maximum flexibility, wildcards, piping to other tools

Both are equally valid. Use what works best for your workflow.

## See Also

- `arc help` - Arc CLI documentation
- `arc templates` - List available workflow templates
- `arc docs <template>` - Show template documentation
- `arc test <template>` - Run template tests
