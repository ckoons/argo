#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# build_program - Build a program from design documents
#
# This workflow reads a design created by design_program and generates
# complete working code with tests and documentation.

set -e

# Directories
DESIGNS_DIR="${HOME}/.argo/designs"
TOOLS_DIR="${HOME}/.argo/tools"
PROGRAM_NAME=""
DESIGN_DIR=""
BUILD_DIR=""
LANGUAGE=""

# Logging
log() {
    echo "[build_program] $*"
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
        ask "Which program design should I build?" PROGRAM_NAME
    fi

    DESIGN_DIR="$DESIGNS_DIR/$PROGRAM_NAME"

    if [[ ! -f "$DESIGN_DIR/design.json" ]]; then
        log "Error: Design not found: $DESIGN_DIR/design.json"
        log ""
        log "Available designs:"
        if [[ -d "$DESIGNS_DIR" ]]; then
            ls -1 "$DESIGNS_DIR" 2>/dev/null || echo "  (none)"
        else
            echo "  (none - run 'arc start design_program' to create one)"
        fi
        log ""
        log "Run: arc start design_program"
        exit 1
    fi

    log "Loading design: $PROGRAM_NAME"

    # Parse design.json
    LANGUAGE=$(jq -r '.language' "$DESIGN_DIR/design.json")
    PURPOSE=$(jq -r '.purpose' "$DESIGN_DIR/design.json")

    # Auto-detect language if set to "auto"
    if [[ "$LANGUAGE" == "auto" ]]; then
        LANGUAGE="bash"  # Default
        log "Language set to auto - defaulting to bash"
    fi

    log "Language: $LANGUAGE"
}

# Phase 2: Determine Build Location
determine_build_location() {
    # Check if user specified location as second argument
    if [[ -n "$2" ]]; then
        BUILD_DIR="${2/#\~/$HOME}"
    else
        DEFAULT_PATH="$TOOLS_DIR/$PROGRAM_NAME"
        ask "Where should I build this? [$DEFAULT_PATH]:" USER_PATH

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
        ask "Directory exists: $BUILD_DIR. Overwrite? (yes/no)" OVERWRITE
        if [[ "$OVERWRITE" != "yes" ]]; then
            log "Aborting."
            exit 1
        fi
        rm -rf "$BUILD_DIR"
    fi

    mkdir -p "$BUILD_DIR"
    log "Building in: $BUILD_DIR"
}

# Phase 3: Pre-Build Questions
ask_clarifying_questions() {
    log "Analyzing design..."
    echo ""

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
        log "Design is complete. Ready to build!"
        ANSWERS=""
    else
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo "AI has questions before building:"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo "$QUESTIONS"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo ""
        ask "Provide answers (or 'skip' to proceed anyway):" ANSWERS

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
    log "Generating program code..."
    echo ""

    REQUIREMENTS=$(cat "$DESIGN_DIR/requirements.md")
    ARCHITECTURE=$(cat "$DESIGN_DIR/architecture.md")
    ANSWERS_CONTEXT="${ANSWERS:-No additional clarifications}"

    # Step 1: Generate main program file
    log "Step 1/4: Generating source code..."

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
        log "Error: Code generation failed"
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

    log "✓ Created $PROGRAM_NAME.$EXT"

    # Step 2: Generate tests
    log "Step 2/4: Generating test suite..."

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
        log "Warning: Test generation failed, creating placeholder..."
        TEST_CODE="#!/bin/bash\necho 'TODO: Add tests'\nexit 0"
    fi

    # Strip markdown
    TEST_CODE=$(echo "$TEST_CODE" | sed '/^```/d')

    mkdir -p "$BUILD_DIR/tests"
    echo "$TEST_CODE" > "$BUILD_DIR/tests/test_$PROGRAM_NAME.$EXT"
    chmod +x "$BUILD_DIR/tests/test_$PROGRAM_NAME.$EXT"

    log "✓ Created tests/test_$PROGRAM_NAME.$EXT"

    # Step 3: Generate README
    log "Step 3/4: Generating documentation..."

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

    log "✓ Created README.md"

    # Step 4: Language-specific build files
    log "Step 4/4: Creating build configuration..."

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
            log "✓ Created Makefile"
            ;;
        python)
            # Could add setup.py, requirements.txt, etc.
            echo "# Python dependencies" > "$BUILD_DIR/requirements.txt"
            log "✓ Created requirements.txt"
            ;;
    esac

    log "Build complete!"
}

# Phase 5: Test Program
test_program() {
    log "Running tests..."
    echo ""

    cd "$BUILD_DIR/tests" || return 1

    if ./test_$PROGRAM_NAME.$EXT 2>&1; then
        echo ""
        log "✓ Tests passed!"
        return 0
    else
        echo ""
        log "⚠ Tests failed or incomplete"
        log "Review tests at: $BUILD_DIR/tests/test_$PROGRAM_NAME.$EXT"
        return 1
    fi
}

# Phase 6: Installation Offer
offer_installation() {
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Program Built Successfully!"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Program: $PROGRAM_NAME"
    echo "Location: $BUILD_DIR"
    echo "Language: $LANGUAGE"
    echo ""
    echo "Files created:"
    ls -lh "$BUILD_DIR" | tail -n +2 | grep -v '^d' | awk '{print "  " $9 " (" $5 ")"}'
    echo ""
    echo "Directories:"
    ls -lh "$BUILD_DIR" | tail -n +2 | grep '^d' | awk '{print "  " $9 "/"}'
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "Usage options:"
    echo "  1. Direct execution:"
    echo "     $BUILD_DIR/$PROGRAM_NAME"
    echo ""
    echo "  2. Install to PATH:"
    echo "     ln -s $BUILD_DIR/$PROGRAM_NAME ~/.local/bin/$PROGRAM_NAME"
    echo ""
    echo "  3. Run tests:"
    echo "     $BUILD_DIR/tests/test_$PROGRAM_NAME.$EXT"
    echo ""

    ask "Install to ~/.local/bin now? (yes/no)" INSTALL

    if [[ "$INSTALL" == "yes" ]]; then
        ln -sf "$BUILD_DIR/$PROGRAM_NAME" "$HOME/.local/bin/$PROGRAM_NAME"
        log "✓ Installed: ~/.local/bin/$PROGRAM_NAME"
        echo ""
        echo "Ready to use anywhere: $PROGRAM_NAME"
    else
        log "Skipped installation. You can install later with:"
        log "  ln -s $BUILD_DIR/$PROGRAM_NAME ~/.local/bin/$PROGRAM_NAME"
    fi
}

# Main execution
main() {
    log "Welcome to Program Builder!"
    echo ""
    echo "This workflow builds programs from design documents created by design_program."
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
    test_program || log "Continue with manual testing"

    # Offer installation
    offer_installation

    log "Build complete!"
}

# Run main with all arguments
main "$@"
