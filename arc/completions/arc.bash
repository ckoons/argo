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

    # Main commands
    local commands="workflow list start abandon status pause resume attach logs switch context help version"

    # Workflow subcommands
    local workflow_cmds="start abandon status pause resume attach logs describe"

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
            case "${words[1]}" in
                workflow)
                    # arc workflow <subcommand>
                    COMPREPLY=( $(compgen -W "$workflow_cmds" -- "$cur") )
                    return 0
                    ;;
                start)
                    # arc start <template>
                    COMPREPLY=( $(compgen -W "$(_arc_templates)" -- "$cur") )
                    return 0
                    ;;
                abandon|status|pause|resume|attach|logs)
                    # arc <command> <workflow_id>
                    COMPREPLY=( $(compgen -W "$(_arc_workflows)" -- "$cur") )
                    return 0
                    ;;
                switch)
                    # arc switch <workflow_id>
                    COMPREPLY=( $(compgen -W "$(_arc_workflows)" -- "$cur") )
                    return 0
                    ;;
            esac
            ;;
        3)
            # Third argument - for workflow subcommands
            case "${words[1]}" in
                workflow)
                    case "${words[2]}" in
                        start)
                            COMPREPLY=( $(compgen -W "$(_arc_templates)" -- "$cur") )
                            return 0
                            ;;
                        abandon|status|pause|resume|attach|logs|describe)
                            COMPREPLY=( $(compgen -W "$(_arc_workflows)" -- "$cur") )
                            return 0
                            ;;
                    esac
                    ;;
            esac
            ;;
    esac

    return 0
}

# Get list of active workflow IDs
_arc_workflows() {
    # Try to get from daemon via arc list
    # Parse output for workflow IDs (second column after stripping whitespace)
    # Try different arc paths: installed, local build, or in PATH
    local arc_cmd=""
    if command -v arc >/dev/null 2>&1; then
        arc_cmd="arc"
    elif [ -x "./arc/bin/arc" ]; then
        arc_cmd="./arc/bin/arc"
    elif [ -x "bin/arc" ]; then
        arc_cmd="bin/arc"
    else
        return 1
    fi

    $arc_cmd list 2>/dev/null | awk '
        BEGIN { in_workflows = 0; }
        /ACTIVE WORKFLOWS:/ { in_workflows = 1; next; }
        /TEMPLATES:/ { in_workflows = 0; }
        in_workflows && NF > 0 && $1 != "CONTEXT" && $1 != "--------" {
            # Print second field (workflow name/id)
            if (NF >= 2) print $2;
        }
    '
}

# Get list of available template names
_arc_templates() {
    # Parse arc list output for template names
    # Try different arc paths: installed, local build, or in PATH
    local arc_cmd=""
    if command -v arc >/dev/null 2>&1; then
        arc_cmd="arc"
    elif [ -x "./arc/bin/arc" ]; then
        arc_cmd="./arc/bin/arc"
    elif [ -x "bin/arc" ]; then
        arc_cmd="bin/arc"
    else
        return 1
    fi

    $arc_cmd list 2>/dev/null | awk '
        BEGIN { in_templates = 0; }
        /TEMPLATES:/ { in_templates = 1; next; }
        in_templates && NF > 0 && $1 != "SCOPE" && $1 != "--------" {
            # Print second field (template name)
            if (NF >= 2) print $2;
        }
    '
}

# Register completion function
complete -F _arc_completion arc

# Also register for bin/arc (during development)
complete -F _arc_completion bin/arc
