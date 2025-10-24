#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# context-manager.sh - Manage context.json for workflow phases
#
# Provides functions for creating, updating, and querying context.json

# Create initial context.json structure
# Args: session_id, project_name
# Returns: Path to created context.json
create_initial_context() {
    local session_id="$1"
    local project_name="$2"
    local session_dir=$(get_session_dir "$session_id")
    local context_file="$session_dir/context.json"

    cat > "$context_file" <<EOF
{
  "session_id": "$session_id",
  "project_name": "$project_name",
  "current_phase": "requirements",
  "next_phase": "requirements",
  "phase_history": ["requirements"],

  "requirements": {
    "platform": null,
    "features": [],
    "constraints": {},
    "converged": false,
    "validated": false,
    "validation_errors": [],
    "file": null
  },

  "architecture": {
    "components": [],
    "interfaces": [],
    "technologies": {},
    "converged": false,
    "validated": false,
    "validation_errors": [],
    "file": null,
    "tasks_file": null
  },

  "build": {
    "status": "not_started",
    "tasks_file": null,
    "builders": [],
    "completed_count": 0,
    "failed_count": 0
  },

  "review": {
    "feedback": [],
    "issues": [],
    "approved": false
  },

  "conversation": {
    "messages": [],
    "decisions": [],
    "pending_questions": []
  },

  "metadata": {
    "created": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
    "last_updated": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
    "version": "1.0"
  }
}
EOF

    echo "$context_file"
}

# Load context.json
# Args: session_id
# Returns: JSON content
load_context() {
    local session_id="$1"
    local session_dir=$(get_session_dir "$session_id")
    local context_file="$session_dir/context.json"

    if [[ ! -f "$context_file" ]]; then
        echo "Error: Context file not found: $context_file" >&2
        return 1
    fi

    cat "$context_file"
}

# Get context file path
# Args: session_id
# Returns: Path to context.json
get_context_file() {
    local session_id="$1"
    local session_dir=$(get_session_dir "$session_id")
    echo "$session_dir/context.json"
}

# Update context.json with new JSON data
# Args: session_id, json_updates (JSON object with fields to update)
# Example: update_context "$session_id" '{"requirements.platform": "web"}'
update_context() {
    local session_id="$1"
    local json_updates="$2"
    local context_file=$(get_context_file "$session_id")

    if [[ ! -f "$context_file" ]]; then
        echo "Error: Context file not found: $context_file" >&2
        return 1
    fi

    # Update timestamp
    local temp_file=$(mktemp)
    jq --argjson updates "$json_updates" \
       '. * $updates | .metadata.last_updated = now | .metadata.last_updated |= todate' \
       "$context_file" > "$temp_file"
    mv "$temp_file" "$context_file"
}

# Set a specific field in context.json
# Args: session_id, field_path, value
# Example: set_context_field "$session_id" "requirements.platform" "web"
set_context_field() {
    local session_id="$1"
    local field_path="$2"
    local value="$3"
    local context_file=$(get_context_file "$session_id")

    if [[ ! -f "$context_file" ]]; then
        echo "Error: Context file not found: $context_file" >&2
        return 1
    fi

    local temp_file=$(mktemp)
    jq --arg path "$field_path" --arg val "$value" \
       'setpath($path | split("."); $val) | .metadata.last_updated = now | .metadata.last_updated |= todate' \
       "$context_file" > "$temp_file"
    mv "$temp_file" "$context_file"
}

# Get a specific field from context.json
# Args: session_id, field_path
# Example: get_context_field "$session_id" "requirements.platform"
# Returns: Field value
get_context_value() {
    local session_id="$1"
    local field_path="$2"
    local context_file=$(get_context_file "$session_id")

    if [[ ! -f "$context_file" ]]; then
        echo "Error: Context file not found: $context_file" >&2
        return 1
    fi

    jq -r ".$field_path" "$context_file"
}

# Append to an array field in context.json
# Args: session_id, array_path, value
# Example: append_to_context_array "$session_id" "requirements.features" "calculator"
append_to_context_array() {
    local session_id="$1"
    local array_path="$2"
    local value="$3"
    local context_file=$(get_context_file "$session_id")

    if [[ ! -f "$context_file" ]]; then
        echo "Error: Context file not found: $context_file" >&2
        return 1
    fi

    local temp_file=$(mktemp)
    jq --arg path "$array_path" --arg val "$value" \
       'getpath($path | split(".")) += [$val] | .metadata.last_updated = now | .metadata.last_updated |= todate' \
       "$context_file" > "$temp_file"
    mv "$temp_file" "$context_file"
}

# Add a conversation message to context.json
# Args: session_id, role (user|assistant), content
add_conversation_message() {
    local session_id="$1"
    local role="$2"
    local content="$3"
    local context_file=$(get_context_file "$session_id")

    if [[ ! -f "$context_file" ]]; then
        echo "Error: Context file not found: $context_file" >&2
        return 1
    fi

    local temp_file=$(mktemp)
    jq --arg role "$role" --arg content "$content" \
       '.conversation.messages += [{"role": $role, "content": $content, "timestamp": (now | todate)}] |
        .metadata.last_updated = now | .metadata.last_updated |= todate' \
       "$context_file" > "$temp_file"
    mv "$temp_file" "$context_file"
}

# Add a decision to context.json
# Args: session_id, decision_description
add_decision() {
    local session_id="$1"
    local decision="$2"
    local context_file=$(get_context_file "$session_id")

    if [[ ! -f "$context_file" ]]; then
        echo "Error: Context file not found: $context_file" >&2
        return 1
    fi

    local temp_file=$(mktemp)
    jq --arg decision "$decision" \
       '.conversation.decisions += [{"decision": $decision, "timestamp": (now | todate)}] |
        .metadata.last_updated = now | .metadata.last_updated |= todate' \
       "$context_file" > "$temp_file"
    mv "$temp_file" "$context_file"
}

# Update current phase in context.json
# Args: session_id, new_phase
update_current_phase() {
    local session_id="$1"
    local new_phase="$2"
    local context_file=$(get_context_file "$session_id")

    if [[ ! -f "$context_file" ]]; then
        echo "Error: Context file not found: $context_file" >&2
        return 1
    fi

    local temp_file=$(mktemp)
    jq --arg phase "$new_phase" \
       '.current_phase = $phase |
        .phase_history += [$phase] |
        .metadata.last_updated = now | .metadata.last_updated |= todate' \
       "$context_file" > "$temp_file"
    mv "$temp_file" "$context_file"
}

# Check if a phase is converged
# Args: session_id, phase_name
# Returns: 0 if converged, 1 if not
is_phase_converged() {
    local session_id="$1"
    local phase_name="$2"
    local converged=$(get_context_value "$session_id" "$phase_name.converged")

    [[ "$converged" == "true" ]]
}

# Export functions
export -f create_initial_context
export -f load_context
export -f get_context_file
export -f update_context
export -f set_context_field
export -f get_context_value
export -f append_to_context_array
export -f add_conversation_message
export -f add_decision
export -f update_current_phase
export -f is_phase_converged
