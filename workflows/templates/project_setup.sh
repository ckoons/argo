#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# project_setup.sh - Initialize .argo-project structure for a new project
#
# Usage: ./project_setup.sh <project_name> [project_path]
#
# Creates directory structure, state.json, config.json, and registers project

# Get script directory (templates directory)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source library modules
source "$SCRIPT_DIR/../lib/state_file.sh"
source "$SCRIPT_DIR/../lib/project_registry.sh"

#
# Main setup function
#
main() {
    local project_name="$1"
    local project_path="${2:-.}"  # Default to current directory

    # Validate arguments
    if [[ -z "$project_name" ]]; then
        echo "ERROR: project_name required" >&2
        echo "Usage: $0 <project_name> [project_path]" >&2
        return 1
    fi

    # Convert to absolute path
    project_path=$(cd "$project_path" && pwd)
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Invalid project path: $project_path" >&2
        return 1
    fi

    cd "$project_path"

    echo "Setting up Argo project: $project_name"
    echo "Location: $project_path"

    # Create directory structure
    echo "Creating directory structure..."
    mkdir -p .argo-project/logs
    mkdir -p .argo-project/checkpoints
    mkdir -p .argo-project/builders

    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to create directory structure" >&2
        return 1
    fi

    # Initialize state.json
    echo "Creating state.json..."
    init_state_file "$project_name" "$project_path"
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to create state.json" >&2
        return 1
    fi

    # Create config.json
    echo "Creating config.json..."
    cat > .argo-project/config.json <<'EOF'
{
  "ci": {
    "tool": "ci",
    "model": null
  },
  "providers": {
    "default": "claude_code",
    "fallback": "claude_api"
  },
  "git": {
    "auto_commit": false,
    "branch_prefix": "argo-builder-"
  },
  "workflow": {
    "auto_checkpoint": true,
    "checkpoint_on_phase_change": true
  }
}
EOF

    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to create config.json" >&2
        return 1
    fi

    # Add .argo-project to .gitignore
    echo "Updating .gitignore..."
    if [[ -f .gitignore ]]; then
        # Check if already present
        if ! grep -q "^.argo-project" .gitignore; then
            echo ".argo-project/" >> .gitignore
        fi
    else
        echo ".argo-project/" > .gitignore
    fi

    # Register project
    echo "Registering project..."
    local project_id=$(register_project "$project_name" "$project_path")
    if [[ $? -ne 0 ]]; then
        echo "WARNING: Failed to register project in global registry" >&2
        # Not fatal - project is still usable
    else
        echo "Project registered with ID: $project_id"
    fi

    echo ""
    echo "Setup complete! Project structure:"
    echo "  .argo-project/"
    echo "  ├── state.json"
    echo "  ├── config.json"
    echo "  ├── logs/"
    echo "  ├── checkpoints/"
    echo "  └── builders/"
    echo ""
    echo "Next steps:"
    echo "  1. Review config.json for your preferences"
    echo "  2. Run your first workflow"
    echo "  3. Check logs in .argo-project/logs/"

    return 0
}

# Run main function
main "$@"
exit $?
