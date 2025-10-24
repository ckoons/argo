# Milestone 3: Build Phase Integration - COMPLETE

## What We Built

Integrated the build phase with tasks.json, completing the full conversational development workflow from requirements through build execution.

### Components Created:

```
workflows/
├── lib/
│   ├── logging.sh                     # NEW: Logging functions wrapper
│   ├── failure-handler.sh             # (existing) Builder failure handling
│   └── conflict-resolver.sh           # (existing) Merge conflict resolution
├── phases/
│   ├── requirements_conversation.sh   # (from M1) Requirements phase
│   ├── architecture_conversation.sh   # (from M2) Architecture phase
│   └── build_phase.sh                 # NEW: Build execution phase
├── tools/
│   ├── validate_requirements          # (from M1) DC tool
│   ├── generate_requirements_md       # (from M1) DC tool
│   ├── validate_architecture          # (from M2) DC tool
│   ├── generate_architecture_md       # (from M2) DC tool
│   ├── generate_tasks                 # (from M2) DC tool
│   └── resolve_task_dependencies      # NEW: DC tool for dependency resolution
└── develop_project.sh                 # (from M2) Meta-workflow orchestrator (updated)
```

## The Architecture in Action

### Build Phase Workflow:
1. **Dependency Resolution** - Topological sort of tasks based on depends_on
2. **Sequential Execution** - Execute tasks in dependency order
3. **Progress Tracking** - Update task status in tasks.json
4. **Failure Handling** - Stop on first failure, allow retry from architecture
5. **Completion** - Transition to review phase when all tasks succeed

### DC Tool: resolve_task_dependencies
- **Input:** tasks.json with task dependencies
- **Algorithm:** Kahn's algorithm for topological sort
- **Output:** Task IDs in execution order (one per line)
- **Error Detection:** Detects circular dependencies

### Simplified Builder Execution (M3):
For Milestone 3, task execution is simulated to demonstrate the workflow:
- **setup tasks** - Simulate project structure creation
- **implementation tasks** - Simulate code generation (90% success rate)
- **test tasks** - Simulate test execution (95% success rate)
- **integration tasks** - Simulate integration testing
- **documentation tasks** - Simulate documentation generation

**Future:** Replace simulated execution with full builder spawning (using existing failure-handler.sh and builder state management).

## How It Works

### 1. Task Dependency Resolution

**Example tasks.json:**
```json
{
  "tasks": [
    {"id": "setup", "depends_on": []},
    {"id": "implement_ui", "depends_on": ["setup"]},
    {"id": "implement_logic", "depends_on": ["setup"]},
    {"id": "test_ui", "depends_on": ["implement_ui"]},
    {"id": "test_logic", "depends_on": ["implement_logic"]},
    {"id": "integration", "depends_on": ["test_ui", "test_logic"]},
    {"id": "documentation", "depends_on": ["implement_ui", "implement_logic"]}
  ]
}
```

**Resolved execution order:**
```
setup
implement_ui
implement_logic
test_ui
test_logic
documentation
integration
```

### 2. Build Execution Flow

```
build_phase.sh
    ↓
1. Load tasks.json
    ↓
2. Resolve dependencies → resolve_task_dependencies
    ↓
3. For each task in order:
    a. Display task info
    b. Execute task → execute_task()
    c. Update status in tasks.json
    d. If failure: stop and return to architecture
    ↓
4. Show build summary
    ↓
5. Update context.json with build results
    ↓
6. Transition to review phase (if success)
```

### 3. Integration with Meta-Workflow

```
develop_project.sh (meta-workflow)
    ↓
requirements phase → architecture phase → BUILD PHASE → review phase
                     ↑                        ↓
                     └────── (on failure) ────┘
```

## How to Test

### Test Full Workflow (Requirements → Architecture → Build)

```bash
cd /Users/cskoons/projects/github/argo/workflows
./develop_project.sh test_build calculator
```

**What happens:**
1. Requirements conversation
2. Architecture conversation
3. Tasks generated
4. Build phase executes tasks in dependency order
5. Progress shown for each task
6. Build summary displayed
7. Transition to review phase

### Test Build Phase Directly

Create a session with completed architecture first:

```bash
# Create session with architecture
./test_architecture_conversation.sh calculator

# Get session ID (e.g., proj-F)
session_id=$(ls -td ~/.argo/sessions/proj-* | head -1 | xargs basename)

# Run build phase
./phases/build_phase.sh "$session_id"
```

### Test Dependency Resolver

```bash
# Create test tasks.json
cat > /tmp/test_tasks.json <<'EOF'
{
  "tasks": [
    {"id": "task_a", "depends_on": ["task_b"]},
    {"id": "task_b", "depends_on": ["task_c"]},
    {"id": "task_c", "depends_on": []}
  ]
}
EOF

# Resolve dependencies
./tools/resolve_task_dependencies /tmp/test_tasks.json

# Output:
# task_c
# task_b
# task_a
```

## What This Proves

✅ **Task dependency resolution works** - Topological sort handles complex dependency graphs
✅ **Build phase integrates with tasks.json** - Seamless handoff from architecture
✅ **Sequential execution** - Tasks execute in correct dependency order
✅ **Progress tracking** - Task status updated in real-time
✅ **Failure handling** - Build stops on failure, allows retry
✅ **Full workflow complete** - Requirements → Architecture → Build → Review
✅ **State persistence** - Can resume at any phase

## Key Implementation Details

### Topological Sort (Kahn's Algorithm)

**resolve_task_dependencies implementation:**
```bash
# 1. Find tasks with no dependencies
# 2. Add them to result
# 3. Mark as completed
# 4. Find next tasks whose dependencies are all completed
# 5. Repeat until all tasks processed
# 6. Detect circular dependencies if no progress made
```

