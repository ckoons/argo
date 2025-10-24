# Milestone 2: Architecture Conversation & Meta-Workflow - COMPLETE

## What We Built

A complete architecture design phase and meta-workflow orchestrator that demonstrates **phase hopping** and the full conversational development workflow.

### Components Created:

```
workflows/
├── lib/
│   ├── ci_onboarding.sh          # (from M1) CI workflow primer
│   ├── context-manager.sh        # (from M1) Context.json management
│   ├── state-manager.sh          # (existing) Session state management
│   └── logging.sh                # (existing) Logging utilities
├── phases/
│   ├── requirements_conversation.sh   # (from M1) Requirements phase
│   └── architecture_conversation.sh   # NEW: Architecture phase
├── tools/
│   ├── validate_requirements          # (from M1) DC tool
│   ├── generate_requirements_md       # (from M1) DC tool
│   ├── validate_architecture          # NEW: DC tool
│   ├── generate_architecture_md       # NEW: DC tool
│   └── generate_tasks                 # NEW: DC tool
├── develop_project.sh                 # NEW: Meta-workflow orchestrator
├── test_requirements_conversation.sh  # (from M1) Requirements test
└── test_architecture_conversation.sh  # NEW: Architecture test
```

## The Architecture in Action

### AI (CI) Responsibilities - Architecture Phase:
- **Conversation:** Discusses component breakdown, interfaces, technology choices
- **Gap detection:** Watches for statements that reveal requirement gaps
- **Hopping:** Can suggest going back to requirements if gaps detected
- **Convergence:** Proposes when architecture is complete

### Deterministic Code (DC) Responsibilities - Architecture Phase:
- **Validation:** Checks architecture completeness (components, requirements converged)
- **Generation:** Creates architecture.md from context.json
- **Task breakdown:** Generates tasks.json for build phase
- **State management:** Updates context.json atomically

### The Meta-Workflow Orchestrator:
- **Phase management:** Routes between requirements → architecture → build → review
- **Hopping support:** Allows backward navigation (architecture → requirements)
- **State persistence:** Resumes from current phase
- **User approval:** Gates major phase transitions

## Phase Hopping: The Key Innovation

### What is Phase Hopping?

When the CI detects a gap in an earlier phase, it can suggest "hopping back" to resolve it:

```
User: "Let's use React for the UI"
CI: "I notice you mentioned React (web framework), but the platform
     requirement doesn't specify React vs. Vue vs. vanilla JS.
     Should we hop back to requirements to clarify this?"

User: "yes"

→ System transitions from architecture → requirements
→ User refines requirements with React specification
→ System returns to architecture with updated context
```

### How Hopping Works:

1. **CI Detection** (architecture_conversation.sh:82-88):
   - CI watches for statements that reference undefined requirements
   - CI sets `next_phase: "requirements"` and provides `hop_reason`

2. **User Approval** (architecture_conversation.sh:114-132):
   - System asks user to confirm the hop
   - If approved, updates current_phase and returns "requirements"

3. **Meta-Workflow Routing** (develop_project.sh:89-99):
   - Meta-workflow sees next_phase != current_phase
   - Transitions to the new phase
   - Loads conversation context from previous phase

4. **Return to Architecture**:
   - After requirements updated, user approves
   - Meta-workflow transitions back to architecture
   - Architecture conversation resumes with updated requirements

## How to Test

### Test 1: Requirements Phase Only

```bash
cd /Users/cskoons/projects/github/argo/workflows
./test_requirements_conversation.sh calculator
```

**What happens:**
1. Creates new session (e.g., proj-E)
2. CI asks about platform, features, constraints
3. You describe your project
4. CI proposes convergence
5. DC validates and generates requirements.md
6. Shows next phase: architecture

### Test 2: Architecture Phase Only

```bash
cd /Users/cskoons/projects/github/argo/workflows
./test_architecture_conversation.sh calculator
```

**What happens:**
1. Creates session with pre-populated requirements
2. CI discusses architecture design
3. Try mentioning something not in requirements (to test hopping)
4. CI proposes convergence
5. DC validates, generates architecture.md and tasks.json
6. Shows task breakdown

### Test 3: Full Workflow with Hopping

```bash
cd /Users/cskoons/projects/github/argo/workflows
./develop_project.sh test_hop calculator
```

**Example conversation demonstrating hopping:**

