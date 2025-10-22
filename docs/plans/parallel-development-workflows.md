# Parallel Development Workflows

© 2025 Casey Koons. All rights reserved.

**Status**: Design Complete - Ready for Implementation
**Author**: Casey Koons with Claude Code
**Date**: 2025-10-21

## Executive Summary

This document describes a multi-tier workflow system for Argo that enables AI-assisted software development with parallel builder support, automated testing, intelligent merge conflict resolution, and GitHub integration.

**Key Innovation**: Parent CI orchestrates multiple independent Builder CIs working on feature branches in parallel, then intelligently merges results with automated conflict resolution.

## System Architecture

### Tier 1: Project Intake & Setup
**Workflow**: `project_intake`
**Purpose**: Deterministic question flow to classify project type and initialize workspace

### Tier 2: Design & Requirements
**Workflow**: `design_program` (enhanced)
**Purpose**: AI-assisted requirements gathering and architecture design

### Tier 3: Build Execution
**Workflows**: `build_tools`, `build_program` (unified)
**Purpose**: Code generation with optional parallel builders

### Tier 4: Parallel Orchestration
**Component**: Builder CI Loop (background processes)
**Purpose**: Independent code generation with test-driven development

## Project Type Classification

### Type 1: Tool
- **Definition**: Single-file standalone utility
- **Location**: `~/.argo/tools/{name}/`
- **Git**: No version control
- **Builder**: Single builder, no parallelization
- **Example**: Calculator, text formatter, file converter

### Type 2: Program
- **Definition**: Multi-file application without version control
- **Location**: User-specified (default: `~/projects/github/{name}/`)
- **Git**: Optional local repo (`git init`)
- **Builder**: Can parallelize if complex
- **Example**: Data processing pipeline, batch job system

### Type 3: Project
- **Definition**: Full GitHub-integrated software project
- **Location**: User-specified (default: `~/projects/github/{name}/`)
- **Git**: Full repository with remote
- **GitHub**: Clone/fork existing or create new
- **Builder**: Parallel builders with feature branches
- **Example**: Web application, API service, library

## Workflow Chain

```
project_intake
    ↓ (creates session context)
design_program
    ↓ (creates design + test suite)
build_program (parent mode)
    ↓ (spawns N builders)
[builder-A] [builder-B] [builder-C]
    ↓ (all complete)
merge_results
    ↓ (sequential merge with conflict resolution)
merge_to_main
    ↓ (if required)
cleanup & report
```

## State Management

### Session State Structure

```
~/.argo/sessions/{session_id}/
├── context.json           # Project-level state
├── design.json           # From design_program
└── builders/
    ├── argo-A.json       # Builder A state
    ├── argo-B.json       # Builder B state
    └── argo-C.json       # Builder C state
```

### Context Schema

```json
{
  "session_id": "proj-20251021-143022",
  "project_name": "myapp",
  "project_type": "project",
  "project_dir": "/Users/cskoons/projects/github/myapp",
  "is_git_repo": true,
  "is_github_project": true,
  "main_branch": "main",
  "feature_branch": "myapp/feature",
  "parallel_builds": true,
  "builder_count": 3,
  "current_phase": "building"
}
```

### Builder State Schema

```json
{
  "builder_id": "argo-A",
  "feature_branch": "myapp/feature/argo-A",
  "task_description": "Implement authentication system",
  "status": "completed",
  "build_dir": "/Users/cskoons/projects/github/myapp",
  "tests_passed": true,
  "tests_passed_count": 7,
  "tests_failed_count": 0,
  "total_tests": 7,
  "failed_tests": [],
  "test_results": "...",
  "commits": [
    "abc123: Add login endpoint",
    "def456: Add JWT validation"
  ],
  "needs_assistance": false,
  "completion_time": "2025-10-21T14:45:00Z"
}
```

### Status Values

- `initialized` - Builder spawned, preparing workspace
- `building` - Generating code
- `testing` - Running test suite
- `fixing` - Iterating on test failures
- `completed` - All tests pass, ready to merge
- `failed` - Exceeded max attempts, cannot continue
- `needs_assistance` - Parent intervention required

## Branch Naming Convention

### Structure

```
{project}/{feature}/argo-{A,B,C,D,E}
```

### Example Hierarchy

```
main                           # Production branch
└── myapp/auth                 # Feature branch (parent merges here first)
    ├── myapp/auth/argo-A      # Builder A (login system)
    ├── myapp/auth/argo-B      # Builder B (JWT tokens)
    └── myapp/auth/argo-C      # Builder C (password reset)
```

