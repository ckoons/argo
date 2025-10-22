#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# build_program - Build a program from design documents
#
# This workflow reads a design created by design_program and generates
# complete working code with tests and documentation.
#
# Supports parallel builder orchestration for complex projects.

set -e

# Get workflow root directory
WORKFLOW_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATES_DIR="$(dirname "$WORKFLOW_DIR")"
ARGO_ROOT="$(dirname "$TEMPLATES_DIR")"
LIB_DIR="$ARGO_ROOT/lib"

# Load required libraries
source "$LIB_DIR/arc-colors.sh"
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/git-helpers.sh"

# Load configuration
if [[ -f "${HOME}/.argo/config" ]]; then
    source "${HOME}/.argo/config"
fi

# Set defaults if not in config
ARGO_MAX_BUILDERS="${ARGO_MAX_BUILDERS:-5}"
ARGO_MAX_TEST_ATTEMPTS="${ARGO_MAX_TEST_ATTEMPTS:-5}"
ARGO_MIN_FEATURES_FOR_PARALLEL="${ARGO_MIN_FEATURES_FOR_PARALLEL:-3}"

# Set workflow name for logging
export WORKFLOW_NAME="build_program"

# Directories
DESIGNS_DIR="${HOME}/.argo/designs"
TOOLS_DIR="${HOME}/.argo/tools"
PROGRAM_NAME=""
DESIGN_DIR=""
BUILD_DIR=""
LANGUAGE=""

# Session state
SESSION_ID=""
PROJECT_TYPE=""
PROJECT_DIR=""
IS_GIT_REPO="no"
IS_GITHUB_PROJECT="no"
MAIN_BRANCH=""
FEATURE_BRANCH=""

# Parallel build state
PARALLEL_BUILD=false
BUILDER_COUNT=0
declare -a BUILDER_IDS
declare -a BUILDER_PIDS

# Load session context if available
load_session_context() {
    # Check for last session file
    local last_session_file="${HOME}/.argo/.last_session"

    if [[ -f "$last_session_file" ]]; then
        SESSION_ID=$(cat "$last_session_file")

        if [[ -n "$SESSION_ID" ]] && session_exists "$SESSION_ID"; then
            info "Loading session: $(label $SESSION_ID)"

            # Load context
            PROJECT_TYPE=$(get_context_field "$SESSION_ID" "project_type")
            PROJECT_DIR=$(get_context_field "$SESSION_ID" "project_dir")
            PROGRAM_NAME=$(get_context_field "$SESSION_ID" "project_name")
            IS_GIT_REPO=$(get_context_field "$SESSION_ID" "is_git_repo")
            IS_GITHUB_PROJECT=$(get_context_field "$SESSION_ID" "is_github_project")
            MAIN_BRANCH=$(get_context_field "$SESSION_ID" "main_branch")

            success "Session loaded"
            list_item "Project type:" "$(label $PROJECT_TYPE)"
            list_item "Project name:" "$PROGRAM_NAME"
            list_item "Directory:" "$(path $PROJECT_DIR)"
            echo ""

            # Update session phase
            update_phase "$SESSION_ID" "build"

            return 0
        fi
    fi

    # No session found
    return 1
}

# Ask user a question and get response
ask() {
    local question="$1"
    local varname="$2"
    local response

    echo "$question"
    read -r response

    printf -v "$varname" '%s' "$response"
}

# Phase 1: Load Design
load_design() {
    # Get program name (from workflow argument or ask)
    if [[ -n "$1" ]]; then
        PROGRAM_NAME="$1"
    else
        prompt "Which program design should I build?"
        read -r PROGRAM_NAME
    fi

    DESIGN_DIR="$DESIGNS_DIR/$PROGRAM_NAME"

    if [[ ! -f "$DESIGN_DIR/design.json" ]]; then
        error "Design not found: $(path $DESIGN_DIR/design.json)"
        echo ""
        list_header "Available designs:"
        if [[ -d "$DESIGNS_DIR" ]] && [[ -n "$(ls -A "$DESIGNS_DIR" 2>/dev/null)" ]]; then
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
        else
            info "(none - run $(cmd 'arc start design_program') to create one)"
        fi
        echo ""
        show_command "Create a design first" "arc start design_program"
        exit 1
    fi

    success "Loading design: $(path $PROGRAM_NAME)"

    # Parse design.json
    LANGUAGE=$(jq -r '.language' "$DESIGN_DIR/design.json")
    PURPOSE=$(jq -r '.purpose' "$DESIGN_DIR/design.json")

    # Auto-detect language if set to "auto"
    if [[ "$LANGUAGE" == "auto" ]]; then
        LANGUAGE="bash"  # Default
        info "Language set to auto - defaulting to $(arg bash)"
    fi

    echo -e "${BOLD}${WHITE}Language:${NC} $(arg $LANGUAGE)"
    echo -e "${BOLD}${WHITE}Purpose:${NC} $PURPOSE"
}

