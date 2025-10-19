#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# design_program - Interactive program design workflow
#
# This workflow helps users design programs through dialog with AI.
# Output: Design documents in ~/.argo/designs/{program_name}/

set -e

# Directories
DESIGNS_DIR="${HOME}/.argo/designs"
PROGRAM_NAME=""
DESIGN_DIR=""

# Logging
log() {
    echo "[design_program] $*"
}

# Ask user a question and get response
ask() {
    local question="$1"
    local varname="$2"
    local response

    echo "$question"
    read -r response

    # Use printf with %s to safely assign (avoids eval quoting issues)
    printf -v "$varname" '%s' "$response"
}

# Phase 1: Program Identity
gather_program_identity() {
    log "Phase 1: Program Identity"
    echo ""

    ask "What should we call this program?" PROGRAM_NAME

    # Sanitize name (lowercase, underscores)
    PROGRAM_NAME=$(echo "$PROGRAM_NAME" | tr '[:upper:]' '[:lower:]' | tr ' ' '_' | tr -cd '[:alnum:]_')

    DESIGN_DIR="$DESIGNS_DIR/$PROGRAM_NAME"

    # Check for existing design
    if [[ -d "$DESIGN_DIR" ]]; then
        log "Design already exists: $PROGRAM_NAME"
        ask "What would you like to do? (load/overwrite/rename)" ACTION

        case "$ACTION" in
            load)
                log "Loading existing design..."
                load_existing_design
                return 0
                ;;
            overwrite)
                log "Removing existing design..."
                rm -rf "$DESIGN_DIR"
                ;;
            rename)
                gather_program_identity
                return 0
                ;;
            *)
                log "Invalid choice. Aborting."
                exit 1
                ;;
        esac
    fi

    mkdir -p "$DESIGN_DIR"
    log "Creating design: $PROGRAM_NAME"
}

# Load existing design and show summary
load_existing_design() {
    if [[ -f "$DESIGN_DIR/design.json" ]]; then
        echo ""
        echo "Existing design:"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        cat "$DESIGN_DIR/requirements.md"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo ""
        log "Design ready for build_program"
        log "Run: arc start build_program $PROGRAM_NAME"
        exit 0
    else
        log "Warning: Design directory exists but incomplete. Continuing..."
    fi
}

# Phase 2: Requirements Gathering
gather_requirements() {
    log "Phase 2: Requirements Gathering"
    echo ""
    echo "Let's define what this program should do..."
    echo ""

    ask "1. What problem does this program solve?" PURPOSE
    ask "2. Who will use this program? (you/team/public)" USERS
    ask "3. What language should we use? (python/bash/c/auto)" LANGUAGE
    ask "4. What are the key features? (comma-separated)" FEATURES
    ask "5. How will you know it works? (success criteria)" SUCCESS_CRITERIA
    echo ""
    echo "6. Any dependencies or constraints? (Press Enter to skip)"
    read -r CONSTRAINTS

    # Save to requirements.md
    cat > "$DESIGN_DIR/requirements.md" <<EOF
# Requirements: $PROGRAM_NAME

## Purpose
$PURPOSE

## Target Users
$USERS

## Key Features
$(echo "$FEATURES" | tr ',' '\n' | sed 's/^[[:space:]]*/- /')

## Success Criteria
$SUCCESS_CRITERIA

## Language
$LANGUAGE

## Constraints
${CONSTRAINTS:-None specified}

---
Created: $(date)
EOF

    log "Requirements captured!"
}

