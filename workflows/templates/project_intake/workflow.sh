#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# project_intake - Classify project type and initialize workspace
#
# This workflow is the entry point for all Argo projects. It determines:
# - Project type (tool/program/project)
# - New vs. existing project
# - Directory setup
# - Git/GitHub initialization
# - Session state creation
#
# Then hands off to design_program for the design phase.

set -e

# Get workflow root directory
WORKFLOW_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATES_DIR="$(dirname "$WORKFLOW_DIR")"
ARGO_ROOT="$(dirname "$TEMPLATES_DIR")"
LIB_DIR="$ARGO_ROOT/lib"

# Source required libraries
source "$LIB_DIR/arc-colors.sh"
source "$LIB_DIR/state-manager.sh"
source "$LIB_DIR/git-helpers.sh"

# Source configuration
source "$WORKFLOW_DIR/config/defaults.env"

# Setup colors
setup_colors

# ============================================================================
# Phase 1: Welcome & Overview
# ============================================================================

show_welcome() {
    clear
    box_header "Welcome to Argo Project Intake" "$SPARKLES"
    echo ""
    echo -e "${WHITE}This workflow will help you:${NC}"
    echo ""
    list_item "Classify your project type"
    list_item "Set up your workspace"
    list_item "Initialize version control (if needed)"
    list_item "Create a session for tracking progress"
    echo ""
    echo -e "${GRAY}Let's get started!${NC}"
    echo ""
}

# ============================================================================
# Phase 2: Project Type Classification
# ============================================================================

classify_project_type() {
    log_phase "Project Type"
    echo ""
    echo -e "${WHITE}What would you like to build?${NC}"
    echo ""

    list_item "1" "$(label Tool) - Single-file utility (calculator, formatter, converter)"
    list_item "2" "$(label Program) - Multi-file application (pipeline, batch system)"
    list_item "3" "$(label Project) - GitHub-integrated software (web app, API, library)"
    echo ""

    local choice
    while true; do
        question "Select project type (1-3)"
        read -r choice

        case "$choice" in
            1)
                PROJECT_TYPE="tool"
                break
                ;;
            2)
                PROJECT_TYPE="program"
                break
                ;;
            3)
                PROJECT_TYPE="project"
                break
                ;;
            *)
                error "Invalid choice. Please enter 1, 2, or 3."
                ;;
        esac
    done

    success "Project type: $(label $PROJECT_TYPE)"
    echo ""
}

# ============================================================================
# Phase 3: New vs. Existing Project
# ============================================================================

determine_new_or_existing() {
    log_phase "Project Source"
    echo ""

    local choice
    while true; do
        question "Is this a new or existing project?"
        echo -e "  ${CYAN}1${NC} - New (start from scratch)"
        echo -e "  ${CYAN}2${NC} - Existing (clone/fork from GitHub)"
        read -r choice

        case "$choice" in
            1)
                IS_NEW_PROJECT="yes"
                break
                ;;
            2)
                IS_NEW_PROJECT="no"
                break
                ;;
            *)
                error "Invalid choice. Please enter 1 or 2."
                ;;
        esac
    done

    if [[ "$IS_NEW_PROJECT" == "yes" ]]; then
        success "Creating new project"
    else
        success "Working with existing project"
    fi
    echo ""
}

# ============================================================================
# Phase 4: Project Name
# ============================================================================