```
PHASE: Requirements
==================
CI: What platform for your calculator?
You: web browser

CI: What operations?
You: basic math

CI: Requirements complete?
You: yes

→ Transition to architecture

PHASE: Architecture
==================
CI: Let's design the architecture. How should we structure this?
You: Use React with TypeScript

CI: I notice you mentioned React and TypeScript, but the requirements
    don't specify these technologies. The platform is "web" but that
    could be vanilla JS, React, Vue, etc. Should we hop back to
    requirements to document this choice?

You: yes

→ Transition back to requirements

PHASE: Requirements (resumed)
==================
CI: Let's clarify the technology requirements. What did you want to add?
You: React with TypeScript

CI: Updated requirements:
    - Platform: web (React + TypeScript)
    - Operations: add, subtract, multiply, divide

    Does this capture your vision?

You: yes

→ Transition back to architecture

PHASE: Architecture (resumed)
==================
CI: Great! Now let's design with React + TypeScript...
[Architecture conversation continues with complete context]
```

## The Files Generated

### Session directory structure:
```
~/.argo/sessions/test_hop/
├── context.json              # Complete conversation state
├── requirements.md           # Human-readable requirements
├── architecture.md           # Human-readable architecture
└── tasks.json               # Build task breakdown
```

### context.json contains:
```json
{
  "session_id": "test_hop",
  "project_name": "calculator",
  "current_phase": "build",
  "requirements": {
    "platform": "web (React + TypeScript)",
    "features": ["add", "subtract", "multiply", "divide"],
    "converged": true,
    "validated": true,
    "file": "~/.argo/sessions/test_hop/requirements.md"
  },
  "architecture": {
    "components": [
      {"name": "Calculator UI", "description": "React component"},
      {"name": "Math Engine", "description": "TypeScript calculation logic"}
    ],
    "technologies": ["React", "TypeScript", "CSS"],
    "converged": true,
    "validated": true,
    "file": "~/.argo/sessions/test_hop/architecture.md",
    "tasks_file": "~/.argo/sessions/test_hop/tasks.json"
  },
  "conversation": {
    "messages": [...],
    "decisions": [
      "Hopped back to requirements: Technology stack not specified",
      "Requirements converged and approved by user",
      "Architecture converged and approved by user"
    ]
  },
  "phase_history": ["requirements", "architecture", "requirements", "architecture", "build"]
}
```

### tasks.json contains:
```json
{
  "project_name": "calculator",
  "platform": "web (React + TypeScript)",
  "tasks": [
    {
      "id": "setup",
      "name": "Setup project structure",
      "type": "setup",
      "status": "pending"
    },
    {
      "id": "implement_calculator_ui",
      "name": "Implement Calculator UI",
      "type": "implementation",
      "component": "Calculator UI",
      "status": "pending",
      "depends_on": ["setup"]
    },
    {
      "id": "test_calculator_ui",
      "name": "Test Calculator UI",
      "type": "test",
      "status": "pending",
      "depends_on": ["implement_calculator_ui"]
    },
    {
      "id": "implement_math_engine",
      "name": "Implement Math Engine",
      "type": "implementation",
      "component": "Math Engine",
      "status": "pending",
      "depends_on": ["setup"]
    },
    {
      "id": "test_math_engine",
      "name": "Test Math Engine",
      "type": "test",
      "status": "pending",
      "depends_on": ["implement_math_engine"]
    },
    {
      "id": "integration",
      "name": "Integration testing",
      "type": "integration",
      "status": "pending",
      "depends_on": ["test_calculator_ui", "test_math_engine"]
    },
    {
      "id": "documentation",
      "name": "Generate documentation",
      "type": "documentation",
      "status": "pending",
      "depends_on": ["implement_calculator_ui", "implement_math_engine"]
    }
  ]
}
```

## What This Proves

✅ **Architecture phase works** - CI-driven architecture design conversation
✅ **Gap detection works** - CI recognizes requirement gaps during architecture
✅ **Phase hopping works** - Can navigate backward (architecture → requirements)
✅ **Context preservation** - Conversation state maintained across hops
✅ **DC task generation** - Deterministic breakdown into build tasks
✅ **Meta-workflow orchestration** - Simple bash can route complex workflows
✅ **User approval gates** - Major transitions require user confirmation

## Key Implementation Details

### Phase Hopping Pattern:

**Detection** (architecture_conversation.sh:78-88):
```bash
# CI prompt includes hopping instructions
ci_response=$(ci <<EOF
You are in ARCHITECTURE phase.
WATCH for requirement gaps - suggest hopping back if detected.
EOF
)
```

**Approval** (architecture_conversation.sh:114-132):
```bash
if [[ "$next_phase" == "requirements" ]]; then
    local hop_reason=$(echo "$ci_response" | jq -r '.metadata.hop_reason')
    warning "CI detected requirement gap: $hop_reason"

    read -r hop_approval
    if [[ "$hop_approval" =~ ^[Yy] ]]; then
        update_current_phase "$session_id" "requirements"
        echo "requirements"
        return 0
    fi
fi
```

