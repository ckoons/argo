#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# checkpoint.sh - Checkpoint save/restore system
#
# Provides functions for saving and restoring workflow state via checkpoints.
# Checkpoints are stored as timestamped JSON files in .argo-project/checkpoints/.

# Default checkpoint directory
CHECKPOINT_DIR="${CHECKPOINT_DIR:-.argo-project/checkpoints}"

#
# ensure_checkpoint_dir - Create checkpoint directory if it doesn't exist
#
ensure_checkpoint_dir() {
    if [[ ! -d "$CHECKPOINT_DIR" ]]; then
        mkdir -p "$CHECKPOINT_DIR" 2>/dev/null
    fi
}

#
# save_checkpoint - Save current state as a named checkpoint
#
# Arguments:
#   name - Checkpoint identifier (short, no spaces)
#   description - Human-readable description
#
# Returns:
#   0 on success, 1 on failure
#
save_checkpoint() {
    local name="$1"
    local description="$2"

    # Validate arguments
    if [[ -z "$name" ]]; then
        echo "ERROR: checkpoint name required" >&2
        return 1
    fi

    if [[ -z "$description" ]]; then
        echo "ERROR: checkpoint description required" >&2
        return 1
    fi

    # Check state file exists
    if [[ ! -f ".argo-project/state.json" ]]; then
        echo "ERROR: state.json not found" >&2
        return 1
    fi

    # Ensure checkpoint directory exists
    ensure_checkpoint_dir

    # Generate checkpoint filename with timestamp
    local timestamp=$(date +%s)
    local checkpoint_file="$CHECKPOINT_DIR/${timestamp}_${name}.json"

    # Copy state to checkpoint
    cp ".argo-project/state.json" "$checkpoint_file"
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to create checkpoint file" >&2
        return 1
    fi

    # Log checkpoint creation
    log_checkpoint "$name" "$description"

    # Update state.json checkpoints object
    local iso_timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
    update_state "checkpoints.$name" "$iso_timestamp" 2>/dev/null

    return 0
}

