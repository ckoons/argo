#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# conflict-resolver.sh - Phase 5.2: Merge conflict resolution
#
# Handles merge conflicts through:
# - Trivial conflict detection (AI-based)
# - Auto-resolution of simple conflicts
# - Complex conflict handling
# - Branch respawn on merge failure

# Configuration
ARGO_MAX_MERGE_ATTEMPTS="${ARGO_MAX_MERGE_ATTEMPTS:-2}"

# Analyze merge conflicts using AI
# Args: builder_id, feature_branch, builder_branch
# Returns: JSON analysis
analyze_merge_conflicts() {
    local builder_id="$1"
    local feature_branch="$2"
    local builder_branch="$3"

    # Get list of conflicted files
    local conflicted_files=$(git diff --name-only --diff-filter=U)

    if [[ -z "$conflicted_files" ]]; then
        echo '{"has_conflicts":false}'
        return 0
    fi

    # Get conflict details for each file
    local conflicts_json="["
    local first=true

    while IFS= read -r file; do
        if [[ ! -f "$file" ]]; then
            continue
        fi

        # Extract conflict markers
        local conflict_content=$(grep -A 5 -B 5 '^<<<<<<< \|^=======$\|^>>>>>>> ' "$file" || echo "")

        if [[ -n "$conflict_content" ]]; then
            if [[ "$first" == "false" ]]; then
                conflicts_json+=","
            fi
            first=false

            # Escape for JSON
            local escaped_content=$(echo "$conflict_content" | jq -Rs .)

            conflicts_json+="{\"file\":\"$file\",\"content\":$escaped_content}"
        fi
    done <<< "$conflicted_files"

    conflicts_json+="]"

    # Ask AI to analyze conflicts
    local analysis=$(ci <<EOF
You are analyzing merge conflicts in a parallel development workflow.

Builder: $builder_id
Merging: $builder_branch into $feature_branch

Conflicts:
$conflicts_json

Analyze these conflicts and determine:
1. **Conflict Type**: "trivial" (whitespace, imports, formatting) or "complex" (logic, data structures)
2. **Can Auto-Resolve**: true if conflicts are simple enough to auto-resolve
3. **Resolution Strategy**: How to resolve each conflict
4. **Risk Level**: "low", "medium", or "high"

Respond in JSON format:
{
  "has_conflicts": true,
  "conflict_count": <number>,
  "conflict_type": "trivial" or "complex",
  "can_auto_resolve": true or false,
  "risk_level": "low" or "medium" or "high",
  "conflicts": [
    {
      "file": "filename",
      "type": "whitespace|imports|formatting|logic|structure",
      "resolution": "specific resolution strategy",
      "auto_resolvable": true or false
    }
  ],
  "recommendation": "auto_resolve" or "manual_review" or "respawn_builder"
}
EOF
)

    echo "$analysis"
}

