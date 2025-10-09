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
complete -c arc -f -n __fish_use_subcommand -a workflow -d 'Workflow operations'
complete -c arc -f -n __fish_use_subcommand -a list -d 'List active workflows and templates'
complete -c arc -f -n __fish_use_subcommand -a start -d 'Start a new workflow'
complete -c arc -f -n __fish_use_subcommand -a abandon -d 'Terminate a workflow'
complete -c arc -f -n __fish_use_subcommand -a status -d 'Show workflow status'
complete -c arc -f -n __fish_use_subcommand -a pause -d 'Pause a running workflow'
complete -c arc -f -n __fish_use_subcommand -a resume -d 'Resume a paused workflow'
complete -c arc -f -n __fish_use_subcommand -a attach -d 'Attach to workflow output'
complete -c arc -f -n __fish_use_subcommand -a logs -d 'View workflow logs'
complete -c arc -f -n __fish_use_subcommand -a switch -d 'Switch workflow context'
complete -c arc -f -n __fish_use_subcommand -a context -d 'Manage workflow contexts'
complete -c arc -f -n __fish_use_subcommand -a help -d 'Show help information'
complete -c arc -f -n __fish_use_subcommand -a version -d 'Show version information'

# Workflow subcommands
complete -c arc -f -n '__fish_seen_subcommand_from workflow' -a start -d 'Start a new workflow'
complete -c arc -f -n '__fish_seen_subcommand_from workflow' -a abandon -d 'Terminate a workflow'
complete -c arc -f -n '__fish_seen_subcommand_from workflow' -a status -d 'Show workflow status'
complete -c arc -f -n '__fish_seen_subcommand_from workflow' -a pause -d 'Pause a workflow'
complete -c arc -f -n '__fish_seen_subcommand_from workflow' -a resume -d 'Resume a workflow'
complete -c arc -f -n '__fish_seen_subcommand_from workflow' -a attach -d 'Attach to workflow'
complete -c arc -f -n '__fish_seen_subcommand_from workflow' -a logs -d 'View workflow logs'
complete -c arc -f -n '__fish_seen_subcommand_from workflow' -a describe -d 'Describe a workflow'

# Helper function to get template names
function __fish_arc_templates
    arc list 2>/dev/null | awk '
        /TEMPLATES:/ { in_templates=1; next }
        in_templates && NF >= 2 && $1 != "SCOPE" && $1 != "--------" {
            print $2
        }
    '
end

# Helper function to get active workflow IDs
function __fish_arc_workflows
    arc list 2>/dev/null | awk '
        /ACTIVE WORKFLOWS:/ { in_workflows=1; next }
        /TEMPLATES:/ { in_workflows=0 }
        in_workflows && NF >= 2 && $1 != "CONTEXT" && $1 != "--------" {
            print $2
        }
    '
end

# Template name completion for "arc start <template>"
complete -c arc -f -n '__fish_seen_subcommand_from start; and not __fish_seen_subcommand_from workflow' -a '(__fish_arc_templates)'

# Template name completion for "arc workflow start <template>"
complete -c arc -f -n '__fish_seen_subcommand_from workflow; and __fish_seen_subcommand_from start' -a '(__fish_arc_templates)'

# Workflow ID completion for commands that operate on workflows
complete -c arc -f -n '__fish_seen_subcommand_from abandon; and not __fish_seen_subcommand_from workflow' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from status; and not __fish_seen_subcommand_from workflow' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from pause; and not __fish_seen_subcommand_from workflow' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from resume; and not __fish_seen_subcommand_from workflow' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from attach; and not __fish_seen_subcommand_from workflow' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from logs; and not __fish_seen_subcommand_from workflow' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from switch' -a '(__fish_arc_workflows)'

# Workflow ID completion for "arc workflow <cmd> <workflow>"
complete -c arc -f -n '__fish_seen_subcommand_from workflow; and __fish_seen_subcommand_from abandon' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from workflow; and __fish_seen_subcommand_from status' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from workflow; and __fish_seen_subcommand_from pause' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from workflow; and __fish_seen_subcommand_from resume' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from workflow; and __fish_seen_subcommand_from attach' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from workflow; and __fish_seen_subcommand_from logs' -a '(__fish_arc_workflows)'
complete -c arc -f -n '__fish_seen_subcommand_from workflow; and __fish_seen_subcommand_from describe' -a '(__fish_arc_workflows)'
