# Milestone 1: Requirements Conversation - COMPLETE

## What We Built

A complete conversational requirements gathering system that demonstrates the **AI + Deterministic Code** architecture.

### Components Created:

```
workflows/
├── lib/
│   ├── ci_onboarding.sh          # CI workflow primer (system prompt)
│   ├── context-manager.sh        # Context.json management functions
│   ├── state-manager.sh          # (existing) Session state management
│   └── logging.sh                # (existing) Logging utilities
├── phases/
│   └── requirements_conversation.sh  # Main conversational workflow
├── tools/
│   ├── validate_requirements     # DC tool: validate requirements
│   └── generate_requirements_md  # DC tool: generate requirements.md
└── test_requirements_conversation.sh  # End-to-end test script
```

## The Architecture in Action

### AI (CI) Responsibilities:
- **Conversation:** Asks clarifying questions, understands user intent
- **Routing:** Decides when to stay in requirements vs. move to architecture
- **Gap detection:** Recognizes when user mentions something from another phase
- **Convergence:** Proposes when requirements are complete

### Deterministic Code (DC) Responsibilities:
- **Validation:** Checks requirements completeness (platform, features)
- **Generation:** Creates requirements.md from context.json
- **State management:** Updates context.json atomically

### The Shared State (context.json):
- **CI writes:** requirements.platform, requirements.features, conversation.messages
- **DC writes:** requirements.validated, requirements.validation_errors, requirements.file
- **Both read:** Full context for decision-making

## How to Test

### Run the test:
```bash
cd /Users/cskoons/projects/github/argo/workflows
./test_requirements_conversation.sh calculator
```

### What happens:
1. Creates a new session (e.g., proj-D)
2. CI greets you and starts asking questions
3. You describe your calculator project
4. CI updates context.json with your responses
5. When converged, CI shows summary and asks approval
6. DC validates requirements
7. DC generates requirements.md
8. Shows next phase (architecture)

### Example conversation:
```
CI: Great! I'd love to help you build a calculator. Let me ask a few questions:
    1. What platform? (web, desktop, mobile, CLI)
    2. What operations? (basic, scientific, financial)
    3. Any constraints?

You: Web browser, basic math operations

CI: Perfect! Which operations specifically? (add, subtract, multiply, divide)
    Should it handle decimals or just integers?

You: Add, subtract, multiply, divide with decimals

CI: Got it! Any design preferences or target users?

You: Simple, clean design for students

CI: Requirements Summary:
    - Platform: web
    - Operations: add, subtract, multiply, divide
    - Number type: decimal/floating point
    - Target: students
    - Design: simple, clean

    Does this capture your vision?

You: yes

✓ Requirements complete! requirements.md generated
→ Next phase: architecture
```

## The Files Generated

### Session directory structure:
```
~/.argo/sessions/proj-D/
├── context.json           # Complete conversation state
├── requirements.md        # Human-readable requirements
└── builders/             # (created in build phase)
```

### context.json contains:
```json
{
  "session_id": "proj-D",
  "project_name": "calculator",
  "current_phase": "architecture",
  "requirements": {
    "platform": "web",
    "features": ["add", "subtract", "multiply", "divide"],
    "constraints": {"number_type": "decimal", "target": "students"},
    "converged": true,
    "validated": true,
    "file": "~/.argo/sessions/proj-D/requirements.md"
  },
  "conversation": {
    "messages": [...],
    "decisions": [...]
  }
}
```

## What This Proves

✅ **CI can drive conversation** - Natural language interaction
✅ **CI can make routing decisions** - Knows when to stay/hop
✅ **DC tools work deterministically** - Validation and generation
✅ **JSON context works** - Both CI and DC update it
✅ **Convergence with user approval** - CI proposes, user approves
✅ **State persists** - Can resume conversation later

## Testing the Individual Components

### Test CI onboarding:
```bash
source lib/ci_onboarding.sh
get_workflow_primer | head -20
```

### Test context creation:
```bash
source lib/state-manager.sh
source lib/context-manager.sh
session_id=$(create_session "test_project")
create_initial_context "$session_id" "test_project"
cat "$(get_context_file $session_id)" | jq
```

### Test DC tools:
```bash
# Create test context
session_id=$(create_session "test")
create_initial_context "$session_id" "test"
context_file="$(get_context_file $session_id)"

# Update with test data
jq '.requirements.platform = "web" | .requirements.features = ["calc"]' \
   "$context_file" > "$context_file.tmp"
mv "$context_file.tmp" "$context_file"

# Validate
./tools/validate_requirements "$context_file"
echo "Validated: $(jq -r '.requirements.validated' $context_file)"

# Generate
./tools/generate_requirements_md "$context_file"
cat "$(jq -r '.requirements.file' $context_file)"
```

## Key Implementation Details

### Stateless CI Pattern:
- Each CI call loads full context from context.json
- No session state in daemon (yet)
- Simpler, more robust, easier to debug
- Can optimize later if needed

### Thread Safety:
- DC tools use atomic mv (write to temp, then move)
- Context updates are file-level (whole file replaced)
- CI and DC rarely write simultaneously (CI waits for DC)
- No locks needed currently

### Error Handling:
- DC validation failures → CI discusses with user
- CI response parsing errors → Ask user to rephrase
- Missing files → Create fresh context
- Invalid JSON → Report error and exit

## Next Steps (Milestone 2)

To complete the full workflow loop:

1. **Architecture phase workflow** (similar to requirements)
   - Conversation about components, interfaces
   - DC tools: validate_architecture, generate_architecture_md, generate_tasks
   - Can hop back to requirements if gaps found

2. **Meta-workflow orchestrator**
   - Simple bash script that calls phases
   - Handles phase transitions
   - Shows the "hopping" in action

3. **Integration with build_program**
   - Architecture phase generates tasks.json
   - Hands off to existing build_program workflow
   - Proves end-to-end flow

4. **Review phase**
   - Present build results
   - Gather feedback
   - Route based on feedback type

## Files to Review

**Most important:**
- `lib/ci_onboarding.sh` - See how we "teach" the CI
- `phases/requirements_conversation.sh` - See the conversational loop
- `lib/context-manager.sh` - See JSON state management

**Run the test:**
- `./test_requirements_conversation.sh` - Try the full flow

## Success Criteria ✅

- [x] CI understands the workflow system
- [x] CI can have natural conversation
- [x] CI updates context.json with requirements
- [x] DC validates requirements deterministically
- [x] DC generates requirements.md
- [x] User approval gates phase transition
- [x] State persists across restarts
- [x] Clean separation of AI (creativity) and DC (precision)

**The pattern works!** Ready for Milestone 2.
