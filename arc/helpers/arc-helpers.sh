#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
# Arc Helper Functions for Template Management
#
# These are convenience wrappers for common template operations.
# Templates are just directories - these helpers make common tasks easier.
#
# Installation:
#   Automatically installed by: make install-all
#   Or manually: source arc/helpers/arc-helpers.sh
#
# Add to your ~/.bashrc or ~/.zshrc:
#   source ~/.local/share/arc/helpers/arc-helpers.sh

# Get template directory (check current dir first, then ~/.argo)
_arc_template_dir() {
    if [ -d "workflows/templates" ]; then
        echo "workflows/templates"
    elif [ -d "$HOME/.argo/workflows/templates" ]; then
        echo "$HOME/.argo/workflows/templates"
    else
        echo "Error: No template directory found" >&2
        echo "  Searched: ./workflows/templates, ~/.argo/workflows/templates" >&2
        return 1
    fi
}

# arc-template: Template management helper
arc-template() {
    local cmd=$1
    shift

    local template_dir
    template_dir=$(_arc_template_dir) || return 1

    case "$cmd" in
        rm|remove)
            if [ $# -eq 0 ]; then
                echo "Usage: arc-template rm <template>" >&2
                return 1
            fi
            echo "Removing template: $1"
            rm -rf "$template_dir/$1"
            ;;

        mv|rename)
            if [ $# -ne 2 ]; then
                echo "Usage: arc-template mv <old> <new>" >&2
                return 1
            fi
            echo "Renaming template: $1 → $2"
            mv "$template_dir/$1" "$template_dir/$2"
            ;;

        cp|copy)
            if [ $# -ne 2 ]; then
                echo "Usage: arc-template cp <source> <new>" >&2
                return 1
            fi
            echo "Copying template: $1 → $2"
            cp -r "$template_dir/$1" "$template_dir/$2"
            ;;

        edit)
            if [ $# -eq 0 ]; then
                echo "Usage: arc-template edit <template>" >&2
                return 1
            fi
            local editor="${EDITOR:-vim}"
            "$editor" "$template_dir/$1/workflow.sh"
            ;;

        ls|list)
            arc templates
            ;;

        dir|path)
            echo "$template_dir"
            ;;

        help|--help|-h)
            cat <<'EOF'
Arc Template Management Helpers

Usage: arc-template <command> [args...]

Commands:
  rm <template>          Remove template
  mv <old> <new>         Rename template
  cp <source> <new>      Copy template
  edit <template>        Edit template in $EDITOR
  ls                     List templates (calls: arc templates)
  dir                    Show template directory path
  help                   Show this help

Examples:
  arc-template rm old_template
  arc-template cp base_template my_new_template
  arc-template edit my_template
  arc-template mv old_name new_name

Short alias (if preferred):
  arct rm old_template

These are convenience wrappers for filesystem operations.
You can also use standard Unix commands directly:
  rm -rf workflows/templates/<name>
  mv workflows/templates/<old> workflows/templates/<new>
  cp -r workflows/templates/<src> workflows/templates/<dst>
  $EDITOR workflows/templates/<name>/workflow.sh

Template directories can be in two locations:
  1. ./workflows/templates/           (project-local)
  2. ~/.argo/workflows/templates/     (user-global)

Helpers auto-detect which location to use (local first, then global).

EOF
            ;;

        *)
            echo "Unknown command: $cmd" >&2
            echo "Use: arc-template help" >&2
            return 1
            ;;
    esac
}

# Optional: shorter alias
alias arct='arc-template'

# Tab completion for arc-template (bash only)
if [ -n "$BASH_VERSION" ]; then
    _arc_template_complete() {
        local cur prev
        cur="${COMP_WORDS[COMP_CWORD]}"
        prev="${COMP_WORDS[COMP_CWORD-1]}"

        # Complete commands
        if [ $COMP_CWORD -eq 1 ]; then
            COMPREPLY=($(compgen -W "rm mv cp edit ls dir help" -- "$cur"))
            return 0
        fi

        # Complete template names for commands that need them
        case "$prev" in
            rm|remove|edit|mv|rename|cp|copy)
                local template_dir
                template_dir=$(_arc_template_dir 2>/dev/null) || return 1
                local templates
                templates=$(cd "$template_dir" 2>/dev/null && ls -1d */ 2>/dev/null | sed 's|/||')
                COMPREPLY=($(compgen -W "$templates" -- "$cur"))
                ;;
        esac
    }
    complete -F _arc_template_complete arc-template arct
fi
