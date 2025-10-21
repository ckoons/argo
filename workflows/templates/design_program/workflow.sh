#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# design_program - Interactive program design workflow
#
# This workflow helps users design programs through dialog with AI.
# Output: Design documents in ~/.argo/designs/{program_name}/

set -e

# Load color library
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../../lib/arc-colors.sh"

# Set workflow name for logging
export WORKFLOW_NAME="design_program"

# Directories
DESIGNS_DIR="${HOME}/.argo/designs"
PROGRAM_NAME=""
DESIGN_DIR=""

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

# Show available designs (colorized)
show_available_designs() {
    if [[ -d "$DESIGNS_DIR" ]] && [[ -n "$(ls -A "$DESIGNS_DIR" 2>/dev/null)" ]]; then
        echo ""
        list_header "Available designs:"
        for design_dir in "$DESIGNS_DIR"/*; do
            if [[ -d "$design_dir" ]]; then
                local design_name=$(basename "$design_dir")
                if [[ -f "$design_dir/design.json" ]]; then
                    local purpose=$(jq -r '.purpose' "$design_dir/design.json" 2>/dev/null | head -c 50)
                    list_item "$design_name" "$purpose..."
                else
                    list_pending "$design_name" "(incomplete)"
                fi
            fi
        done
        echo ""
    fi
}

# Phase 1: Program Identity
gather_program_identity() {
    log_phase "Phase 1: Program Identity"

    # Show existing designs if any
    show_available_designs

    prompt "What should we call this program?"
    read -r PROGRAM_NAME

    # Sanitize name (lowercase, underscores)
    PROGRAM_NAME=$(echo "$PROGRAM_NAME" | tr '[:upper:]' '[:lower:]' | tr ' ' '_' | tr -cd '[:alnum:]_')

    DESIGN_DIR="$DESIGNS_DIR/$PROGRAM_NAME"

    # Check for existing design
    if [[ -d "$DESIGN_DIR" ]]; then
        warning "Design already exists: $(path $PROGRAM_NAME)"
        prompt "What would you like to do? (load/overwrite/rename)"
        read -r ACTION

        case "$ACTION" in
            load)
                log "Loading existing design..."
                load_existing_design
                return 0
                ;;
            overwrite)
                warning "Removing existing design..."
                rm -rf "$DESIGN_DIR"
                ;;
            rename)
                gather_program_identity
                return 0
                ;;
            *)
                error "Invalid choice. Aborting."
                exit 1
                ;;
        esac
    fi

    mkdir -p "$DESIGN_DIR"
    success "Creating design: $(path $PROGRAM_NAME)"
}

# Load existing design and show summary
load_existing_design() {
    if [[ -f "$DESIGN_DIR/design.json" ]]; then
        box_header "Existing Design: $PROGRAM_NAME" "📋"
        cat "$DESIGN_DIR/requirements.md" | colorize_markdown
        box_footer

        prompt "What would you like to do?"
        echo -e "  ${YELLOW}1.${NC} ${WHITE}Modify this design${NC} ${GRAY}(update requirements, refine architecture)${NC}"
        echo -e "  ${YELLOW}2.${NC} ${WHITE}Use as-is${NC} ${GRAY}(ready to build)${NC}"
        echo -e "  ${YELLOW}3.${NC} ${WHITE}Start fresh${NC} ${GRAY}(discard and recreate)${NC}"
        echo ""
        prompt "Enter choice (1/2/3):"
        read -r CHOICE

        case "$CHOICE" in
            1)
                info "Loading design for modification..."
                modify_existing_design
                ;;
            2)
                success "Design ready for build_program"
                show_command "Next step" "arc start build_program $PROGRAM_NAME"
                exit 0
                ;;
            3)
                warning "Discarding existing design..."
                rm -rf "$DESIGN_DIR"
                mkdir -p "$DESIGN_DIR"
                success "Ready to create fresh design"
                ;;
            *)
                error "Invalid choice. Exiting."
                exit 1
                ;;
        esac
    else
        warning "Design directory exists but incomplete. Continuing..."
    fi
}

# Modify existing design
modify_existing_design() {
    # Load existing values from design.json
    PURPOSE=$(jq -r '.purpose' "$DESIGN_DIR/design.json")
    USERS=$(jq -r '.users' "$DESIGN_DIR/design.json")
    LANGUAGE=$(jq -r '.language' "$DESIGN_DIR/design.json")
    FEATURES=$(jq -r '.features | join(", ")' "$DESIGN_DIR/design.json")
    SUCCESS_CRITERIA=$(jq -r '.success_criteria' "$DESIGN_DIR/design.json")
    CONSTRAINTS=$(jq -r '.constraints' "$DESIGN_DIR/design.json")

    log_phase "Modify Design"
    info "Current values shown in ${GRAY}gray${NC}. Press Enter to keep, or type new value."

    # Re-gather requirements with current values as defaults
    question "1" "What problem does this program solve?"
    echo -e "  ${GRAY}Current: $PURPOSE${NC}"
    read -r NEW_PURPOSE
    PURPOSE="${NEW_PURPOSE:-$PURPOSE}"

    question "2" "Who will use this program? (you/team/public)"
    echo -e "  ${GRAY}Current: $USERS${NC}"
    read -r NEW_USERS
    USERS="${NEW_USERS:-$USERS}"

    question "3" "What language should we use? (python/bash/c/auto)"
    echo -e "  ${GRAY}Current: $LANGUAGE${NC}"
    read -r NEW_LANGUAGE
    LANGUAGE="${NEW_LANGUAGE:-$LANGUAGE}"

    question "4" "What are the key features? (comma-separated)"
    echo -e "  ${GRAY}Current: $FEATURES${NC}"
    read -r NEW_FEATURES
    FEATURES="${NEW_FEATURES:-$FEATURES}"

    question "5" "How will you know it works? (success criteria)"
    echo -e "  ${GRAY}Current: $SUCCESS_CRITERIA${NC}"
    read -r NEW_SUCCESS_CRITERIA
    SUCCESS_CRITERIA="${NEW_SUCCESS_CRITERIA:-$SUCCESS_CRITERIA}"

    echo ""
    info "Any dependencies or constraints? (Press Enter to keep current)"
    echo -e "  ${GRAY}Current: $CONSTRAINTS${NC}"
    read -r NEW_CONSTRAINTS
    CONSTRAINTS="${NEW_CONSTRAINTS:-$CONSTRAINTS}"

    # Save updated requirements
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
Created: $(jq -r '.created' "$DESIGN_DIR/design.json")
Modified: $(date)
EOF

    success "Requirements updated!"

    # Ask if they want to regenerate architecture
    echo ""
    prompt "Regenerate architecture based on updated requirements? (yes/no)"
    read -r REGEN

    if [[ "$REGEN" == "yes" ]]; then
        design_dialog
    else
        info "Keeping existing architecture"
        finalize_design
    fi
}

# Phase 2: Requirements Gathering
gather_requirements() {
    log_phase "Phase 2: Requirements Gathering"
    info "Let's define what this program should do..."

    question "1" "What problem does this program solve?"
    read -r PURPOSE

    question "2" "Who will use this program? (you/team/public)"
    read -r USERS

    question "3" "What language should we use? (python/bash/c/auto)"
    read -r LANGUAGE

    question "4" "What are the key features? (comma-separated)"
    read -r FEATURES

    question "5" "How will you know it works? (success criteria)"
    read -r SUCCESS_CRITERIA

    echo ""
    info "Any dependencies or constraints? (Press Enter to skip)"
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

    success "Requirements captured!"
}

# Phase 3: AI Design Dialog
design_dialog() {
    log_phase "Phase 3: Design Dialog"
    info "Analyzing requirements and designing architecture..."

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
        error "AI design analysis failed"
        warning "Continuing with manual design..."
        ARCHITECTURE="AI unavailable - manual design required"
    fi

    # Save architecture
    echo "$ARCHITECTURE" > "$DESIGN_DIR/architecture.md"

    # Display to user
    box_header "AI-Proposed Architecture" "${ROBOT}"
    cat "$DESIGN_DIR/architecture.md" | colorize_markdown
    box_footer

    prompt "Does this design work? (yes/no/refine)"
    read -r APPROVAL

    case "$APPROVAL" in
        yes)
            finalize_design
            ;;
        no)
            info "Let's redesign..."
            gather_requirements
            design_dialog
            ;;
        refine)
            prompt "What would you like to change?"
            read -r REFINEMENT
            refine_design "$REFINEMENT"
            ;;
        *)
            warning "Invalid choice. Treating as 'yes'..."
            finalize_design
            ;;
    esac
}

# Refinement phase
refine_design() {
    local refinement="$1"

    info "Refining design based on your feedback..."

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
        warning "AI refinement failed, keeping original design"
    fi

    # Show updated design
    box_header "Updated Architecture" "${ROBOT}"
    cat "$DESIGN_DIR/architecture.md" | colorize_markdown
    box_footer

    prompt "Does this work now? (yes/refine)"
    read -r APPROVAL
    if [[ "$APPROVAL" == "yes" ]]; then
        finalize_design
    else
        prompt "What else to change?"
        read -r REFINEMENT
        refine_design "$REFINEMENT"
    fi
}

# Finalize design
finalize_design() {
    info "Finalizing design..."

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

    banner_summary "Design Complete!"

    echo -e "${BOLD}${WHITE}Program:${NC} $(path $PROGRAM_NAME)"
    echo -e "${BOLD}${WHITE}Language:${NC} $(arg $LANGUAGE)"
    echo -e "${BOLD}${WHITE}Location:${NC} $(path $DESIGN_DIR)"

    list_header "Files created:"
    list_item "requirements.md" "what you want"
    list_item "architecture.md" "how to build it"
    list_item "design.json" "structured design"

    next_steps "Next steps" \
        "Review design: $(cmd cat) $(path $DESIGN_DIR/architecture.md)" \
        "Build program: $(cmd arc start build_program) $(arg $PROGRAM_NAME)"
}

# Main execution
main() {
    banner_welcome "Welcome to Program Designer!" \
                   "AI-assisted program design through interactive dialog"

    info "This workflow will:"
    list_pending "Gather your requirements"
    list_pending "AI proposes architecture"
    list_pending "Create design documents"

    echo ""
    info "Press Enter to start..."
    read -r

    # Ensure designs directory exists
    mkdir -p "$DESIGNS_DIR"

    # Run phases
    gather_program_identity
    gather_requirements
    design_dialog

    success "Design workflow complete!"
}

# Run main
main
