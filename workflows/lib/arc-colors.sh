#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# arc-colors.sh - Shared color library for Argo workflows
#
# Provides consistent, beautiful colorized output across all templates.
#
# Usage:
#   source "$(dirname "$0")/../../lib/arc-colors.sh"
#   setup_colors
#   log_phase "Phase 1: Getting Started"
#   prompt "What is your name?"
#   success "Template created!"

# Setup colors (detects if terminal supports colors)
setup_colors() {
    if [[ -t 1 ]] && [[ "${NO_COLOR:-}" != "1" ]]; then
        # Basic colors
        export RED='\033[0;31m'
        export GREEN='\033[0;32m'
        export YELLOW='\033[1;33m'
        export BLUE='\033[0;34m'
        export MAGENTA='\033[0;35m'
        export CYAN='\033[0;36m'
        export WHITE='\033[1;37m'
        export GRAY='\033[0;90m'

        # Styles
        export BOLD='\033[1m'
        export DIM='\033[2m'
        export NC='\033[0m'  # No Color (reset)

        # Unicode symbols
        export CHECK="✓"
        export CROSS="✗"
        export WARN="⚠"
        export ARROW="❯"
        export BULLET="•"
        export ROBOT="🤖"
    else
        # No colors if not a terminal or NO_COLOR set
        export RED='' GREEN='' YELLOW='' BLUE='' MAGENTA=''
        export CYAN='' WHITE='' GRAY='' BOLD='' DIM='' NC=''
        export CHECK="+" CROSS="x" WARN="!" ARROW=">" BULLET="-" ROBOT="AI"
    fi
}

# ============================================================================
# Logging Functions
# ============================================================================

# General log message (with workflow name prefix)
log() {
    local workflow_name="${WORKFLOW_NAME:-workflow}"
    echo -e "${GRAY}[${workflow_name}]${NC} $*"
}

# Phase header (large, prominent)
log_phase() {
    echo ""
    echo -e "${CYAN}${BOLD}━━━ $* ━━━${NC}"
    echo ""
}

# Success message
success() {
    echo -e "${GREEN}${CHECK}${NC} ${BOLD}$*${NC}"
}

# Warning message
warning() {
    echo -e "${YELLOW}${WARN}${NC} ${YELLOW}$*${NC}"
}

# Error message
error() {
    echo -e "${RED}${CROSS}${NC} ${RED}${BOLD}$*${NC}"
}

# Info message (subtle)
info() {
    echo -e "${GRAY}${BULLET}${NC} $*"
}

# Step message (numbered progress)
step() {
    local num="$1"
    shift
    echo -e "${CYAN}Step ${num}:${NC} $*"
}

# ============================================================================
# Interactive Prompts
# ============================================================================

# Prompt for user input (yellow arrow)
prompt() {
    echo -e "${YELLOW}${ARROW}${NC} ${WHITE}$*${NC}"
}

# Question prompt (numbered)
question() {
    local num="$1"
    shift
    echo ""
    echo -e "${YELLOW}${num}.${NC} ${WHITE}$*${NC}"
}

# ============================================================================
# Display Helpers
# ============================================================================

# Box header (for AI output, summaries, etc.)
box_header() {
    local title="$1"
    local icon="${2:-${ROBOT}}"
    echo ""
    echo -e "${MAGENTA}╔═══════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${MAGENTA}║${NC} ${BOLD}${CYAN}${icon} ${title}${NC}"
    echo -e "${MAGENTA}╚═══════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

# Box footer (closing line)
box_footer() {
    echo ""
    echo -e "${MAGENTA}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
}

# Simple separator
separator() {
    echo -e "${GRAY}────────────────────────────────────────────────────────────────────${NC}"
}

# ============================================================================
# List Helpers
# ============================================================================

# List header
list_header() {
    echo ""
    echo -e "${BOLD}${WHITE}$*${NC}"
}

# List item (with checkmark)
list_item() {
    local name="$1"
    local desc="${2:-}"
    if [[ -n "$desc" ]]; then
        echo -e "  ${GREEN}${CHECK}${NC} ${CYAN}${name}${NC}  ${GRAY}${desc}${NC}"
    else
        echo -e "  ${GREEN}${CHECK}${NC} ${CYAN}${name}${NC}"
    fi
}

# List item (pending, no checkmark)
list_pending() {
    local name="$1"
    local desc="${2:-}"
    if [[ -n "$desc" ]]; then
        echo -e "  ${GRAY}${BULLET}${NC} ${name}  ${GRAY}${desc}${NC}"
    else
        echo -e "  ${GRAY}${BULLET}${NC} ${name}"
    fi
}

# ============================================================================
# Path and Command Highlighting
# ============================================================================

# Highlight file path
path() {
    echo -e "${CYAN}$*${NC}"
}

# Highlight command
cmd() {
    echo -e "${GREEN}$*${NC}"
}

# Highlight argument
arg() {
    echo -e "${YELLOW}$*${NC}"
}

# Code block (inline)
code() {
    echo -e "${DIM}${CYAN}$*${NC}"
}

# ============================================================================
# Banner Helpers
# ============================================================================

# Welcome banner
banner_welcome() {
    local title="$1"
    local subtitle="${2:-}"
    echo ""
    echo -e "${BOLD}${MAGENTA}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BOLD}${MAGENTA}║${NC}  ${BOLD}${WHITE}${title}${NC}"
    if [[ -n "$subtitle" ]]; then
        echo -e "${BOLD}${MAGENTA}║${NC}  ${GRAY}${subtitle}${NC}"
    fi
    echo -e "${BOLD}${MAGENTA}╚════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

# Summary banner
banner_summary() {
    local title="$1"
    echo ""
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║${NC}  ${BOLD}${GREEN}${CHECK} ${title}${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

# ============================================================================
# Next Steps Display
# ============================================================================

# Show numbered next steps
next_steps() {
    local title="${1:-Next steps}"
    shift
    echo ""
    echo -e "${BOLD}${WHITE}${title}:${NC}"
    local num=1
    for step in "$@"; do
        echo -e "  ${YELLOW}${num}.${NC} ${step}"
        ((num++))
    done
    echo ""
}

# Show command to run next
show_command() {
    local description="$1"
    local command="$2"
    echo -e "  ${description}:"
    echo -e "    ${GREEN}${command}${NC}"
    echo ""
}

# ============================================================================
# Progress Indicators
# ============================================================================

# Simple progress (X/Y format)
progress() {
    local current="$1"
    local total="$2"
    local desc="${3:-}"
    echo -e "${CYAN}[${current}/${total}]${NC} ${desc}"
}

# ============================================================================
# Markdown Syntax Highlighting (Basic)
# ============================================================================

# Colorize markdown headings in output
colorize_markdown() {
    sed -e "s/^## \(.*\)/${CYAN}${BOLD}## \1${NC}/g" \
        -e "s/^### \(.*\)/${BLUE}${BOLD}### \1${NC}/g" \
        -e "s/^\*\*\(.*\)\*\*/${BOLD}\1${NC}/g" \
        -e "s/^\* \(.*\)/${GRAY}•${NC} \1/g" \
        -e "s/^- \(.*\)/${GRAY}•${NC} \1/g" \
        -e "s/\`\([^\`]*\)\`/${CYAN}\1${NC}/g"
}

# ============================================================================
# Initialization
# ============================================================================

# Auto-setup colors when sourced (can be disabled by setting ARC_NO_AUTO_COLOR=1)
if [[ "${ARC_NO_AUTO_COLOR:-}" != "1" ]]; then
    setup_colors
fi
