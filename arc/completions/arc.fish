# © 2025 Casey Koons. All rights reserved.
# Fish shell completion for arc CLI
#
# Installation:
#   mkdir -p ~/.config/fish/completions
#   cp arc.fish ~/.config/fish/completions/
#
# Fish will automatically load completions from this location

# Remove any existing arc completions
complete -c arc -e

# Main commands
complete -c arc -f -n __fish_use_subcommand -a help -d 'Show help information'
complete -c arc -f -n __fish_use_subcommand -a start -d 'Start workflow from template'
complete -c arc -f -n __fish_use_subcommand -a list -d 'List active workflows'
complete -c arc -f -n __fish_use_subcommand -a templates -d 'List available templates'
complete -c arc -f -n __fish_use_subcommand -a status -d 'Show workflow status'
complete -c arc -f -n __fish_use_subcommand -a states -d 'Show all workflow states'
complete -c arc -f -n __fish_use_subcommand -a attach -d 'Attach to workflow logs'
complete -c arc -f -n __fish_use_subcommand -a pause -d 'Pause running workflow'
complete -c arc -f -n __fish_use_subcommand -a resume -d 'Resume paused workflow'
complete -c arc -f -n __fish_use_subcommand -a abandon -d 'Stop and remove workflow'
complete -c arc -f -n __fish_use_subcommand -a test -d 'Run template tests'
complete -c arc -f -n __fish_use_subcommand -a docs -d 'Show template documentation'

# Helper function to get template names
function __fish_arc_templates
    # Try to find arc command
    if command -v arc >/dev/null 2>&1
        set arc_cmd arc
    else if test -x ./bin/arc
        set arc_cmd ./bin/arc
    else if test -x ../bin/arc
        set arc_cmd ../bin/arc
    else
        return 1
    end

    # Parse template names from arc templates output
    $arc_cmd templates 2>/dev/null | awk '
        /^  [a-zA-Z]/ && !/^  arc/ { print $1; }
    '
end

# Helper function to get active workflow IDs
function __fish_arc_workflows
    # Try to find arc command
    if command -v arc >/dev/null 2>&1
        set arc_cmd arc
    else if test -x ./bin/arc
        set arc_cmd ./bin/arc
    else if test -x ../bin/arc
        set arc_cmd ../bin/arc
    else
        return 1
    end

    # Parse workflow IDs from arc list output
    $arc_cmd list 2>/dev/null | awk '
        /^  wf_/ { print $1; }
    '
end

# Template name completion for "arc start <template>"
complete -c arc -f -n '__fish_seen_subcommand_from start' -a '(__fish_arc_templates)'

# Template name completion for "arc docs <template>" and "arc test <template>"
complete -c arc -f -n '__fish_seen_subcommand_from docs' -a '(__fish_arc_templates)'
complete -c arc -f -n '__fish_seen_subcommand_from test' -a '(__fish_arc_templates)'

# Workflow ID completion for commands that operate on workflows
complete -c arc -f -n '__fish_seen_subcommand_from status' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from attach' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from pause' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from resume' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from abandon' -a '(__fish_arc_workflows)'

# Command name completion for "arc help <command>"
complete -c arc -f -n '__fish_seen_subcommand_from help' -a 'help start list templates status states attach pause resume abandon test docs'