# Phase 2: Determine Build Location
determine_build_location() {
    log_phase "Phase 2: Build Location"

    # Check if user specified location as second argument
    if [[ -n "$2" ]]; then
        BUILD_DIR="${2/#\~/$HOME}"
    else
        DEFAULT_PATH="$TOOLS_DIR/$PROGRAM_NAME"
        echo -e "${GRAY}Default: $(path $DEFAULT_PATH)${NC}"
        prompt "Where should I build this? (Press Enter for default):"
        read -r USER_PATH

        if [[ -n "$USER_PATH" ]]; then
            # Expand tilde
            BUILD_DIR="${USER_PATH/#\~/$HOME}"
        else
            # Use default
            BUILD_DIR="$DEFAULT_PATH"
        fi
    fi

    # Verify/create directory
    if [[ -d "$BUILD_DIR" ]]; then
        warning "Directory exists: $(path $BUILD_DIR)"
        prompt "Overwrite? (yes/no)"
        read -r OVERWRITE
        if [[ "$OVERWRITE" != "yes" ]]; then
            error "Aborting."
            exit 1
        fi
        rm -rf "$BUILD_DIR"
    fi

    mkdir -p "$BUILD_DIR"
    success "Building in: $(path $BUILD_DIR)"
}

# Phase 3: Pre-Build Questions
ask_clarifying_questions() {
    log_phase "Phase 3: Pre-Build Analysis"
    info "Analyzing design for completeness..."

    REQUIREMENTS=$(cat "$DESIGN_DIR/requirements.md")
    ARCHITECTURE=$(cat "$DESIGN_DIR/architecture.md")

    QUESTIONS=$(ci <<EOF
You are about to build this program:

Requirements:
$REQUIREMENTS

Architecture:
$ARCHITECTURE

Before building, what clarifying questions do you need to ask?
Focus on:
- Ambiguities in the design
- Missing implementation details
- User preferences (e.g., config file format, output style)

If the design is clear and complete, respond with: "No questions - ready to build"

Otherwise, provide numbered questions (max 3 most important).
EOF
)

    if [[ "$QUESTIONS" == *"ready to build"* ]] || [[ "$QUESTIONS" == *"Ready to build"* ]]; then
        success "Design is complete. Ready to build!"
        ANSWERS=""
    else
        box_header "Pre-Build Questions" "${ROBOT}"
        echo "$QUESTIONS"
        box_footer

        prompt "Provide answers (or 'skip' to proceed anyway):"
        read -r ANSWERS

        if [[ "$ANSWERS" != "skip" ]]; then
            # Save Q&A to design
            echo "" >> "$DESIGN_DIR/conversation.log"
            echo "Build Questions ($(date)):" >> "$DESIGN_DIR/conversation.log"
            echo "$QUESTIONS" >> "$DESIGN_DIR/conversation.log"
            echo "" >> "$DESIGN_DIR/conversation.log"
            echo "Answers:" >> "$DESIGN_DIR/conversation.log"
            echo "$ANSWERS" >> "$DESIGN_DIR/conversation.log"
        fi
    fi
}

