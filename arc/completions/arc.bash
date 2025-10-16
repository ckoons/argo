#!/bin/bash
# Arc bash completion script
# © 2025 Casey Koons. All rights reserved.
#
# Installation:
#   source arc/completions/arc.bash
#
# Or system-wide:
#   sudo cp arc/completions/arc.bash /usr/local/etc/bash_completion.d/arc
#   # Restart shell

_arc_completion() {
    local cur prev words cword
    _init_completion || return

    # Main commands (all workflow commands work as shortcuts)
    local commands="help start list templates status states attach pause resume abandon test docs"

    # Current word being completed
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Handle different completion contexts
    case "${COMP_CWORD}" in
        1)
            # First argument - complete main commands
            COMPREPLY=( $(compgen -W "$commands" -- "$cur") )
            return 0
            ;;
        2)
            # Second argument - depends on first command
            case "${prev}" in
                start)
                    # arc start <template>
                    COMPREPLY=( $(compgen -W "$(_arc_templates)" -- "$cur") )
                    return 0
                    ;;
                status|attach|pause|resume|abandon)
                    # arc <command> <workflow_id>
                    COMPREPLY=( $(compgen -W "$(_arc_workflows)" -- "$cur") )
                    return 0
                    ;;
                docs|test)
                    # arc docs/test <template>
                    COMPREPLY=( $(compgen -W "$(_arc_templates)" -- "$cur") )
                    return 0
                    ;;
                help)
                    # arc help <command>
                    COMPREPLY=( $(compgen -W "$commands" -- "$cur") )
                    return 0
                    ;;
            esac
            ;;
    esac

    return 0
}

# Get list of active workflow IDs
_arc_workflows() {
    # Try to get from daemon via arc list
    local arc_cmd=""
    if command -v arc >/dev/null 2>&1; then
        arc_cmd="arc"
    elif [ -x "./bin/arc" ]; then
        arc_cmd="./bin/arc"
    elif [ -x "../bin/arc" ]; then
        arc_cmd="../bin/arc"
    else
        return 1
    fi

    # Parse workflow IDs from arc list output
    $arc_cmd list 2>/dev/null | awk '
        /^  wf_/ { print $1; }
    '
}

# Get list of available template names
_arc_templates() {
    # Get templates from arc templates command
    local arc_cmd=""
    if command -v arc >/dev/null 2>&1; then
        arc_cmd="arc"
    elif [ -x "./bin/arc" ]; then
        arc_cmd="./bin/arc"
    elif [ -x "../bin/arc" ]; then
        arc_cmd="../bin/arc"
    else
        return 1
    fi

    # Parse template names from arc templates output
    $arc_cmd templates 2>/dev/null | awk '
        /^  [a-zA-Z]/ && !/^  arc/ { print $1; }
    '
}

# Register completion function
complete -F _arc_completion arc

# Also register for bin/arc (during development)
complete -F _arc_completion bin/arc