### Merge Flow

1. Builders commit to `{project}/{feature}/argo-X` branches
2. Parent merges all `argo-X` branches into `{project}/{feature}`
3. Parent merges `{project}/{feature}` into `main` (if required)
4. Cleanup: Delete all `argo-X` branches after successful merge

## Configuration

### Global Settings

```bash
# ~/.argo/config

# Builder limits
ARGO_MAX_BUILDERS=5

# Merge behavior
ARGO_AUTO_MERGE_TO_MAIN=true
ARGO_MAX_MERGE_ATTEMPTS=2

# Test behavior
ARGO_MAX_TEST_ATTEMPTS=5
```

## Parent CI Responsibilities

### 1. Orchestrator
- Analyze requirements and decompose into independent tasks
- Determine optimal number of builders (capped at `ARGO_MAX_BUILDERS`)
- Spawn builder processes with task assignments
- Monitor builder state files for completion

### 2. Test Engineer
- Generate comprehensive test suite BEFORE spawning builders
- Split tests among builders based on task decomposition
- Ensure test coverage for all requirements
- Commit tests to feature branch for builders to use

### 3. Failure Analyst
- Monitor builder progress (tests passed vs. failed)
- Analyze test failures when builder needs assistance
- Determine if problem is in test or implementation
- Make decisions on test refactoring or code regeneration

### 4. Test Refactorer
- Identify unreasonable or incorrectly written tests
- Use AI to refactor tests while maintaining requirement validation
- Respawn builders with corrected tests

### 5. Merge Strategist
- Determine optimal merge order (smallest changes first)
- Perform sequential merges with conflict detection
- Classify conflicts as trivial or complex

### 6. Conflict Resolver
- Auto-resolve trivial conflicts (whitespace, imports, formatting)
- Use AI to detect and fix simple merge issues
- Respawn builders on complex conflicts with updated base branch

### 7. Quality Gatekeeper
- Run full test suite after each merge
- Verify tests pass before merging to main
- Prevent broken code from reaching production

### 8. User Liaison
- Escalate complex decisions to user
- Request approval for test omissions
- Report failures that cannot be auto-resolved

## Builder CI Responsibilities

### 1. Code Generator
- Generate code to pass pre-written tests (test-driven development)
- Follow language best practices and style guidelines
- Implement assigned task independently

### 2. Test Runner
- Execute test suite repeatedly during development
- Capture test output for failure analysis

### 3. Code Fixer
- Analyze test failures
- Use AI to fix code until tests pass
- Iterate up to `ARGO_MAX_TEST_ATTEMPTS` times

### 4. State Reporter
- Write progress to state file after each phase
- Update test pass/fail counts
- Signal completion or need for assistance

### 5. Independent Worker
- No communication with sibling builders
- Only communicate with parent via state file
- Work entirely on own feature branch

## Builder Communication Rules

### Critical Constraints

1. **No sibling communication**: Builders never talk to each other
2. **Parent-only communication**: All communication via state files
3. **Task independence**: Each builder's task must be fully independent
4. **Dependency grouping**: Dependent tasks grouped in same builder

### Parent Responsibility

Parent must analyze task graph and ensure:
- Tasks assigned to different builders are independent
- OR dependent tasks are grouped in same builder

**Bad Example** (creates dependency):
```
Builder A: Create database schema
Builder B: Create API that uses database  ❌
```

**Good Example** (independent):
```
Builder A: Create database layer + its API
Builder B: Create authentication layer + its API  ✓
```

**Good Example** (grouped dependency):
```
Builder A: Create database schema AND API together  ✓
```

## Test-Driven Development Flow

### Parent Phase: Test Suite Creation

```bash
1. Parent analyzes requirements
2. Parent uses AI to generate comprehensive test suite
3. Parent splits tests by component/task
4. Parent commits tests to feature branch
5. Parent spawns builders with test assignments
```

### Builder Phase: Code to Pass Tests

```bash
1. Builder checks out feature branch (has tests)
2. Builder generates code to pass assigned tests
3. Builder runs tests
4. If tests fail:
   - Analyze failures
   - Fix code with AI
   - Repeat up to ARGO_MAX_TEST_ATTEMPTS
5. If tests pass:
   - Commit code
   - Mark status as completed
6. If max attempts exceeded:
   - Mark status as needs_assistance
   - Exit
```

