# Argo Workflows User Guide

© 2025 Casey Koons. All rights reserved.

This guide helps you understand and use the Argo workflow orchestration system effectively.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Understanding Workflows](#understanding-workflows)
3. [Monitoring Progress](#monitoring-progress)
4. [Working with State Files](#working-with-state-files)
5. [Common Workflows](#common-workflows)
6. [Intervening in Workflows](#intervening-in-workflows)
7. [Tips and Best Practices](#tips-and-best-practices)
8. [FAQ](#faq)

## Getting Started

### Prerequisites

Before using Argo, ensure you have:

```bash
# Check bash version (need 4.0+)
bash --version

# Check for jq (JSON processor)
jq --version

# Check for git
git --version

# Check for CI tool (companion intelligence)
which ci
```

### Initial Setup

1. **Navigate to your project directory**:
```bash
cd your-project-directory
```

2. **Initialize Argo in your project**:
```bash
../templates/project_setup.sh "my-project-name" .
```

This creates a `.argo-project/` directory with:
- `state.json` - Workflow state
- `logs/` - Log files
- `builders/` - Git worktrees (created as needed)

3. **Verify setup**:
```bash
ls -la .argo-project/
# Should show: state.json, logs/, orchestrator.pid (when running)
```

### Starting Your First Workflow

```bash
./start_workflow.sh "Build a calculator with add, subtract, multiply, and divide operations"
```

Output:
```
Workflow started!
Orchestrator PID: 12345
Monitor progress: tail -f .argo-project/logs/workflow.log
```

## Understanding Workflows

### The Five Phases

Argo workflows progress through five phases:

#### 1. Init Phase (design_program.sh)

**What happens**:
- Takes your user query
- Queries the CI tool for a high-level design
- Creates initial design document
- Waits for approval

**Duration**: 30-120 seconds (depends on CI response time)

**User action**: Review and approve the design (or modify it)

**Example design**:
```json
{
  "design": {
    "summary": "Build a calculator application with four basic operations",
    "components": [
      {
        "id": "calculator-core",
        "description": "Core calculation engine",
        "technologies": ["javascript"]
      },
      {
        "id": "cli-interface",
        "description": "Command-line interface",
        "technologies": ["nodejs"]
      }
    ]
  }
}
```

#### 2. Design Phase (design_build.sh)

**What happens**:
- Validates design components
- Creates builder entries for each component
- Sets up git worktrees for parallel work
- Transitions to execution

**Duration**: 5-15 seconds

**User action**: None (automatic)

**What you'll see**:
```bash
=== design_build phase ===
Creating builders from design...
Creating 2 builders
Creating worktree for builder: calculator-core
Creating worktree for builder: cli-interface
design_build phase complete. Ready for execution...
```

#### 3. Execution Phase (execution.sh)

**What happens**:
- Spawns CI sessions for each builder in parallel
- Each builder works in its own git worktree
- Monitors progress via process IDs
- Waits for all builders to complete

**Duration**: 5-30 minutes (depends on component complexity)

**User action**: None (automatic, but you can monitor)

**What you'll see**:
```bash
=== execution phase ===
Spawning 2 builders...
Spawning builder: calculator-core
Spawning builder: cli-interface
All builders spawned. Monitoring progress...
Builder calculator-core completed
Builder cli-interface completed
All builders complete!
```

#### 4. Merge Build Phase (merge_build.sh)

**What happens**:
- Merges each builder branch back to main
- Detects and handles merge conflicts
- Cleans up git worktrees
- Transitions to storage

**Duration**: 10-60 seconds

**User action**: Resolve conflicts if they occur

**What you'll see**:
```bash
=== merge_build phase ===
Merging 2 builder branches...
Merging builder: calculator-core
Merging builder: cli-interface
All builders merged successfully!
merge_build phase complete. Workflow finished!
```

#### 5. Storage Phase (terminal)

**What happens**:
- Workflow complete
- All code committed to git
- Orchestrator exits

**Duration**: Immediate

**User action**: Review the results

## Monitoring Progress

### Real-Time Log Monitoring

```bash
# Watch workflow logs in real-time
tail -f .argo-project/logs/workflow.log
```

Log format:
```
[2025-11-03T10:05:23Z] [STATE] init → design
[2025-11-03T10:05:24Z] [BUILDER] api: spawned (PID 12345)
[2025-11-03T10:08:15Z] [BUILDER] api: completed
[2025-11-03T10:08:20Z] [MERGE] Merging branch argo-builder-api
```

### Quick Status Check

```bash
./status.sh
```

Example output:
```
=== Argo Workflow Status ===
Project: my-project
Phase: execution
Action Owner: code
Action Needed: spawn_builders

Execution Status:
  Builder: calculator-core (running, PID 12345)
  Builder: cli-interface (running, PID 12346)

Orchestrator: Running (PID 9999)
```

### State File Inspection

```bash
# Pretty-print the entire state
jq . .argo-project/state.json

# Check current phase
jq -r '.phase' .argo-project/state.json

# Check who owns the action
jq -r '.action_owner' .argo-project/state.json

# Check builder status
jq -r '.execution.builders[] | "\(.id): \(.status)"' .argo-project/state.json
```

### Process Monitoring

```bash
# Check if orchestrator is running
ps -p $(cat .argo-project/orchestrator.pid)

# Check builder processes
jq -r '.execution.builders[] | select(.status=="running") | .pid' .argo-project/state.json | xargs ps -p
```

## Working with State Files

### State File Structure

The `.argo-project/state.json` file contains all workflow state:

```json
{
  "project_name": "my-project",
  "phase": "execution",
  "action_owner": "code",
  "action_needed": "spawn_builders",
  "user_query": "Build a calculator",
  "design": { ... },
  "execution": { ... },
  "merge": { ... }
}
```

### Key State Fields

**phase**: Current workflow phase
- `init`, `design`, `execution`, `merge_build`, `storage`

**action_owner**: Who should act next
- `code` - Orchestrator will act (automatic)
- `ci` - CI tool or human needs to review/approve
- `user` - User intervention required

**action_needed**: Specific action to take
- Used to select which phase handler to run
- Usually matches the phase name

### Reading State Safely

**Don't** manually edit `state.json` while the workflow is running (race conditions).

**Do** use the state reading utilities:

```bash
# Source the state file utilities
source lib/state_file.sh

# Read a field
phase=$(read_state "phase")
echo "Current phase: $phase"

# Read nested field
builder_status=$(read_state "execution.builders[0].status")
echo "First builder status: $builder_status"
```

### Updating State (Advanced)

If you need to intervene:

1. **Stop the orchestrator**:
```bash
kill $(cat .argo-project/orchestrator.pid)
```

2. **Update state using jq**:
```bash
# Change action owner to allow manual intervention
jq '.action_owner = "user"' .argo-project/state.json > .argo-project/state.json.tmp
mv .argo-project/state.json.tmp .argo-project/state.json
```

3. **Restart the orchestrator**:
```bash
./start_workflow.sh "resume"
```

## Common Workflows

### Simple Single-Component Build

```bash
./start_workflow.sh "Create a hello world program in Python"
```

Expected flow:
- Init: CI designs a single component
- Design: Creates one builder
- Execution: Single builder implements the program
- Merge: Merges back to main
- Storage: Complete

**Duration**: 5-10 minutes

### Multi-Component Application

```bash
./start_workflow.sh "Build a REST API with user authentication, database layer, and API endpoints"
```

Expected flow:
- Init: CI designs 3 components (auth, database, api)
- Design: Creates 3 builders with 3 worktrees
- Execution: 3 parallel CI sessions build components
- Merge: Merges 3 branches (may have conflicts)
- Storage: Complete

**Duration**: 15-45 minutes

### Resuming After Interruption

If the workflow is stopped (Ctrl+C, system restart, etc.):

```bash
# Check the state
./status.sh

# Restart the orchestrator
orchestrator.sh $(pwd)
```

The orchestrator will resume from the current state.

### Approving a Design

After the init phase, the workflow waits for approval:

```bash
# View the design
jq '.design' .argo-project/state.json

# Approve by changing action_owner
jq '.action_owner = "code" | .action_needed = "design_build"' \
   .argo-project/state.json > .argo-project/state.json.tmp
mv .argo-project/state.json.tmp .argo-project/state.json
```

The orchestrator will automatically continue within 5 seconds.

## Intervening in Workflows

### When to Intervene

**Safe to let run**:
- Phase transitions are automatic
- Builders are making progress
- No merge conflicts

**Consider intervening**:
- Design doesn't match your requirements
- Merge conflicts detected
- Builder stuck or taking too long
- Want to modify the approach

### Stopping a Workflow

```bash
# Graceful stop (SIGTERM)
kill $(cat .argo-project/orchestrator.pid)

# Force stop (SIGKILL) - only if graceful fails
kill -9 $(cat .argo-project/orchestrator.pid)
```

### Restarting a Workflow

```bash
# Start the orchestrator pointing at your project
orchestrator.sh $(pwd)
```

The workflow resumes from the current phase.

### Modifying the Design

After init phase, before design_build:

1. Stop the orchestrator
2. Edit the design in `state.json`:
```bash
jq '.design.components += [{"id": "new-component", "description": "Additional feature"}]' \
   .argo-project/state.json > .argo-project/state.json.tmp
mv .argo-project/state.json.tmp .argo-project/state.json
```
3. Update action owner:
```bash
jq '.action_owner = "code" | .action_needed = "design_build"' \
   .argo-project/state.json > .argo-project/state.json.tmp
mv .argo-project/state.json.tmp .argo-project/state.json
```
4. Restart the orchestrator

### Resolving Merge Conflicts

When merge conflicts occur:

1. The workflow pauses with `action_owner = "ci"` and `action_needed = "resolve_conflicts"`
2. Check which builders had conflicts:
```bash
jq '.merge_conflicts' .argo-project/state.json
```
3. Manually resolve conflicts:
```bash
# See the conflict
git status

# Edit the conflicting files
vim path/to/conflicted/file

# Mark resolved
git add path/to/conflicted/file
git commit
```
4. Update state to continue:
```bash
jq '.action_owner = "code" | .action_needed = "merge_branches"' \
   .argo-project/state.json > .argo-project/state.json.tmp
mv .argo-project/state.json.tmp .argo-project/state.json
```

## Tips and Best Practices

### Writing Effective Queries

**Good queries**:
- Specific and actionable
- Include technology preferences
- Mention architectural patterns
- Example: "Build a REST API using Express.js with JWT authentication and PostgreSQL database"

**Poor queries**:
- Too vague: "Build something"
- No context: "Make it better"
- Contradictory: "Build a fast slow application"

### Managing Long-Running Workflows

```bash
# Run orchestrator in background with nohup
nohup orchestrator.sh $(pwd) > orchestrator.out 2>&1 &

# Or use screen/tmux
screen -S argo-workflow
orchestrator.sh $(pwd)
# Ctrl+A, D to detach
```

### Cleaning Up After Workflows

```bash
# Remove completed workflow artifacts (optional)
rm -rf .argo-project/builders/*
rm -f .argo-project/logs/*.log

# Keep state.json for history
```

### Performance Tips

1. **Use SSD storage** - Git worktrees create many small files
2. **Close other programs** - Builders can be CPU intensive
3. **Limit parallel builders** - Default is 10, adjust if needed
4. **Use local git** - Remote git operations slow down builders

### Debugging Workflows

```bash
# Enable verbose logging
export ARGO_LOG_LEVEL=DEBUG
orchestrator.sh $(pwd)

# Check builder outputs
ls -la .argo-project/builders/*/

# View individual builder logs
tail .argo-project/builders/calculator-core/.build_result
```

## FAQ

### Q: Can I run multiple workflows in parallel?

**A**: Not currently. Argo uses a single orchestrator per project. To run parallel workflows, create separate project directories.

### Q: What happens if my computer restarts during a workflow?

**A**: The workflow state is preserved in `state.json`. Restart the orchestrator to continue from where it left off. In-progress builders will need to restart.

### Q: Can I modify code while builders are running?

**A**: Not recommended. Builders work in separate git worktrees, but modifying the main branch can cause merge conflicts.

### Q: How do I cancel a workflow?

**A**: Stop the orchestrator (`kill $(cat .argo-project/orchestrator.pid)`). To clean up, manually delete `.argo-project/` or set `phase = "storage"` in state.json.

### Q: What if a builder gets stuck?

**A**:
1. Check the builder PID: `jq '.execution.builders[] | select(.id=="stuck-builder") | .pid' .argo-project/state.json`
2. Kill the process: `kill <PID>`
3. Update state to mark as failed: `jq '(.execution.builders[] | select(.id=="stuck-builder")) |= (.status = "failed")' .argo-project/state.json > .argo-project/state.json.tmp && mv .argo-project/state.json.tmp .argo-project/state.json`

### Q: Can I use a different CI tool?

**A**: Yes. Set the `CI_COMMAND` environment variable:
```bash
export CI_COMMAND=/path/to/your/ci/tool
./start_workflow.sh "your query"
```

### Q: How do I see what the CI tool is being asked to build?

**A**: Look at the builder spawning code in `phases/execution.sh` or check the prompts in the logs.

### Q: Can I add more builders after execution starts?

**A**: Not automatically. You'd need to stop the workflow, modify the state to add builders, and restart. This is advanced usage.

### Q: What's the difference between action_owner="code" and "ci"?

**A**:
- `action_owner="code"` - Orchestrator will automatically execute the next handler
- `action_owner="ci"` - Workflow pauses, waiting for CI or human review
- `action_owner="user"` - Workflow pauses, explicitly waiting for user intervention

### Q: How do I know when the workflow is complete?

**A**: When `phase = "storage"` in state.json. The orchestrator automatically exits.

### Q: Can I reuse a completed workflow?

**A**: The state file is preserved. You can review it for history, but to start a new workflow, either delete `.argo-project/` or create a new project directory.

### Q: What files does Argo create?

**A**:
- `.argo-project/state.json` - Workflow state
- `.argo-project/orchestrator.pid` - Orchestrator process ID (when running)
- `.argo-project/logs/workflow.log` - Workflow logs
- `.argo-project/builders/*` - Git worktrees (temporary, cleaned up after merge)
- Git branches: `argo-builder-<component-id>` (temporary, deleted after merge)

---

For more details, see:
- [README.md](README.md) - Overview and quick start
- [ARCHITECTURE.md](ARCHITECTURE.md) - Technical deep-dive
- [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) - Creating phase handlers
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Common issues and solutions