#
# restore_checkpoint - Restore state from a named checkpoint
#
# Arguments:
#   name - Checkpoint identifier
#
# Returns:
#   0 on success, 1 if checkpoint not found
#
restore_checkpoint() {
    local name="$1"

    # Validate arguments
    if [[ -z "$name" ]]; then
        echo "ERROR: checkpoint name required" >&2
        return 1
    fi

    # Find checkpoint file (pattern: {timestamp}_{name}.json)
    local checkpoint_file=$(ls "$CHECKPOINT_DIR"/*_${name}.json 2>/dev/null | head -n1)

    if [[ -z "$checkpoint_file" ]] || [[ ! -f "$checkpoint_file" ]]; then
        echo "ERROR: Checkpoint not found: $name" >&2
        return 1
    fi

    # Restore state from checkpoint
    cp "$checkpoint_file" ".argo-project/state.json"
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to restore checkpoint" >&2
        return 1
    fi

    # Log restoration
    log_checkpoint "$name" "Restored from checkpoint"

    return 0
}

#
# list_checkpoints - List all available checkpoints
#
# Returns:
#   Tab-separated lines: name\ttimestamp\tdescription
#   Exit code 0
#
list_checkpoints() {
    # Check if checkpoint directory exists
    if [[ ! -d "$CHECKPOINT_DIR" ]]; then
        return 0  # Empty list is success
    fi

    # List checkpoint files
    for checkpoint_file in "$CHECKPOINT_DIR"/*.json; do
        # Skip if no files found
        if [[ ! -f "$checkpoint_file" ]]; then
            continue
        fi

        # Extract name and timestamp from filename
        local filename=$(basename "$checkpoint_file" .json)
        local timestamp=$(echo "$filename" | cut -d'_' -f1)
        local name=$(echo "$filename" | cut -d'_' -f2-)

        # Convert Unix timestamp to ISO 8601
        local iso_timestamp=$(date -u -r "$timestamp" +"%Y-%m-%dT%H:%M:%SZ" 2>/dev/null)
        if [[ -z "$iso_timestamp" ]]; then
            iso_timestamp="$timestamp"  # Fallback if conversion fails
        fi

        # Get description from state.json checkpoints object if available
        local description=""
        if [[ -f ".argo-project/state.json" ]]; then
            description=$(jq -r ".checkpoints.\"$name\" // empty" ".argo-project/state.json" 2>/dev/null)
        fi

        # Output tab-separated
        echo -e "${name}\t${iso_timestamp}\t${description}"
    done

    return 0
}

#
# delete_checkpoint - Delete a specific checkpoint
#
# Arguments:
#   name - Checkpoint identifier
#
# Returns:
#   0 on success, 1 if checkpoint not found
#
delete_checkpoint() {
    local name="$1"

    # Validate arguments
    if [[ -z "$name" ]]; then
        echo "ERROR: checkpoint name required" >&2
        return 1
    fi

    # Find checkpoint file
    local checkpoint_file=$(ls "$CHECKPOINT_DIR"/*_${name}.json 2>/dev/null | head -n1)

    if [[ -z "$checkpoint_file" ]] || [[ ! -f "$checkpoint_file" ]]; then
        echo "ERROR: Checkpoint not found: $name" >&2
        return 1
    fi

    # Delete file
    rm -f "$checkpoint_file"
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Failed to delete checkpoint" >&2
        return 1
    fi

    # Remove from state.json checkpoints object
    if [[ -f ".argo-project/state.json" ]]; then
        local temp_file=".argo-project/state.json.tmp"
        jq "del(.checkpoints.\"$name\")" ".argo-project/state.json" > "$temp_file" 2>/dev/null
        if [[ $? -eq 0 ]]; then
            mv "$temp_file" ".argo-project/state.json"
        fi
    fi

    return 0
}

#
# cleanup_old_checkpoints - Delete checkpoints older than specified age
#
# Arguments:
#   max_age_days - Maximum age in days (default: 30)
#
# Returns:
#   Number of checkpoints deleted to stdout
#   Exit code 0
#
cleanup_old_checkpoints() {
    local max_age_days="${1:-30}"
    local deleted_count=0

    # Check if checkpoint directory exists
    if [[ ! -d "$CHECKPOINT_DIR" ]]; then
        echo "0"
        return 0
    fi

    # Calculate cutoff timestamp (now - max_age_days)
    local cutoff_timestamp=$(date -d "${max_age_days} days ago" +%s 2>/dev/null)
    if [[ -z "$cutoff_timestamp" ]]; then
        # macOS doesn't support -d, use alternative
        cutoff_timestamp=$(date -v-${max_age_days}d +%s 2>/dev/null)
    fi

    if [[ -z "$cutoff_timestamp" ]]; then
        echo "ERROR: Failed to calculate cutoff date" >&2
        echo "0"
        return 0
    fi

    # Check each checkpoint
    for checkpoint_file in "$CHECKPOINT_DIR"/*.json; do
        # Skip if no files found
        if [[ ! -f "$checkpoint_file" ]]; then
            continue
        fi

        # Get file modification time (Unix timestamp)
        local file_mtime=$(stat -f %m "$checkpoint_file" 2>/dev/null)
        if [[ -z "$file_mtime" ]]; then
            # Linux stat syntax
            file_mtime=$(stat -c %Y "$checkpoint_file" 2>/dev/null)
        fi

        if [[ -z "$file_mtime" ]]; then
            continue  # Skip if we can't get modification time
        fi

        # Compare timestamps
        if [[ "$file_mtime" -lt "$cutoff_timestamp" ]]; then
            # File is old, delete it
            local filename=$(basename "$checkpoint_file" .json)
            rm -f "$checkpoint_file"
            if [[ $? -eq 0 ]]; then
                ((deleted_count++))

                # Remove from state.json
                local name=$(echo "$filename" | cut -d'_' -f2-)
                if [[ -f ".argo-project/state.json" ]]; then
                    local temp_file=".argo-project/state.json.tmp"
                    jq "del(.checkpoints.\"$name\")" ".argo-project/state.json" > "$temp_file" 2>/dev/null
                    if [[ $? -eq 0 ]]; then
                        mv "$temp_file" ".argo-project/state.json"
                    fi
                fi
            fi
        fi
    done

    echo "$deleted_count"
    return 0
}

# Export functions
export -f ensure_checkpoint_dir
export -f save_checkpoint
export -f restore_checkpoint
export -f list_checkpoints
export -f delete_checkpoint
export -f cleanup_old_checkpoints