# Auto-resolve trivial conflicts
# Args: builder_id
# Returns: 0 if resolved, 1 if failed
auto_resolve_conflicts() {
    local builder_id="$1"
    local feature_branch="$2"
    local builder_branch="$3"

    info "Attempting auto-resolution for $(label $builder_id)..."

    local analysis=$(analyze_merge_conflicts "$builder_id" "$feature_branch" "$builder_branch")
    local can_auto_resolve=$(echo "$analysis" | jq -r '.can_auto_resolve')
    local conflict_type=$(echo "$analysis" | jq -r '.conflict_type')
    local recommendation=$(echo "$analysis" | jq -r '.recommendation')

    if [[ "$can_auto_resolve" != "true" ]]; then
        warning "Conflicts are too complex for auto-resolution"
        return 1
    fi

    # Get conflict details
    local conflict_count=$(echo "$analysis" | jq -r '.conflict_count')
    local conflicts=$(echo "$analysis" | jq -r '.conflicts')

    box_header "Auto-Resolving $conflict_count Conflicts" "$ROBOT"
    echo "$conflicts" | jq -r '.[] | "  [\(.file)] \(.type) - \(.resolution)"'
    box_footer

    # For each auto-resolvable conflict, apply resolution
    local resolved=0
    for i in $(seq 0 $((conflict_count - 1))); do
        local file=$(echo "$conflicts" | jq -r ".[$i].file")
        local auto_resolvable=$(echo "$conflicts" | jq -r ".[$i].auto_resolvable")
        local conflict_type=$(echo "$conflicts" | jq -r ".[$i].type")

        if [[ "$auto_resolvable" == "true" ]] && [[ -f "$file" ]]; then
            case "$conflict_type" in
                whitespace)
                    # Take ours for whitespace conflicts
                    git checkout --ours "$file"
                    git add "$file"
                    resolved=$((resolved + 1))
                    success "Resolved whitespace conflict: $file"
                    ;;
                imports)
                    # Merge both import sections
                    resolve_import_conflict "$file"
                    if [[ $? -eq 0 ]]; then
                        git add "$file"
                        resolved=$((resolved + 1))
                        success "Resolved import conflict: $file"
                    fi
                    ;;
                formatting)
                    # Take ours and reformat
                    git checkout --ours "$file"
                    git add "$file"
                    resolved=$((resolved + 1))
                    success "Resolved formatting conflict: $file"
                    ;;
                *)
                    warning "Skipping complex conflict in $file"
                    ;;
            esac
        fi
    done

    if [[ $resolved -eq $conflict_count ]]; then
        success "All $conflict_count conflicts resolved automatically"
        return 0
    else
        warning "Resolved $resolved of $conflict_count conflicts"
        return 1
    fi
}

# Resolve import conflicts (merge both sides)
# Args: file
# Returns: 0 on success
resolve_import_conflict() {
    local file="$1"

    # Extract imports from both sides of conflict
    local temp_file=$(mktemp)

    # This is a simplified version - real implementation would:
    # 1. Extract imports from <<<<<<< section
    # 2. Extract imports from >>>>>>> section
    # 3. Merge and deduplicate
    # 4. Write back to file

    # For now, use AI to merge imports
    local file_content=$(cat "$file")
    local merged=$(ci <<EOF
This file has an import conflict. Merge the imports from both sides intelligently.

File content with conflict markers:
$file_content

Output the corrected file with:
1. All imports from both sides merged
2. Duplicates removed
3. Sorted alphabetically
4. No conflict markers

Output ONLY the corrected file content.
EOF
)

    echo "$merged" > "$file"
    return 0
}

# Respawn builder after merge failure
# Args: session_id, builder_id, feature_branch, design_dir, build_dir
# Returns: 0 on success, 1 on failure
respawn_builder_for_merge() {
    local session_id="$1"
    local builder_id="$2"
    local feature_branch="$3"
    local design_dir="$4"
    local build_dir="$5"

    warning "Complex conflicts detected - respawning $(label $builder_id) with updated context"

    local builder_file="$(get_session_dir "$session_id")/builders/$builder_id.json"

    # Get merge attempt count
    local merge_attempts=$(jq -r '.merge_attempts // 0' "$builder_file")

    if [[ $merge_attempts -ge $ARGO_MAX_MERGE_ATTEMPTS ]]; then
        error "Max merge attempts reached for $builder_id"
        return 1
    fi

    # Increment merge attempts
    local temp_file=$(mktemp)
    jq --arg count "$((merge_attempts + 1))" \
       '.merge_attempts = ($count | tonumber)' \
       "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"

    # Abort current merge
    git_abort_merge

    # Delete builder branch
    local builder_branch="$feature_branch/$builder_id"
    git_delete_branch "$builder_branch"

    # Get current state of feature branch
    local feature_state=$(git show "$feature_branch:$file" 2>/dev/null || echo "")

    # Update task context with feature branch state
    info "Updating task with current feature branch context..."

    # Recreate builder branch
    git_create_branch "$builder_branch" "$feature_branch"

    # Update builder state
    local temp_file=$(mktemp)
    jq '.status = "respawning_for_merge"' "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"

    # Respawn builder
    local lib_dir="$(dirname "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")/lib"
    "$lib_dir/builder_ci_loop.sh" "$session_id" "$builder_id" "$design_dir" "$build_dir" &
    local new_pid=$!

    success "Respawned $builder_id for merge (PID: $new_pid)"
    echo "$new_pid"
    return 0
}