get_project_name() {
    log_phase "Project Name"
    echo ""

    local valid_name=false
    while [[ "$valid_name" == false ]]; do
        question "What is the project name?"
        read -r PROJECT_NAME

        # Validate name
        if [[ -z "$PROJECT_NAME" ]]; then
            error "Project name cannot be empty"
            continue
        fi

        if [[ ${#PROJECT_NAME} -gt $MAX_PROJECT_NAME_LENGTH ]]; then
            error "Project name too long (max $MAX_PROJECT_NAME_LENGTH characters)"
            continue
        fi

        if [[ ! "$PROJECT_NAME" =~ ^[a-zA-Z0-9_-]+$ ]]; then
            error "Project name can only contain letters, numbers, hyphens, and underscores"
            continue
        fi

        valid_name=true
    done

    success "Project name: $(label $PROJECT_NAME)"
    echo ""
}

# ============================================================================
# Phase 5: Directory Setup
# ============================================================================

setup_directory() {
    log_phase "Directory Setup"
    echo ""

    # Determine default directory based on project type
    if [[ "$PROJECT_TYPE" == "tool" ]]; then
        DEFAULT_DIR="$DEFAULT_TOOLS_DIR/$PROJECT_NAME"
    else
        DEFAULT_DIR="$DEFAULT_PROJECTS_DIR/$PROJECT_NAME"
    fi

    # Ask user for directory
    echo -e "${WHITE}Where should the project be located?${NC}"
    echo -e "${GRAY}Default: $(path $DEFAULT_DIR)${NC}"
    echo -e "${GRAY}Press Enter to use default, or type a custom path:${NC}"
    read -r custom_dir

    if [[ -n "$custom_dir" ]]; then
        PROJECT_DIR="$custom_dir"
    else
        PROJECT_DIR="$DEFAULT_DIR"
    fi

    # Expand tilde
    PROJECT_DIR="${PROJECT_DIR/#\~/$HOME}"

    # Check if directory exists
    if [[ -d "$PROJECT_DIR" ]]; then
        if [[ "$IS_NEW_PROJECT" == "yes" ]]; then
            warning "Directory already exists: $(path $PROJECT_DIR)"
            question "Overwrite? (yes/no)"
            read -r overwrite

            if [[ "$overwrite" != "yes" ]]; then
                error "Aborted. Please choose a different directory or name."
                exit 1
            fi

            rm -rf "$PROJECT_DIR"
        fi
    fi

    # Create directory
    if [[ ! -d "$PROJECT_DIR" ]]; then
        mkdir -p "$PROJECT_DIR"
        success "Created directory: $(path $PROJECT_DIR)"
    else
        success "Using directory: $(path $PROJECT_DIR)"
    fi

    cd "$PROJECT_DIR"
    echo ""
}

# ============================================================================
# Phase 6: GitHub Operations (if existing project)
# ============================================================================

handle_github_existing() {
    if [[ "$IS_NEW_PROJECT" == "no" ]]; then
        log_phase "GitHub Clone/Fork"
        echo ""

        question "Enter GitHub repository URL:"
        read -r REPO_URL

        if [[ -z "$REPO_URL" ]]; then
            error "Repository URL is required for existing projects"
            exit 1
        fi

        # Ask if fork or clone
        question "Fork or clone? (fork/clone)"
        read -r fork_or_clone

        if [[ "$fork_or_clone" == "fork" ]]; then
            step "Forking repository..."
            if gh_fork_repo "$REPO_URL" "yes"; then
                success "Repository forked and cloned"
            else
                error "Failed to fork repository"
                exit 1
            fi
        else
            step "Cloning repository..."
            if gh_clone_repo "$REPO_URL" "$PROJECT_DIR"; then
                success "Repository cloned"
            else
                error "Failed to clone repository"
                exit 1
            fi
        fi

        cd "$PROJECT_DIR"
        IS_GIT_REPO="yes"
        IS_GITHUB_PROJECT="yes"
        echo ""
    fi
}

# ============================================================================
# Phase 7: Git Initialization (if new project and not tool)
# ============================================================================

handle_git_initialization() {
    if [[ "$IS_NEW_PROJECT" == "yes" ]]; then
        if [[ "$PROJECT_TYPE" == "tool" ]]; then
            IS_GIT_REPO="no"
            IS_GITHUB_PROJECT="no"
        elif [[ "$PROJECT_TYPE" == "program" ]]; then
            log_phase "Version Control"
            echo ""
            question "Initialize git repository? (yes/no)"
            read -r init_git

            if [[ "$init_git" == "yes" ]]; then
                step "Initializing git repository..."
                if git_init_repo "."; then
                    success "Git repository initialized"
                    IS_GIT_REPO="yes"
                else
                    warning "Failed to initialize git repository"
                    IS_GIT_REPO="no"
                fi
            else
                IS_GIT_REPO="no"
            fi

            IS_GITHUB_PROJECT="no"
            echo ""
        else  # project
            log_phase "Version Control"
            echo ""
            step "Initializing git repository..."

            if git_init_repo "."; then
                success "Git repository initialized"
                IS_GIT_REPO="yes"
            else
                error "Failed to initialize git repository"
                exit 1
            fi

            # Ask about GitHub
            question "Create GitHub repository? (yes/no)"
            read -r create_github

            if [[ "$create_github" == "yes" ]]; then
                question "Is the repository public or private? (public/private)"
                read -r visibility

                step "Creating GitHub repository..."
                if gh repo create "$PROJECT_NAME" "--${visibility}" --confirm; then
                    success "GitHub repository created"
                    IS_GITHUB_PROJECT="yes"

                    # Add remote
                    git_add_remote "https://github.com/$(gh api user --jq .login)/$PROJECT_NAME.git"
                else
                    warning "Failed to create GitHub repository"
                    IS_GITHUB_PROJECT="no"
                fi
            else
                IS_GITHUB_PROJECT="no"
            fi

            echo ""
        fi
    fi
}

# ============================================================================
# Phase 8: Main Branch Setup
# ============================================================================

setup_main_branch() {
    if [[ "$IS_GIT_REPO" == "yes" ]]; then
        # Determine main branch
        MAIN_BRANCH=$(git_current_branch 2>/dev/null || echo "$DEFAULT_MAIN_BRANCH")

        if [[ -z "$MAIN_BRANCH" ]]; then
            MAIN_BRANCH="$DEFAULT_MAIN_BRANCH"
        fi
    else
        MAIN_BRANCH=""
    fi
}

# ============================================================================
# Phase 9: Session Creation
# ============================================================================

create_project_session() {
    log_phase "Session Creation"
    echo ""

    step "Creating session state..."
    SESSION_ID=$(create_session "$PROJECT_NAME")

    if [[ -z "$SESSION_ID" ]]; then
        error "Failed to create session"
        exit 1
    fi

    success "Session created: $(label $SESSION_ID)"

    # Save project context
    local context_json=$(cat <<EOF
{
  "session_id": "$SESSION_ID",
  "project_name": "$PROJECT_NAME",
  "project_type": "$PROJECT_TYPE",
  "project_dir": "$PROJECT_DIR",
  "is_new_project": "$IS_NEW_PROJECT",
  "is_git_repo": "$IS_GIT_REPO",
  "is_github_project": "$IS_GITHUB_PROJECT",
  "main_branch": "$MAIN_BRANCH",
  "feature_branch": "",
  "parallel_builds": false,
  "builder_count": 0,
  "current_phase": "intake_complete",
  "created": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
}
EOF
)

    save_context "$SESSION_ID" "$context_json"
    success "Context saved"
    echo ""
}

# ============================================================================
# Phase 10: Summary & Handoff
# ============================================================================

show_summary() {
    banner_summary "Project Intake Complete!"
    echo ""

    list_header "Configuration:"
    list_item "Project name:" "$(label $PROJECT_NAME)"
    list_item "Project type:" "$(label $PROJECT_TYPE)"
    list_item "Directory:" "$(path $PROJECT_DIR)"
    list_item "Git repository:" "$IS_GIT_REPO"
    list_item "GitHub project:" "$IS_GITHUB_PROJECT"
    list_item "Session ID:" "$(label $SESSION_ID)"
    echo ""

    list_header "Next steps:"
    next_steps "Run design phase: $(cmd arc start $NEXT_WORKFLOW)"
    next_steps "Session will be automatically loaded"
    echo ""
}

# ============================================================================
# Main Workflow
# ============================================================================

main() {
    show_welcome
    classify_project_type
    determine_new_or_existing
    get_project_name
    setup_directory
    handle_github_existing
    handle_git_initialization
    setup_main_branch
    create_project_session
    show_summary

    # Export session for next workflow
    echo "$SESSION_ID" > "${HOME}/.argo/.last_session"
}

# Run workflow
main "$@"