# Phase 3: AI Design Dialog
design_dialog() {
    log "Phase 3: Design Dialog"
    echo ""
    echo "Analyzing requirements and designing architecture..."
    echo ""

    REQUIREMENTS=$(cat "$DESIGN_DIR/requirements.md")

    ARCHITECTURE=$(ci <<EOF
You are an expert software architect. Based on these requirements, design a program:

$REQUIREMENTS

Provide a detailed design covering:

1. **Architecture Overview**: High-level structure (single file, modules, classes, etc.)

2. **Component Breakdown**: What are the main components and their responsibilities?

3. **Data Structures**: What data structures are needed? (files, database, memory, etc.)

4. **Interface Design**: Command-line arguments, flags, input/output format

5. **Error Handling**: What can go wrong and how to handle it

6. **Testing Strategy**: How to test each component

7. **File Structure**: What files/directories should be created

8. **Implementation Steps**: Ordered steps to build this (for build_program to follow)

Be specific and actionable. Focus on clarity over cleverness.
EOF
)

    if [ $? -ne 0 ] || [ -z "$ARCHITECTURE" ]; then
        log "Error: AI design analysis failed"
        log "Continuing with manual design..."
        ARCHITECTURE="AI unavailable - manual design required"
    fi

    # Save architecture
    echo "$ARCHITECTURE" > "$DESIGN_DIR/architecture.md"

    # Display to user
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Proposed Architecture:"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    cat "$DESIGN_DIR/architecture.md"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""

    ask "Does this design work? (yes/no/refine)" APPROVAL

    case "$APPROVAL" in
        yes)
            finalize_design
            ;;
        no)
            log "Let's redesign..."
            gather_requirements
            design_dialog
            ;;
        refine)
            ask "What would you like to change?" REFINEMENT
            refine_design "$REFINEMENT"
            ;;
        *)
            log "Invalid choice. Treating as 'yes'..."
            finalize_design
            ;;
    esac
}

# Refinement phase
refine_design() {
    local refinement="$1"

    log "Refining design based on your feedback..."

    UPDATED_ARCHITECTURE=$(ci <<EOF
The user wants to refine the design with this feedback:
$refinement

Original requirements:
$(cat "$DESIGN_DIR/requirements.md")

Original architecture:
$(cat "$DESIGN_DIR/architecture.md")

Provide an updated architecture that incorporates this feedback.
Keep the same structure (8 sections) but update based on feedback.
EOF
)

    if [ $? -eq 0 ] && [ -n "$UPDATED_ARCHITECTURE" ]; then
        echo "$UPDATED_ARCHITECTURE" > "$DESIGN_DIR/architecture.md"
    else
        log "Warning: AI refinement failed, keeping original design"
    fi

    # Show updated design
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Updated Architecture:"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    cat "$DESIGN_DIR/architecture.md"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""

    ask "Does this work now? (yes/refine)" APPROVAL
    if [[ "$APPROVAL" == "yes" ]]; then
        finalize_design
    else
        ask "What else to change?" REFINEMENT
        refine_design "$REFINEMENT"
    fi
}

# Finalize design
finalize_design() {
    log "Finalizing design..."

    # Create design.json (structured for build_program)
    cat > "$DESIGN_DIR/design.json" <<EOF
{
  "program_name": "$PROGRAM_NAME",
  "purpose": $(printf '%s' "$PURPOSE" | jq -Rs .),
  "language": "$LANGUAGE",
  "users": $(printf '%s' "$USERS" | jq -Rs .),
  "features": $(echo "$FEATURES" | jq -Rc 'split(",") | map(gsub("^\\s+|\\s+$";""))'),
  "success_criteria": $(printf '%s' "$SUCCESS_CRITERIA" | jq -Rs .),
  "constraints": $(printf '%s' "$CONSTRAINTS" | jq -Rs .),
  "design_complete": true,
  "created": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "build_location": "~/.argo/tools/$PROGRAM_NAME"
}
EOF

    log "Design complete!"
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Design Summary"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Program: $PROGRAM_NAME"
    echo "Language: $LANGUAGE"
    echo "Location: $DESIGN_DIR"
    echo ""
    echo "Files created:"
    echo "  ✓ requirements.md   (what you want)"
    echo "  ✓ architecture.md   (how to build it)"
    echo "  ✓ design.json       (structured design)"
    echo ""
    echo "Next steps:"
    echo "  1. Review design: cat $DESIGN_DIR/architecture.md"
    echo "  2. Build program: arc start build_program $PROGRAM_NAME"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
}

# Main execution
main() {
    log "Welcome to Program Designer!"
    echo ""
    echo "This workflow will help you design a program through dialog with AI."
    echo "We'll gather requirements, propose an architecture, and create design docs."
    echo ""
    echo "Press Enter to start..."
    read -r
    echo ""

    # Ensure designs directory exists
    mkdir -p "$DESIGNS_DIR"

    # Run phases
    gather_program_identity
    gather_requirements
    design_dialog

    log "Design workflow complete!"
}

# Run main
main