**Routing** (develop_project.sh:89-99):
```bash
case "$current_phase" in
    requirements)
        next_phase=$("$PHASES_DIR/requirements_conversation.sh" "$session_id" "$project_name")
        current_phase="$next_phase"
        ;;
    architecture)
        next_phase=$("$PHASES_DIR/architecture_conversation.sh" "$session_id")
        current_phase="$next_phase"
        ;;
esac
```

### DC Tool Pattern (generate_tasks):

**Input:** context.json with architecture.components
**Process:**
- For each component: create implementation + test tasks
- Add setup task (dependencies for all)
- Add integration task (if multiple components)
- Add documentation task
- Generate dependency graph

**Output:** tasks.json with dependency-ordered tasks

### Atomic File Operations:

All DC tools use this pattern:
```bash
temp_file=$(mktemp)
jq '... updates ...' "$context_file" > "$temp_file"
mv "$temp_file" "$context_file"  # Atomic
```

## Testing the Individual Components

### Test DC tools:
```bash
# Create test session
session_id=$(create_session "test")
create_initial_context "$session_id" "test"
context_file="$(get_context_file $session_id)"

# Add test architecture
jq '.architecture.components = [
      {"name": "Component A", "description": "First component"},
      {"name": "Component B", "description": "Second component"}
    ] |
    .requirements.converged = true' \
   "$context_file" > "$context_file.tmp"
mv "$context_file.tmp" "$context_file"

# Validate
./tools/validate_architecture "$context_file"
echo "Validated: $(jq -r '.architecture.validated' $context_file)"

# Generate
./tools/generate_architecture_md "$context_file"
cat "$(jq -r '.architecture.file' $context_file)"

# Generate tasks
./tools/generate_tasks "$context_file"
jq -r '.tasks[] | "[\(.status)] \(.name)"' "$(jq -r '.architecture.tasks_file' $context_file)"
```

### Test meta-workflow navigation:
```bash
# Start fresh
./develop_project.sh my_session my_project

# Or resume
./develop_project.sh existing_session
```

## Architecture Decision Records

### Why Simple Bash for Meta-Workflow?

**Decision:** Use simple bash case statement for phase routing

**Rationale:**
- Phase transitions are deterministic (next_phase returned by phase scripts)
- No complex state machine needed - just a loop with case statement
- Composable - each phase script is independent
- Debuggable - easy to trace phase transitions
- Extensible - add new phases by adding cases

**Alternative considered:** State machine library or framework
**Rejected because:** Added complexity without benefit for our simple routing needs

### Why User Approval for Hopping?

**Decision:** Ask user to confirm phase hops

**Rationale:**
- User may not agree with CI's gap detection
- Hopping interrupts the current conversation flow
- User should control when to refine earlier phases
- Transparency - user sees why CI wants to hop

**Alternative considered:** Automatic hopping
**Rejected because:** Loses user agency, could be disruptive

### Why Separate DC Tools vs. Integrated?

**Decision:** Three separate DC tools (validate, generate_md, generate_tasks)

**Rationale:**
- Each tool has single responsibility
- Testable in isolation
- Reusable (generate_tasks could be called from other workflows)
- Clear contracts (input: context.json, output: specific file + context update)

**Alternative considered:** Single monolithic architecture_processor
**Rejected because:** Violates Unix philosophy, harder to test and maintain

## Next Steps (Future Milestones)

### Milestone 3: Build Integration
- Connect tasks.json to existing build_program workflow
- Execute builders in parallel based on task dependencies
- Report build progress back to meta-workflow
- Handle build failures and retry logic

### Milestone 4: Review & Iteration
- Implement review phase conversation
- Route feedback to appropriate phase (requirements/architecture/build)
- Support multiple iteration cycles
- Track iteration history in context.json

### Milestone 5: Advanced Features
- Multi-session support (work on multiple projects)
- Session templates (start from previous successful project)
- Diff view (show changes between iterations)
- Export/import sessions
- CI performance optimization (cache context, streaming)

## Files to Review

**Most important:**
- `phases/architecture_conversation.sh` - See phase hopping in action
- `tools/generate_tasks` - See task breakdown logic
- `develop_project.sh` - See meta-workflow orchestration

**Run the tests:**
- `./test_architecture_conversation.sh` - Try architecture phase
- `./develop_project.sh test_hop calculator` - Try full workflow with hopping

## Success Criteria ✅

- [x] Architecture phase conversation works
- [x] CI detects requirement gaps during architecture
- [x] Phase hopping works (architecture → requirements → architecture)
- [x] DC validates architecture deterministically
- [x] DC generates architecture.md
- [x] DC generates tasks.json with dependency graph
- [x] Meta-workflow orchestrates phase transitions
- [x] User approval gates phase hops
- [x] Context persists across phase hops
- [x] State can be resumed at any phase
- [x] Clean separation maintained (AI creativity, DC precision)

**The full conversational workflow is working!**

Requirements ⟷ Architecture → Build → Review

Ready for Milestone 3: Build integration.