# Phase 4: Build Program
build_program() {
    log_phase "Phase 4: Code Generation"

    REQUIREMENTS=$(cat "$DESIGN_DIR/requirements.md")
    ARCHITECTURE=$(cat "$DESIGN_DIR/architecture.md")
    ANSWERS_CONTEXT="${ANSWERS:-No additional clarifications}"

    # Step 1: Generate main program file
    step "1/4" "Generating source code..."

    PROGRAM_CODE=$(ci <<EOF
Build the complete program based on this design:

Requirements:
$REQUIREMENTS

Architecture:
$ARCHITECTURE

Additional context from user:
$ANSWERS_CONTEXT

Generate the complete, working source code for the main program file in $LANGUAGE.

Requirements:
- Include proper shebang/headers for $LANGUAGE
- Add copyright: © 2025 Casey Koons All rights reserved
- Include comprehensive error handling
- Add helpful comments
- Follow best practices for $LANGUAGE
- Make it production-ready
- Implement all features from requirements

Output ONLY the source code, no explanations or markdown formatting.
EOF
)

    if [ $? -ne 0 ] || [ -z "$PROGRAM_CODE" ]; then
        error "Code generation failed"
        exit 1
    fi

    # Strip markdown code blocks if present
    PROGRAM_CODE=$(echo "$PROGRAM_CODE" | sed '/^```/d')

    # Determine file extension
    case "$LANGUAGE" in
        python) EXT="py" ;;
        bash) EXT="sh" ;;
        c) EXT="c" ;;
        *) EXT="sh" ;;
    esac

    # Write main program
    echo "$PROGRAM_CODE" > "$BUILD_DIR/$PROGRAM_NAME.$EXT"
    chmod +x "$BUILD_DIR/$PROGRAM_NAME.$EXT"

    # Create symlink without extension for convenience
    ln -sf "$PROGRAM_NAME.$EXT" "$BUILD_DIR/$PROGRAM_NAME"

    success "Created $(path $PROGRAM_NAME.$EXT)"

    # Step 2: Generate tests
    step "2/4" "Generating test suite..."

    TEST_CODE=$(ci <<EOF
Generate a comprehensive test suite for this program:

Program name: $PROGRAM_NAME
Language: $LANGUAGE

Program code:
$PROGRAM_CODE

Requirements:
$REQUIREMENTS

Create tests that verify the success criteria:
$(jq -r '.success_criteria' "$DESIGN_DIR/design.json")

Generate complete test code in $LANGUAGE that:
- Tests all major features
- Includes positive and negative test cases
- Has clear pass/fail output
- Can be run standalone

Output ONLY the test code, no explanations or markdown.
EOF
)

    if [ $? -ne 0 ] || [ -z "$TEST_CODE" ]; then
        warning "Test generation failed, creating placeholder..."
        TEST_CODE="#!/bin/bash\necho 'TODO: Add tests'\nexit 0"
    fi

    # Strip markdown
    TEST_CODE=$(echo "$TEST_CODE" | sed '/^```/d')

    mkdir -p "$BUILD_DIR/tests"
    echo "$TEST_CODE" > "$BUILD_DIR/tests/test_$PROGRAM_NAME.$EXT"
    chmod +x "$BUILD_DIR/tests/test_$PROGRAM_NAME.$EXT"

    success "Created $(path tests/test_$PROGRAM_NAME.$EXT)"

    # Step 3: Generate README
    step "3/4" "Generating documentation..."

    cat > "$BUILD_DIR/README.md" <<EOF
# $PROGRAM_NAME

## Purpose
$PURPOSE

## Installation