# Handle merge conflicts (Phase 5.2 main entry point)
# Args: session_id, builder_id, feature_branch, builder_branch, design_dir, build_dir
# Returns: 0 if resolved, 1 if escalation needed
handle_merge_conflict() {
    local session_id="$1"
    local builder_id="$2"
    local feature_branch="$3"
    local builder_branch="$4"
    local design_dir="$5"
    local build_dir="$6"

    warning "Merge conflict detected for $(label $builder_id)"

    # Step 1: Analyze conflicts
    local analysis=$(analyze_merge_conflicts "$builder_id" "$feature_branch" "$builder_branch")
    local recommendation=$(echo "$analysis" | jq -r '.recommendation')
    local conflict_type=$(echo "$analysis" | jq -r '.conflict_type')
    local risk_level=$(echo "$analysis" | jq -r '.risk_level')

    echo ""
    box_header "Conflict Analysis: $builder_id" "$WARNING"
    echo "Conflict type: $(label $conflict_type)"
    echo "Risk level: $(label $risk_level)"
    echo ""
    echo "Recommendation: $recommendation"
    box_footer

    # Try resolution strategies in order
    if [[ "$recommendation" == "auto_resolve" ]]; then
        # Try auto-resolution
        if auto_resolve_conflicts "$builder_id" "$feature_branch" "$builder_branch"; then
            success "Conflicts resolved automatically"
            return 0
        else
            # Auto-resolution failed, try respawn
            warning "Auto-resolution failed, attempting respawn..."
            recommendation="respawn_builder"
        fi
    fi

    if [[ "$recommendation" == "respawn_builder" ]]; then
        # Respawn builder with updated context
        local new_pid=$(respawn_builder_for_merge "$session_id" "$builder_id" "$feature_branch" "$design_dir" "$build_dir")
        if [[ $? -eq 0 ]]; then
            return 0
        else
            # Respawn failed, escalate
            warning "Respawn failed, escalating to user..."
            recommendation="manual_review"
        fi
    fi

    # Manual intervention needed (either recommended or fell through from failures)
    escalate_merge_conflict "$session_id" "$builder_id" "$feature_branch" "$builder_branch"
    return 1
}

# Escalate merge conflict to user
# Args: session_id, builder_id, feature_branch, builder_branch
escalate_merge_conflict() {
    local session_id="$1"
    local builder_id="$2"
    local feature_branch="$3"
    local builder_branch="$4"

    echo ""
    box_header "Manual Conflict Resolution Required: $builder_id" "$ALERT"

    echo "Merge conflict cannot be resolved automatically."
    echo ""
    echo "Builder: $(label $builder_id)"
    echo "Merging: $builder_branch → $feature_branch"
    echo ""
    echo "Conflicted files:"
    git diff --name-only --diff-filter=U | while read -r file; do
        echo "  - $file"
    done
    echo ""
    echo "Options:"
    echo "  1. Resolve manually: git mergetool"
    echo "  2. Skip builder: git merge --abort"
    echo "  3. View conflicts: git diff"

    box_footer

    # Mark builder as needs assistance
    local builder_file="$(get_session_dir "$session_id")/builders/$builder_id.json"
    local temp_file=$(mktemp)
    jq '.needs_assistance = true | .status = "merge_conflict"' \
       "$builder_file" > "$temp_file"
    mv "$temp_file" "$builder_file"
}

# Export functions
export -f analyze_merge_conflicts
export -f auto_resolve_conflicts
export -f resolve_import_conflict
export -f respawn_builder_for_merge
export -f handle_merge_conflict
export -f escalate_merge_conflict
