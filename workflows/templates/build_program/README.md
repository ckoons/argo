# build_program

Build a complete program from design documents with AI code generation.

## Description

This workflow reads design documents created by `design_program` and generates complete, working code with tests and documentation. The AI analyzes the design, asks clarifying questions if needed, and produces production-ready code.

## Purpose

- Generate code from design documents
- Create comprehensive test suites
- Produce documentation automatically
- Build deployable programs

## Usage

```bash
# Build from existing design
arc start build_program my_build calculator

# Interactive mode (prompts for design name and location)
arc start build_program my_build

# Specify custom build location
arc start build_program my_build calculator ~/projects/calculator
```

## Prerequisites

Must have a design created by `design_program`:

```bash
# First, create a design
arc start design_program calc_design

# Then, build it
arc start build_program calc_build calculator
```

## Workflow Phases

### Phase 1: Load Design
- Reads design from `~/.argo/designs/{program_name}/`
- Validates design.json exists
- Extracts language, purpose, requirements

### Phase 2: Determine Build Location
- Default: `~/.argo/tools/{program_name}/`
- Prompts for custom location if desired
- Creates directory structure
- Handles existing directories (overwrite confirmation)

### Phase 3: Pre-Build Questions
- AI analyzes the design
- Identifies ambiguities or missing details
- Asks up to 3 clarifying questions
- Records Q&A in conversation.log

### Phase 4: Build Program
Four-step build process:

1. **Generate source code**
   - Complete implementation in specified language
   - Proper copyright headers
   - Comprehensive error handling
   - Best practices followed

2. **Generate test suite**
   - Tests for all features
   - Positive and negative test cases
   - Validates success criteria
   - Standalone executable tests

3. **Generate documentation**
   - README with usage instructions
   - Installation options
   - Feature list
   - Testing instructions

4. **Create build configuration**
   - Makefile (for C programs)
   - requirements.txt (for Python)
   - Language-specific tooling

### Phase 5: Test Program
- Automatically runs generated tests
- Reports pass/fail status
- Continues even if tests need work

### Phase 6: Installation Offer
- Shows summary of created files
- Offers to install to `~/.local/bin/`
- Provides usage instructions

## Output Structure

```
~/.argo/tools/{program_name}/
├── {program_name}           # Symlink (for convenience)
├── {program_name}.{ext}     # Main program
├── tests/
│   └── test_{program_name}.{ext}
├── README.md
└── Makefile (if C) or requirements.txt (if Python)
```

## Supported Languages

- **Python** (.py)
  - Generates .py file
  - Creates requirements.txt
  - Python 3 compatible

- **Bash** (.sh)
  - Generates .sh file
  - Includes proper shebang
  - Portable bash script

- **C** (.c)
  - Generates .c file
  - Creates Makefile
  - gcc compatible

- **Auto**: Defaults to bash

## Examples

### Example 1: Build Calculator
```bash
# After designing with design_program
arc start build_program calc_build calculator

# AI generates:
# - calculator.py (if Python was selected)
# - tests/test_calculator.py
# - README.md
# - requirements.txt

# Install and use
ln -s ~/.argo/tools/calculator/calculator ~/.local/bin/
calculator 5 + 3
```

### Example 2: Build Todo Tool
```bash
arc start build_program todo_build todo ~/tools/todo

# Builds in custom location: ~/tools/todo/
# Install
ln -s ~/tools/todo/todo ~/.local/bin/
todo add "Write documentation"
todo list
```

## Integration with design_program

```bash
# Complete flow
arc start design_program my_design      # Design phase
arc start build_program my_build my_program  # Build phase

# The design is preserved in ~/.argo/designs/
# The program is built in ~/.argo/tools/ (or custom location)
# You can rebuild from the same design multiple times
```

## Error Handling

- **Design not found**: Lists available designs, suggests creating one
- **Code generation fails**: Aborts with error message
- **Test generation fails**: Creates placeholder, continues build
- **Tests fail**: Reports failure, continues to installation offer
- **Build directory exists**: Prompts for overwrite confirmation

## Tips

- Review the design before building (`cat ~/.argo/designs/{name}/architecture.md`)
- AI asks questions if design is ambiguous - answer for better results
- Tests are generated but may need refinement
- Default location (`~/.argo/tools/`) keeps programs organized
- Can rebuild to different location anytime

## Dependencies

- `ci` tool (for AI code generation)
- `jq` (for JSON processing)
- Argo daemon running
- Existing design from design_program

## Author

Created by Casey Koons on 2025-10-19

---
Part of the Argo workflow system