\`\`\`bash
# Add to PATH
ln -s $BUILD_DIR/$PROGRAM_NAME ~/.local/bin/$PROGRAM_NAME

# Or use directly
$BUILD_DIR/$PROGRAM_NAME
\`\`\`

## Usage

\`\`\`bash
$PROGRAM_NAME --help
\`\`\`

## Features
$(jq -r '.features[]' "$DESIGN_DIR/design.json" | sed 's/^/- /')

## Requirements
Language: $LANGUAGE
$(cat "$DESIGN_DIR/requirements.md" | grep -A 10 "## Constraints" || echo "")

## Testing

\`\`\`bash
cd $BUILD_DIR/tests
./test_$PROGRAM_NAME.$EXT
\`\`\`

## Design

For detailed architecture and design decisions, see:
- Requirements: $DESIGN_DIR/requirements.md
- Architecture: $DESIGN_DIR/architecture.md

---
Built by build_program on $(date)
EOF

    success "Created $(path README.md)"

    # Step 4: Language-specific build files
    step "4/4" "Creating build configuration..."

    case "$LANGUAGE" in
        c)
            # Generate Makefile for C programs
            cat > "$BUILD_DIR/Makefile" <<MAKEFILE_EOF
CC = gcc
CFLAGS = -Wall -Werror -Wextra -std=c11

all: $PROGRAM_NAME

$PROGRAM_NAME: $PROGRAM_NAME.c
	\$(CC) \$(CFLAGS) -o $PROGRAM_NAME $PROGRAM_NAME.c

clean:
	rm -f $PROGRAM_NAME

test: $PROGRAM_NAME
	@cd tests && ./test_$PROGRAM_NAME.sh

.PHONY: all clean test
MAKEFILE_EOF
            success "Created $(path Makefile)"
            ;;
        python)
            # Could add setup.py, requirements.txt, etc.
            echo "# Python dependencies" > "$BUILD_DIR/requirements.txt"
            success "Created $(path requirements.txt)"
            ;;
    esac

    success "Code generation complete!"
}

# Phase 5: Test Program
test_program() {
    log_phase "Phase 5: Testing"
    info "Running generated tests..."

    cd "$BUILD_DIR/tests" || return 1

    if ./test_$PROGRAM_NAME.$EXT 2>&1; then
        echo ""
        success "Tests passed!"
        return 0
    else
        echo ""
        warning "Tests failed or incomplete"
        info "Review tests at: $(path $BUILD_DIR/tests/test_$PROGRAM_NAME.$EXT)"
        return 1
    fi
}

# Phase 6: Installation Offer
offer_installation() {
    banner_summary "Program Built Successfully!"

    echo -e "${BOLD}${WHITE}Program:${NC} $(path $PROGRAM_NAME)"
    echo -e "${BOLD}${WHITE}Location:${NC} $(path $BUILD_DIR)"
    echo -e "${BOLD}${WHITE}Language:${NC} $(arg $LANGUAGE)"

    list_header "Files created:"
    ls -lh "$BUILD_DIR" | tail -n +2 | grep -v '^d' | while read -r line; do
        filename=$(echo "$line" | awk '{print $9}')
        filesize=$(echo "$line" | awk '{print $5}')
        list_item "$filename" "$filesize"
    done

    echo ""
    list_header "Directories:"
    ls -lh "$BUILD_DIR" | tail -n +2 | grep '^d' | while read -r line; do
        dirname=$(echo "$line" | awk '{print $9}')
        list_item "$dirname/"
    done

    next_steps "Usage options" \
        "Direct: $(cmd $BUILD_DIR/$PROGRAM_NAME)" \
        "Install: $(cmd ln -s) $(path $BUILD_DIR/$PROGRAM_NAME) $(path ~/.local/bin/$PROGRAM_NAME)" \
        "Test: $(cmd $BUILD_DIR/tests/test_$PROGRAM_NAME.$EXT)"

    prompt "Install to ~/.local/bin now? (yes/no)"
    read -r INSTALL

    if [[ "$INSTALL" == "yes" ]]; then
        ln -sf "$BUILD_DIR/$PROGRAM_NAME" "$HOME/.local/bin/$PROGRAM_NAME"
        success "Installed: $(path ~/.local/bin/$PROGRAM_NAME)"
        echo ""
        echo -e "Ready to use anywhere: $(cmd $PROGRAM_NAME)"
    else
        info "Skipped installation. You can install later with:"
        show_command "" "ln -s $BUILD_DIR/$PROGRAM_NAME ~/.local/bin/$PROGRAM_NAME"
    fi
}

# Main execution
main() {
    banner_welcome "Welcome to Program Builder!" \
                   "AI-powered code generation from design documents"

    info "This workflow will:"
    list_pending "Load your design"
    list_pending "Ask clarifying questions"
    list_pending "Generate complete source code"
    list_pending "Create comprehensive tests"
    list_pending "Build documentation"

    echo ""

    # Load design (pass workflow arguments)
    load_design "$@"

    # Determine where to build
    determine_build_location "$@"

    # Ask any clarifying questions
    ask_clarifying_questions

    # Build the program
    build_program

    # Test it
    test_program || warning "Continue with manual testing"

    # Offer installation
    offer_installation

    success "Build workflow complete!"
}

# Run main with all arguments
main "$@"
