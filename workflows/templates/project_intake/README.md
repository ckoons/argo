# project_intake

© 2025 Casey Koons All rights reserved

Classify project type and initialize workspace for Argo workflows.

## Description

This workflow is the entry point for all Argo projects. It guides you through a deterministic question flow to classify your project, set up the workspace, and create a session for tracking progress.

## Purpose

- Classify project type (tool/program/project)
- Determine if new or existing project
- Set up project directory
- Initialize git repository (if needed)
- Clone/fork GitHub repository (if existing)
- Create session state for workflow tracking
- Hand off to design_program workflow

## Usage

```bash
# Start project intake
arc start project_intake my_project_session
```

## Workflow Phases

### Phase 1: Welcome & Overview
- Displays welcome banner
- Explains what the workflow will do

### Phase 2: Project Type Classification
User chooses one of three project types:

**1. Tool** - Single-file utility
- Location: `~/.argo/tools/{name}/`
- No version control
- Single builder (no parallelization)
- Examples: Calculator, text formatter, file converter

**2. Program** - Multi-file application
- Location: User-specified (default: `~/projects/github/{name}/`)
- Optional git repository
- Can parallelize if complex
- Examples: Data pipeline, batch job system

**3. Project** - GitHub-integrated software
- Location: User-specified (default: `~/projects/github/{name}/`)
- Full git repository with remote
- Parallel builders with feature branches
- Examples: Web application, API service, library

### Phase 3: New vs. Existing Project
Determines if:
- Starting from scratch (new)
- Cloning/forking existing GitHub repository

### Phase 4: Project Name
- Prompts for project name
- Validates name (alphanumeric, hyphens, underscores)
- Enforces length limit (64 characters)

### Phase 5: Directory Setup
- Determines default directory based on project type
- Allows custom directory path
- Creates directory structure
- Handles existing directories (overwrite confirmation)

### Phase 6: GitHub Operations (if existing)
For existing projects:
- Prompts for GitHub repository URL
- Offers fork or clone option
- Uses `gh` CLI to perform operation
- Changes to cloned directory

### Phase 7: Git Initialization (if new)
Behavior varies by project type:

**Tool**: No git initialization (tools are simple, single-file)

**Program**: Optional git initialization
- Asks user if they want version control
- Creates local repository if yes
- No GitHub integration

**Project**: Required git initialization
- Creates local repository
- Offers to create GitHub repository
- Adds remote if GitHub created
- Uses `gh` CLI for GitHub operations

### Phase 8: Main Branch Setup
- Determines main branch name
- Defaults to "main"
- Detects existing branch if cloned

### Phase 9: Session Creation
- Generates unique session ID: `proj-YYYYMMDD-HHMMSS`
- Creates session directory structure
- Saves project context as JSON
- Includes all configuration details

### Phase 10: Summary & Handoff
- Displays configuration summary
- Shows next steps
- Saves session ID for next workflow
- Ready for design_program handoff

## Output Structure

### Session Directory
```
~/.argo/sessions/{session_id}/
├── context.json           # Project configuration
└── builders/              # Builder state (created later)
```

### Context Schema
```json
{
  "session_id": "proj-20251022-143022",
  "project_name": "myapp",
  "project_type": "project",
  "project_dir": "/Users/cskoons/projects/github/myapp",
  "is_new_project": "yes",
  "is_git_repo": "yes",
  "is_github_project": "yes",
  "main_branch": "main",
  "feature_branch": "",
  "parallel_builds": false,
  "builder_count": 0,
  "current_phase": "intake_complete",
  "created": "2025-10-22T14:30:22Z"
}
```

### Last Session Marker
```
~/.argo/.last_session       # Contains session ID for next workflow
```

## Integration with Other Workflows

### Handoff to design_program
After completion, the workflow:
1. Exports session ID to `~/.argo/.last_session`
2. User runs: `arc start design_program`
3. design_program automatically loads session context
4. Design phase begins with full project knowledge

### Session State Usage
Subsequent workflows read context to determine:
- Project type (affects parallelization decisions)
- Directory location (where to build)
- Git status (whether to use branches)
- GitHub status (whether to create PRs)

## Examples

### Example 1: Create a Simple Tool
```bash
arc start project_intake calc_session

# Questions:
# What would you like to build? 1 (Tool)
# Is this new or existing? 1 (New)
# Project name? calculator
# Directory? [Press Enter for ~/.argo/tools/calculator]

# Result:
# - Directory: ~/.argo/tools/calculator
# - No git repository
# - Session: proj-20251022-143022
```

### Example 2: Create a Program with Git
```bash
arc start project_intake pipeline_session

# Questions:
# What would you like to build? 2 (Program)
# Is this new or existing? 1 (New)
# Project name? data_pipeline
# Directory? [Press Enter for ~/projects/github/data_pipeline]
# Initialize git? yes

# Result:
# - Directory: ~/projects/github/data_pipeline
# - Local git repository
# - No GitHub integration
# - Session: proj-20251022-143525
```

### Example 3: Fork Existing GitHub Project
```bash
arc start project_intake webapp_session

# Questions:
# What would you like to build? 3 (Project)
# Is this new or existing? 2 (Existing)
# Project name? mywebapp
# Repository URL? https://github.com/someuser/webapp
# Fork or clone? fork

# Result:
# - Forked and cloned to ~/projects/github/mywebapp
# - Full git history
# - Remote configured
# - Session: proj-20251022-144012
```

### Example 4: Create New GitHub Project
```bash
arc start project_intake api_session

# Questions:
# What would you like to build? 3 (Project)
# Is this new or existing? 1 (New)
# Project name? my_api
# Directory? [Press Enter for ~/projects/github/my_api]
# Create GitHub repository? yes
# Public or private? public

# Result:
# - Directory: ~/projects/github/my_api
# - Git repository initialized
# - GitHub repository created
# - Remote added (origin)
# - Session: proj-20251022-144523
```

## Error Handling

**Invalid project name**: Re-prompts until valid name provided

**Directory already exists (new project)**: Asks for overwrite confirmation

**GitHub operations fail**: Reports error and continues (allows manual setup)

**Git initialization fails**: Reports error and exits (critical for projects)

**Session creation fails**: Reports error and exits (required for workflow tracking)

## Dependencies

- **State management library** (`workflows/lib/state-manager.sh`)
- **Git helpers library** (`workflows/lib/git-helpers.sh`)
- **Color library** (`workflows/lib/arc-colors.sh`)
- **gh CLI** (for GitHub operations) - optional, but required for GitHub features
- **git** (for version control) - optional for tools/programs, required for projects
- **jq** (for JSON processing)

## Configuration

Edit `config/defaults.env` to change:
- Default tools directory
- Default projects directory
- Default main branch name
- Next workflow in chain

## Author

Created by Casey Koons on 2025-10-22

---

Part of the Argo parallel development workflow system