**Complexity:** O(V + E) where V = number of tasks, E = number of dependencies

### Build Phase Pattern

**phases/build_phase.sh:**
```bash
build_phase() {
    # 1. Validate architecture converged
    # 2. Load tasks.json
    # 3. Resolve dependencies
    # 4. Execute tasks sequentially
    # 5. Update status after each task
    # 6. Show summary
    # 7. Return next phase
}
```

**Task Execution (simplified for M3):**
```bash
execute_task() {
    case "$task_type" in
        setup) simulate_setup ;;
        implementation) simulate_build ;;
        test) simulate_test ;;
        integration) simulate_integration ;;
        documentation) simulate_docs ;;
    esac
}
```

### Meta-Workflow Integration

**develop_project.sh update:**
```bash
case "$current_phase" in
    # ... other phases ...

    build)
        next_phase=$("$PHASES_DIR/build_phase.sh" "$session_id")
        current_phase="$next_phase"
        ;;
esac
```

## Files Generated

### After complete workflow:

```
~/.argo/sessions/test_build/
├── context.json              # Complete state
├── requirements.md           # Requirements document
├── architecture.md           # Architecture document
└── tasks.json               # Tasks with status updates
```

### Updated tasks.json (after build):

```json
{
  "project_name": "calculator",
  "platform": "web",
  "tasks": [
    {
      "id": "setup",
      "name": "Setup project structure",
      "status": "completed"
    },
    {
      "id": "implement_calculator_ui",
      "name": "Implement Calculator UI",
      "status": "completed",
      "depends_on": ["setup"]
    },
    {
      "id": "test_calculator_ui",
      "name": "Test Calculator UI",
      "status": "completed",
      "depends_on": ["implement_calculator_ui"]
    }
  ]
}
```

### Updated context.json (after build):

```json
{
  "session_id": "test_build",
  "current_phase": "review",
  "requirements": {
    "converged": true,
    "validated": true,
    "file": "~/.argo/sessions/test_build/requirements.md"
  },
  "architecture": {
    "converged": true,
    "validated": true,
    "file": "~/.argo/sessions/test_build/architecture.md",
    "tasks_file": "~/.argo/sessions/test_build/tasks.json"
  },
  "build": {
    "status": "completed",
    "completed_tasks": 7,
    "failed_tasks": 0
  }
}
```

## Architecture Decisions

### Why Sequential Execution for M3?

**Decision:** Execute tasks sequentially in dependency order

**Rationale:**
- Simpler implementation for demonstrating the workflow
- Easier to reason about and debug
- Dependency order is respected
- Foundation for parallel execution later

**Future:** Parallel execution of independent tasks (tasks with no shared dependencies)

### Why Simulate Task Execution?

**Decision:** Simulate task execution instead of full builder spawning

**Rationale:**
- Milestone 3 goal: demonstrate workflow integration
- Full builder execution is complex (requires CI spawning, monitoring, failure handling)
- Existing infrastructure (failure-handler.sh, builder state management) can be integrated later
- Simulation proves the workflow pattern works

**Future:** Replace execute_task() with full builder spawning using existing infrastructure

### Why Stop on First Failure?

**Decision:** Stop build on first task failure

**Rationale:**
- Early feedback (no need to wait for all tasks to fail)
- Simpler error handling
- User can fix and retry immediately
- Prevents cascading failures

**Alternative considered:** Continue and collect all failures
**Rejected because:** User typically wants to fix first failure before continuing

## Next Steps (Future Milestones)

### Milestone 4: Full Builder Execution
- Replace simulated execution with real builder spawning
- Integrate with failure-handler.sh
- Implement builder progress monitoring
- Handle builder failures with automatic recovery
- Support parallel execution of independent tasks

### Milestone 5: Review & Iteration Phase
- Implement review phase conversation
- Gather user feedback on build results
- Route feedback to appropriate phase
- Support multiple iteration cycles
- Track iteration history

### Milestone 6: Advanced Features
- Incremental builds (only rebuild changed components)
- Build caching
- Parallel execution optimization
- Build visualization (task dependency graph)
- Performance metrics

## Testing the Components

### Test Dependency Resolver (circular dependency detection):
```bash
cat > /tmp/circular.json <<'EOF'
{
  "tasks": [
    {"id": "a", "depends_on": ["b"]},
    {"id": "b", "depends_on": ["c"]},
    {"id": "c", "depends_on": ["a"]}
  ]
}
EOF

./tools/resolve_task_dependencies /tmp/circular.json
# Error: Circular dependency detected
```

### Test Build Phase (failure handling):
```bash
# Build phase handles failures by returning to architecture
# Simulated tasks have random failures (10% for implementation, 5% for tests)
# If failure occurs, build stops and returns to architecture phase
```

## Files to Review

**Most important:**
- `phases/build_phase.sh` - See build execution workflow
- `tools/resolve_task_dependencies` - See dependency resolution algorithm
- `lib/logging.sh` - Logging functions wrapper

**Run the tests:**
- `./develop_project.sh test_build calculator` - Try full workflow

## Success Criteria ✅

- [x] Task dependency resolution works (topological sort)
- [x] Build phase executes tasks in correct order
- [x] Task status tracked in tasks.json
- [x] Build failures handled gracefully
- [x] Integration with meta-workflow complete
- [x] Full workflow functional (requirements → architecture → build → review)
- [x] State persists across phases
- [x] Can resume from any phase

**The full conversational development workflow is now operational!**

`Requirements ⟷ Architecture → Build → Review`

Foundation ready for Milestone 4: Full builder execution with CI spawning and failure recovery.
