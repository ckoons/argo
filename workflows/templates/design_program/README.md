# design_program

Interactive program design workflow with AI assistance.

## Description

This workflow helps you design programs through an interactive dialog with AI. It gathers requirements, proposes architecture, and creates comprehensive design documents that can be used by `build_program` to generate the actual code.

## Purpose

- Design programs collaboratively with AI
- Create structured design documents
- Prepare for automated code generation
- Iterate on design before implementation

## Usage

```bash
# Interactive mode (recommended)
arc start design_program my_design

# The workflow will:
# 1. Ask for program name
# 2. Gather requirements (purpose, users, language, features)
# 3. AI proposes architecture
# 4. You review and refine
# 5. Save complete design documents
```

## Output

Creates design documents in `~/.argo/designs/{program_name}/`:

- **requirements.md** - What the program should do
- **architecture.md** - How it should be built (AI-proposed)
- **design.json** - Structured data for build_program

## Workflow Phases

### Phase 1: Program Identity
- Name the program
- Check for existing designs
- Option to load/overwrite/rename

### Phase 2: Requirements Gathering
- Problem statement
- Target users
- Programming language
- Key features
- Success criteria
- Constraints

### Phase 3: Design Dialog
- AI analyzes requirements
- Proposes detailed architecture covering:
  - Architecture overview
  - Component breakdown
  - Data structures
  - Interface design
  - Error handling
  - Testing strategy
  - File structure
  - Implementation steps
- User can approve, reject, or refine
- Iterative refinement supported

## Design Document Structure

### requirements.md
Human-readable requirements in markdown format.

### architecture.md
AI-proposed architecture with 8 key sections providing implementation guidance.

### design.json
Machine-readable structured design:
```json
{
  "program_name": "example",
  "purpose": "...",
  "language": "python",
  "features": [...],
  "success_criteria": "...",
  "design_complete": true,
  "build_location": "~/.argo/tools/example"
}
```

## Next Steps

After completing the design:

1. **Review the design**:
   ```bash
   cat ~/.argo/designs/{program_name}/architecture.md
   ```

2. **Build the program**:
   ```bash
   arc start build_program {program_name}
   ```

3. **Or refine the design**:
   ```bash
   arc start design_program {program_name}
   # Choose "load" to continue iterating
   ```

## Examples

### Example 1: CLI Calculator
```bash
arc start design_program calc_design

# Answers:
# Name: calculator
# Problem: Perform basic arithmetic operations
# Users: Personal use
# Language: python
# Features: add, subtract, multiply, divide
# Success: Handles basic math with error checking
```

### Example 2: Todo List Tool
```bash
arc start design_program todo_design

# Answers:
# Name: todo
# Problem: Track tasks from command line
# Users: Personal productivity
# Language: bash
# Features: add task, list tasks, complete task, delete task
# Success: Persistent storage, simple interface
```

## Tips

- Be specific in requirements (helps AI design better)
- Review architecture carefully before approving
- Use "refine" to iterate on design
- Language choice affects what AI generates in build_program
- Designs are saved - you can review/update anytime

## Dependencies

- `ci` tool (for AI assistance)
- `jq` (for JSON processing)
- Argo daemon running

## Author

Created by Casey Koons on 2025-10-19

---
Part of the Argo workflow system
