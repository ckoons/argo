# Argo Workflows Troubleshooting Guide

© 2025 Casey Koons. All rights reserved.

This guide helps you diagnose and fix common issues with Argo workflows.

## Table of Contents

1. [Orchestrator Issues](#orchestrator-issues)
2. [State File Problems](#state-file-problems)
3. [Git Worktree Issues](#git-worktree-issues)
4. [Builder Problems](#builder-problems)
5. [Merge Conflicts](#merge-conflicts)
6. [CI Tool Issues](#ci-tool-issues)
7. [Permission Problems](#permission-problems)
8. [Resource Issues](#resource-issues)
9. [Log Investigation](#log-investigation)
10. [Recovery Procedures](#recovery-procedures)

## Orchestrator Issues

### Orchestrator Won't Start

**Symptom**: Running `start_workflow.sh` or `orchestrator.sh` fails immediately.

**Diagnosis**:
```bash
# Check if orchestrator is already running
cat .argo-project/orchestrator.pid 2>/dev/null
ps -p $(cat .argo-project/orchestrator.pid 2>/dev/null) 2>/dev/null

# Check for errors in logs
tail .argo-project/logs/workflow.log

# Check state file exists and is valid JSON
jq . .argo-project/state.json
```

**Solutions**:

1. **Already running**: Kill existing orchestrator first
```bash
kill $(cat .argo-project/orchestrator.pid)
rm .argo-project/orchestrator.pid
./start_workflow.sh "your query"
```

2. **Invalid state file**: Restore or recreate
```bash
# Backup corrupted file
cp .argo-project/state.json .argo-project/state.json.backup

# Recreate from template
cp ../templates/state_template.json .argo-project/state.json

# Or manually fix with jq
jq . .argo-project/state.json.backup > .argo-project/state.json
```

3. **Missing directories**: Recreate project structure
```bash
mkdir -p .argo-project/logs
chmod 755 .argo-project
./start_workflow.sh "your query"
```

### Orchestrator Won't Stop

**Symptom**: `kill` command doesn't stop orchestrator.

**Diagnosis**:
```bash
# Check if process exists
ps -p $(cat .argo-project/orchestrator.pid) -o pid,ppid,stat,command

# Check if process is stuck
kill -0 $(cat .argo-project/orchestrator.pid) 2>/dev/null && echo "Process exists"
```

**Solutions**:

1. **Send SIGTERM first** (graceful):
```bash
kill $(cat .argo-project/orchestrator.pid)
# Wait 5 seconds
sleep 5
```

2. **Force kill if needed** (not graceful):
```bash
kill -9 $(cat .argo-project/orchestrator.pid)
rm .argo-project/orchestrator.pid
```

3. **Process is zombie** (wait for parent or reboot):
```bash
# Identify parent process
ps -o ppid= -p $(cat .argo-project/orchestrator.pid)
# If parent is init (1), reboot may be needed
```

### Orchestrator Crashes

**Symptom**: Orchestrator exits unexpectedly.

**Diagnosis**:
```bash
# Check exit code (if available)
echo $?

# Check logs for error messages
tail -n 50 .argo-project/logs/workflow.log

# Check system logs
dmesg | grep -i "argo\|bash"

# Check for out-of-memory
grep -i "out of memory\|oom" /var/log/syslog
```

**Solutions**:

1. **Handler script error**: Fix the failing phase handler
```bash
# Identify which handler failed from logs
grep "ERROR" .argo-project/logs/workflow.log

# Test handler manually
./phases/failing_handler.sh $(pwd)
```

2. **Out of memory**: Reduce parallel builders or increase memory
```bash
# Check memory usage
free -h

# Reduce builders (edit state.json before starting)
jq '.execution.builders = [.execution.builders[0]]' .argo-project/state.json > tmp && mv tmp .argo-project/state.json
```

3. **Disk space full**: Free up space
```bash
# Check disk usage
df -h

# Clean up old builders
rm -rf .argo-project/builders/*

# Clean up old logs
rm .argo-project/logs/*.log.old
```

## State File Problems

### State File Corrupted

**Symptom**: `jq` fails to parse state.json.

**Diagnosis**:
```bash
# Try to parse state file
jq . .argo-project/state.json

# Check file size
ls -lh .argo-project/state.json

# Look for incomplete writes
tail .argo-project/state.json
```

**Solutions**:

1. **Restore from backup** (if available):
```bash
# Check for .tmp files
ls -la .argo-project/state.json*

# Restore from .tmp if more recent and valid
jq . .argo-project/state.json.tmp && mv .argo-project/state.json.tmp .argo-project/state.json
```

2. **Manually fix JSON**:
```bash
# Edit with vim or nano
vim .argo-project/state.json

# Common fixes:
# - Add missing closing brace: }
# - Remove trailing comma
# - Fix unescaped quotes
```

3. **Recreate from scratch**:
```bash
# Backup old file
mv .argo-project/state.json .argo-project/state.json.broken

# Create minimal valid state
cat > .argo-project/state.json <<EOF
{
  "project_name": "my-project",
  "phase": "init",
  "action_owner": "user",
  "action_needed": "manual_recovery"
}
EOF

# Restart workflow
./start_workflow.sh "your query"
```

### State File Has Wrong Permissions

**Symptom**: Cannot read or write state.json.

**Diagnosis**:
```bash
# Check permissions
ls -l .argo-project/state.json

# Check ownership
ls -l .argo-project/state.json | awk '{print $3, $4}'
```

**Solutions**:
```bash
# Fix permissions
chmod 644 .argo-project/state.json

# Fix ownership (if needed)
chown $USER:$(id -gn) .argo-project/state.json
```

### State Doesn't Update

**Symptom**: Workflow appears stuck, state file doesn't change.

**Diagnosis**:
```bash
# Check if orchestrator is running
ps -p $(cat .argo-project/orchestrator.pid) >/dev/null 2>&1 && echo "Running" || echo "Not running"

# Check action_owner
jq -r '.action_owner' .argo-project/state.json

# Check for handler errors in logs
grep "ERROR" .argo-project/logs/workflow.log
```

**Solutions**:

1. **action_owner is "ci" or "user"**: Orchestrator is waiting
```bash
# Check what's needed
jq -r '.action_needed' .argo-project/state.json

# Change owner to resume
jq '.action_owner = "code"' .argo-project/state.json > tmp && mv tmp .argo-project/state.json
```

2. **Orchestrator stopped**: Restart it
```bash
orchestrator.sh $(pwd)
```

3. **Handler failed silently**: Check logs and fix handler
```bash
# Find failing handler
jq -r '.action_needed // .phase' .argo-project/state.json

# Test manually
./phases/<handler>.sh $(pwd)
```

## Git Worktree Issues

### Worktree Creation Fails

**Symptom**: `git worktree add` fails.

**Diagnosis**:
```bash
# Check existing worktrees
git worktree list

# Check if branch already exists
git branch -a | grep "argo-builder"

# Check disk space
df -h .
```

**Solutions**:

1. **Worktree already exists**: Remove it first
```bash
git worktree remove .argo-project/builders/<name> --force
```

2. **Branch already exists**: Delete branch first
```bash
git branch -D argo-builder-<name>
```

3. **Not in git repo**: Initialize git
```bash
git init
git add .
git commit -m "Initial commit"
```

### Worktree Can't Be Removed

**Symptom**: `git worktree remove` fails.

**Diagnosis**:
```bash
# Check worktree status
git worktree list

# Check for processes using worktree
lsof +D .argo-project/builders/<name>

# Check worktree directory permissions
ls -ld .argo-project/builders/<name>
```

**Solutions**:

1. **Force removal**:
```bash
git worktree remove .argo-project/builders/<name> --force
```

2. **Manual cleanup** (if force fails):
```bash
# Remove directory
rm -rf .argo-project/builders/<name>

# Clean up git metadata
git worktree prune
```

3. **Kill processes using worktree**:
```bash
# Find processes
lsof +D .argo-project/builders/<name>

# Kill them
kill <pid>

# Then remove worktree
git worktree remove .argo-project/builders/<name> --force
```

### Worktree Has Uncommitted Changes

**Symptom**: Can't merge or remove worktree due to uncommitted changes.

**Diagnosis**:
```bash
# Check status in worktree
cd .argo-project/builders/<name>
git status
```

**Solutions**:

1. **Commit changes**:
```bash
cd .argo-project/builders/<name>
git add .
git commit -m "Builder changes"
cd ../..
```

2. **Stash changes** (if not ready to commit):
```bash
cd .argo-project/builders/<name>
git stash
cd ../..
```

3. **Discard changes** (if not needed):
```bash
cd .argo-project/builders/<name>
git reset --hard
cd ../..
```

## Builder Problems

### Builder Won't Spawn

**Symptom**: Builder status stays "pending" or spawning fails.

**Diagnosis**:
```bash
# Check CI tool exists
which ci

# Check builder configuration in state
jq -r '.execution.builders[] | "\(.id): \(.status)"' .argo-project/state.json

# Check for spawn errors in logs
grep "spawn" .argo-project/logs/workflow.log
```

**Solutions**:

1. **CI tool not found**: Install or configure CI tool
```bash
# Check PATH
echo $PATH

# Set CI_COMMAND if needed
export CI_COMMAND=/path/to/ci/tool
```

2. **Worktree not created**: Check worktree exists
```bash
ls -la .argo-project/builders/

# Recreate if needed
git worktree add .argo-project/builders/<name> -b argo-builder-<name>
```

3. **Process limit reached**: Reduce parallel builders
```bash
# Check process limit
ulimit -u

# Increase limit (if needed)
ulimit -u 4096
```

### Builder Gets Stuck

**Symptom**: Builder runs but never completes.

**Diagnosis**:
```bash
# Check if process is running
pid=$(jq -r '.execution.builders[] | select(.id=="<builder-id>") | .pid' .argo-project/state.json)
ps -p $pid -o pid,stat,time,command

# Check CPU usage
top -p $pid

# Check what builder is doing
strace -p $pid (if available)
```

**Solutions**:

1. **Kill stuck builder**:
```bash
pid=$(jq -r '.execution.builders[] | select(.id=="<builder-id>") | .pid' .argo-project/state.json)
kill $pid

# Update state
jq '(.execution.builders[] | select(.id=="<builder-id>")) |= (.status = "failed")' \
   .argo-project/state.json > tmp && mv tmp .argo-project/state.json
```

2. **Check builder timeout** (add if needed):
```bash
# Edit execution.sh to add timeout
timeout 3600 "$CI_TOOL" "$prompt"  # 1 hour timeout
```

3. **Manually complete builder**:
```bash
# Work in the worktree
cd .argo-project/builders/<builder-id>

# Complete the task
# ... make changes ...

git add .
git commit -m "Manual completion"

# Mark as completed
cd ../..
jq '(.execution.builders[] | select(.id=="<builder-id>")) |= (.status = "completed")' \
   .argo-project/state.json > tmp && mv tmp .argo-project/state.json
```

### Builder Fails Immediately

**Symptom**: Builder spawns but fails within seconds.

**Diagnosis**:
```bash
# Check builder result file
cat .argo-project/builders/<builder-id>/.build_result

# Check for CI tool errors
# (depends on CI tool logging)

# Check builder worktree
ls -la .argo-project/builders/<builder-id>/
```

**Solutions**:

1. **CI tool error**: Check CI configuration
```bash
# Test CI tool manually
cd .argo-project/builders/<builder-id>
ci "Test query"
```

2. **Permission issues**: Fix permissions
```bash
chmod -R u+rwX .argo-project/builders/<builder-id>
```

3. **Missing dependencies**: Install required tools
```bash
# Example: node, python, etc.
npm install  # or pip install, etc.
```

## Merge Conflicts

### Detecting Merge Conflicts

**Symptom**: merge_build phase reports conflicts.

**Diagnosis**:
```bash
# Check for conflicts in state
jq -r '.merge_conflicts' .argo-project/state.json

# Check git status
git status

# List conflicting files
git diff --name-only --diff-filter=U
```

### Resolving Conflicts

**Solutions**:

1. **Manual resolution**:
```bash
# Edit conflicting files
vim <conflicting-file>

# Look for conflict markers:
# <<<<<<< HEAD
# ... your changes ...
# =======
# ... their changes ...
# >>>>>>> argo-builder-name

# Resolve conflicts, remove markers, save file

# Mark as resolved
git add <conflicting-file>
git commit

# Update state to continue
jq '.action_owner = "code" | .action_needed = "merge_branches" | del(.merge_conflicts)' \
   .argo-project/state.json > tmp && mv tmp .argo-project/state.json
```

2. **Accept theirs** (builder version):
```bash
git checkout --theirs <conflicting-file>
git add <conflicting-file>
git commit
```

3. **Accept ours** (main version):
```bash
git checkout --ours <conflicting-file>
git add <conflicting-file>
git commit
```

4. **Use merge tool**:
```bash
git mergetool  # Configure with git config merge.tool <tool>
git commit
```

### Preventing Conflicts

**Best Practices**:
- Design components with minimal overlap
- Avoid modifying same files in parallel
- Use clear file/directory boundaries for components
- Review design carefully before execution phase

## CI Tool Issues

### CI Tool Not Found

**Symptom**: "CI tool not found" error.

**Diagnosis**:
```bash
# Check if CI tool exists
which ci
echo $?

# Check PATH
echo $PATH

# Check CI_COMMAND variable
echo $CI_COMMAND
```

**Solutions**:

1. **Install CI tool**:
```bash
# Example for Claude Code
# Follow installation instructions for your CI tool
```

2. **Set CI_COMMAND**:
```bash
export CI_COMMAND=/path/to/ci/tool
./start_workflow.sh "your query"
```

3. **Add to PATH**:
```bash
export PATH=$PATH:/path/to/ci/directory
```

### CI Tool Timeout

**Symptom**: CI queries take too long or hang.

**Diagnosis**:
```bash
# Test CI tool manually
time ci "Test query"

# Check network connectivity (if API-based)
ping api.anthropic.com  # or relevant API endpoint
```

**Solutions**:

1. **Increase timeout** (if configurable in CI tool)
2. **Check network connection**
3. **Use local CI tool** instead of API-based
4. **Reduce query complexity**

## Permission Problems

### Cannot Write to .argo-project

**Symptom**: "Permission denied" errors.

**Diagnosis**:
```bash
# Check directory permissions
ls -ld .argo-project

# Check file permissions
ls -l .argo-project/

# Check ownership
ls -l .argo-project/ | awk '{print $3, $4}'
```

**Solutions**:
```bash
# Fix directory permissions
chmod 755 .argo-project

# Fix file permissions
chmod 644 .argo-project/state.json
chmod 644 .argo-project/logs/workflow.log

# Fix ownership (if needed)
chown -R $USER:$(id -gn) .argo-project
```

### Cannot Execute Phase Handlers

**Symptom**: "Permission denied" when running phase handlers.

**Diagnosis**:
```bash
# Check handler permissions
ls -l phases/

# Try to execute manually
./phases/design_program.sh
```

**Solutions**:
```bash
# Make handlers executable
chmod +x phases/*.sh

# Or specific handler
chmod +x phases/design_program.sh
```

## Resource Issues

### Disk Space Full

**Symptom**: "No space left on device" errors.

**Diagnosis**:
```bash
# Check disk usage
df -h

# Check project directory size
du -sh .argo-project

# Check builder size
du -sh .argo-project/builders/*
```

**Solutions**:

1. **Clean up old builders**:
```bash
rm -rf .argo-project/builders/*
git worktree prune
```

2. **Clean up old logs**:
```bash
rm .argo-project/logs/*.log.old
```

3. **Move project to larger disk**:
```bash
mv /small/disk/project /large/disk/project
cd /large/disk/project
```

### Out of Memory

**Symptom**: Processes killed by OOM killer.

**Diagnosis**:
```bash
# Check memory usage
free -h

# Check for OOM kills
dmesg | grep -i "out of memory\|oom"

# Check swap
swapon --show
```

**Solutions**:

1. **Add swap space**:
```bash
# Create swap file
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

2. **Reduce parallel builders**:
```bash
# Edit state before starting
jq '.execution.builders = [.execution.builders[0], .execution.builders[1]]' \
   .argo-project/state.json > tmp && mv tmp .argo-project/state.json
```

3. **Close other programs**

### Too Many Open Files

**Symptom**: "Too many open files" error.

**Diagnosis**:
```bash
# Check file descriptor limit
ulimit -n

# Check current open files
lsof | wc -l
```

**Solutions**:
```bash
# Increase limit
ulimit -n 4096

# Make permanent (add to ~/.bashrc)
echo "ulimit -n 4096" >> ~/.bashrc
```

## Log Investigation

### Finding Errors in Logs

```bash
# Search for errors
grep -i "error" .argo-project/logs/workflow.log

# Search for specific builder
grep "builder-name" .argo-project/logs/workflow.log

# Show last 50 lines
tail -n 50 .argo-project/logs/workflow.log

# Follow log in real-time
tail -f .argo-project/logs/workflow.log
```

### Understanding Log Entries

```
[2025-11-03T10:05:23Z] [STATE] init → design
  └─ timestamp ─────┘    └─type─┘ └─message─┘
```

**Log Types**:
- `[STATE]` - Phase transitions
- `[BUILDER]` - Builder lifecycle events
- `[MERGE]` - Merge operations
- `[ERROR]` - Errors and failures

### Enabling Debug Logging

```bash
# Add debug statements to phase handlers
echo "DEBUG: variable=$variable" >&2

# Or use set -x for full trace
set -x  # Add to top of handler script
```

## Recovery Procedures

### Complete Workflow Reset

```bash
# 1. Stop orchestrator
kill $(cat .argo-project/orchestrator.pid) 2>/dev/null

# 2. Clean up git worktrees
git worktree list | grep "argo-builder" | awk '{print $1}' | xargs -I {} git worktree remove {} --force
git worktree prune

# 3. Delete builder branches
git branch -a | grep "argo-builder" | xargs -I {} git branch -D {}

# 4. Remove .argo-project
rm -rf .argo-project

# 5. Reinitialize
../templates/project_setup.sh "my-project" .

# 6. Start fresh
./start_workflow.sh "your query"
```

### Partial Recovery (Preserve State)

```bash
# 1. Stop orchestrator
kill $(cat .argo-project/orchestrator.pid) 2>/dev/null

# 2. Backup state
cp .argo-project/state.json .argo-project/state.json.backup

# 3. Fix specific issue (see relevant section above)

# 4. Restart orchestrator
orchestrator.sh $(pwd)
```

### Resume from Checkpoint

```bash
# 1. Check current phase
jq -r '.phase' .argo-project/state.json

# 2. Set action_owner to resume
jq '.action_owner = "code"' .argo-project/state.json > tmp && mv tmp .argo-project/state.json

# 3. Restart orchestrator
orchestrator.sh $(pwd)
```

---

## Getting Help

If you've tried these troubleshooting steps and still have issues:

1. **Check Documentation**:
   - [README.md](README.md) - Overview
   - [USER_GUIDE.md](USER_GUIDE.md) - User guide
   - [ARCHITECTURE.md](ARCHITECTURE.md) - Technical details
   - [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) - Phase handler development

2. **Collect Information**:
```bash
# System info
uname -a
bash --version
jq --version
git --version

# Project state
jq . .argo-project/state.json

# Recent logs
tail -n 100 .argo-project/logs/workflow.log

# Git status
git status
git worktree list
```

3. **Create Minimal Reproduction**:
   - Simplest query that reproduces issue
   - Steps to reproduce
   - Expected vs actual behavior

4. **Check Test Suite**:
```bash
# Run tests to verify system health
cd tests
./run_phase2_tests.sh
```

If all tests pass, the issue is likely configuration or environment-specific.