### Advantages

- Test-driven development enforced
- Tests consistent across all builders
- No test duplication or gaps
- Parent knows exact success criteria
- Builder success is unambiguous (tests pass or fail)

## Builder Build-Test-Fix Loop

```bash
builder_ci_loop() {
    update_state "initialized"

    # Check out feature branch (has tests)
    git checkout -b "$BUILDER_BRANCH" "$FEATURE_BRANCH"
    update_state "building"

    local attempt=1
    while [ $attempt -le $ARGO_MAX_TEST_ATTEMPTS ]; do
        if [ $attempt -eq 1 ]; then
            generate_code_for_tests
        else
            fix_code_based_on_failures
        fi

        update_state "testing"
        run_tests_capture_output

        if all_tests_pass; then
            git commit -am "Implement ${TASK_DESC}"
            git push
            update_state "completed"
            return 0
        fi

        update_state "fixing"
        attempt=$((attempt + 1))
    done

    # Failed after max attempts
    update_state "needs_assistance"
    return 1
}
```

## Parent Failure Handling

### Scenario 1: Builder Makes Good Progress

**Condition**: 70%+ tests passing, but 2-3 tests fail

**Parent Actions**:
1. Analyze failed tests with AI
2. Determine if test is incorrectly written
3. Options:
   - **Test is wrong**: Refactor test, respawn builder
   - **Test is too strict**: Refactor test, respawn builder
   - **Test should be omitted**: Ask user approval, omit test
   - **Test is correct**: Respawn builder with guidance

### Scenario 2: Builder Makes Poor Progress

**Condition**: <70% tests passing

**Parent Actions**:
1. Respawn builder immediately
2. No test refactoring (tests likely correct)
3. Provide additional guidance in respawn

### Scenario 3: Multiple Respawn Failures

**Condition**: Builder fails 2-3 times even after respawns

**Parent Actions**:
1. Escalate to user
2. Options:
   - Simplify task (split into smaller tasks)
   - Manual intervention
   - Abort this builder (continue with others)

## Merge Conflict Resolution

### Phase 1: Sequential Merge

```bash
# Determine merge order (smallest changes first)
MERGE_ORDER=$(sort_branches_by_size)

for branch in $MERGE_ORDER; do
    if git merge $branch; then
        # Clean merge
        delete_branch $branch
    else
        # Conflict detected
        if resolve_trivial_conflicts; then
            commit_resolved_merge
            delete_branch $branch
        else
            # Complex conflict
            defer_branch $branch
        fi
    fi
done
```

### Phase 2: Classify Conflicts

**Trivial Conflicts** (Parent auto-resolves with AI):
- Whitespace differences
- Import statement ordering
- Comment wording differences
- Formatting (both sides functionally equivalent)
- Non-conflicting additions to same file

**Complex Conflicts** (Requires respawn):
- Logic conflicts (different implementations of same function)
- Data structure conflicts (incompatible schemas)
- API conflicts (different function signatures)

### Phase 3: Handle Deferred Branches

```bash
for failed_branch in $DEFERRED_BRANCHES; do
    # Delete failed branch
    git branch -D $failed_branch

    # Create new branch from current merged state
    git checkout -b $failed_branch $FEATURE_BRANCH

    # Respawn builder with updated base
    respawn_builder_on_new_base $failed_branch
done

# Wait for respawned builders
wait_for_completion

# Retry merges (should succeed with updated base)
merge_respawned_builders
```

**Key Insight**: Respawning on merged base eliminates conflicts since builder now has all other builders' changes.

## Merge to Main Flow

### Condition Checking

```bash
# Only merge to main if:
1. All builder branches successfully merged to feature branch
2. Requirements specify merge to main
3. User approves (if not auto-merge)
```

### Merge Strategy

```bash
1. Merge main INTO feature branch first
   git checkout $FEATURE_BRANCH
   git merge main

2. If conflicts:
   - Resolve trivial conflicts automatically
   - Complex conflicts: Use AI refactoring

3. Run full test suite on merged branch

4. If tests pass:
   - Merge feature INTO main
   - Delete feature branch

5. If tests fail:
   - Attempt refactoring (up to ARGO_MAX_MERGE_ATTEMPTS)
   - If still failing: Escalate to user
```

### Auto-Merge Mode

When `ARGO_AUTO_MERGE_TO_MAIN=true`:

```bash
attempt=1
while [ $attempt -le $ARGO_MAX_MERGE_ATTEMPTS ]; do
    if merge_and_test_passes; then
        success "Merged to main"
        return 0
    fi

    # Failed - attempt refactoring
    refactor_code_to_fix_conflicts_or_tests
    attempt=$((attempt + 1))
done

# Failed after max attempts
report_to_user "Auto-merge failed after $ARGO_MAX_MERGE_ATTEMPTS attempts"
offer_manual_resolution
```

## Implementation Phases

### Phase 1: Foundation (Week 1)
- **State management library** (`workflows/lib/state-manager.sh`)
  - Session creation/loading
  - Context save/load
  - Builder state management

- **Git helpers library** (`workflows/lib/git-helpers.sh`)
  - Branch creation/deletion
  - Merge operations
  - Conflict detection

### Phase 2: Entry Point (Week 1-2)
- **`project_intake` workflow**
  - Question flow for project type
  - Directory setup
  - Git initialization
  - Session state creation

### Phase 3: Design Enhancement (Week 2)
- **Update `design_program`**
  - Read session context
  - Pass project type to CI
  - Generate test suite
  - Save tests to session

### Phase 4: Build Infrastructure (Week 3)
- **Update `build_program`**
  - Unified tool/program/project handler
  - Parallel builder orchestration
  - State monitoring loop
  - Basic merge logic

### Phase 5: Builder Loop (Week 3)
- **Create `builder_ci_loop`**
  - Build-test-fix iteration
  - State file updates
  - Independent execution

### Phase 6: Advanced Features (Week 4)
- **Parent failure handling**
  - Test refactoring
  - Builder respawning

- **Merge conflict resolution**
  - Trivial conflict detection
  - Complex conflict handling
  - Branch respawn strategy

### Phase 7: GitHub Integration (Week 5)
- **Merge to main**
  - Auto-merge mode
  - Test suite validation
  - User escalation

- **Cleanup and reporting**
  - Branch deletion
  - Session archival
  - User summary

## Complexity Assessment

### Modules Suitable for Bash
- State management (file I/O, jq for JSON)
- Git operations (wrapper around git CLI)
- project_intake (deterministic questions)
- Basic orchestration (fork, wait, polling)

### Modules That May Need Python
- **Conflict analyzer** (`workflows/lib/conflict_analyzer.py`)
  - Parse git conflict markers
  - Classify trivial vs. complex
  - Extract conflicting sections

- **Code refactorer** (`workflows/lib/code_refactor.py`)
  - Apply AI-suggested changes safely
  - Preserve code structure
  - Validate syntax

- **Test result parser** (`workflows/lib/test_parser.py`)
  - Extract structured data from test output
  - Language-specific parsing (pytest, jest, etc.)
  - Failure categorization

**Decision**: Start in bash, migrate to Python only if complexity warrants.

## Success Criteria

### For Tools
- Single file generated
- Tests pass
- Installed to `~/.local/bin/` (if user approves)

### For Programs
- Multi-file structure created
- All tests pass
- Documentation generated
- Ready to run

### For Projects
- Full repository with remote
- Feature branch merged (or PR created)
- Main branch updated (if required)
- All tests passing on main
- Clean commit history

## Risk Mitigation

### Risk: Builder Deadlock
**Mitigation**: Timeout on builder execution, parent force-kills after timeout

### Risk: Merge Loop
**Mitigation**: Track respawn count, abort after N respawns, escalate to user

### Risk: Test Suite Quality
**Mitigation**: Parent reviews test suite with AI before spawning builders

### Risk: Resource Exhaustion
**Mitigation**: Cap builders at `ARGO_MAX_BUILDERS`, queue excess tasks

### Risk: GitHub Rate Limiting
**Mitigation**: Batch commits, use personal access tokens with higher limits

## Future Enhancements

### Phase 8: Advanced Orchestration
- Builder priority queuing
- Resource-aware scheduling
- Dependency graph visualization

### Phase 9: Learning & Optimization
- Track merge conflict patterns
- Learn optimal merge ordering
- Suggest task decompositions based on history

### Phase 10: Multi-Repository
- Cross-repo dependency management
- Shared library updates
- Coordinated releases

## References

- Argo Workflow System Architecture
- GitHub CLI Documentation (`gh`)
- Test-Driven Development Best Practices

---

**Document Version**: 1.0
**Last Updated**: 2025-10-21
**Review Status**: Approved for Implementation
