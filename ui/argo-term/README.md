# Argo-Term - Terminal UI for Argo Workflows

© 2025 Casey Koons. All rights reserved.

**Interactive terminal dashboard for multi-AI workflow visualization**

## What is Argo-Term?

Argo-Term provides a real-time, interactive terminal interface for monitoring and controlling Argo workflows. See CI status, workflow progress, and inter-AI communication in a clean, ncurses-based dashboard.

Perfect for:
- Development monitoring
- CI/CD dashboards
- Remote workflow management
- Multi-AI coordination oversight

## Features

### Real-Time Monitoring
- Live workflow status updates
- CI health and activity display
- Inter-AI message streaming
- Resource utilization tracking

### Interactive Control
- Pause/resume workflows
- Start/stop CIs
- Navigate between panels
- Drill down into details

### Visual Clarity
- Color-coded status indicators
- Progress bars for workflows
- Activity indicators for CIs
- Clean, organized layout

## Quick Start

```bash
# Build argo-term (requires argo core and arc)
cd ui/argo-term
make

# Run the UI
../../build/argo-term

# Run with specific registry
../../build/argo-term --registry .argo/registry.json
```

## Interface Overview

```
┌─────────────────────────────────────────────────────┐
│ Argo Multi-AI Workflow Monitor                      │
├─────────────────────────────────────────────────────┤
│ Workflows (3 active)          │  CI Registry (5)    │
│ ● build-123  [DEVELOP] 60%    │  ● builder  [BUSY]  │
│ ● test-456   [TEST]    100%   │  ● tester   [READY] │
│ ○ deploy-789 [PAUSED]  45%    │  ● reviewer [READY] │
│                                │  ○ deployer [OFFLINE]│
│ [r]efresh [p]ause [q]uit       │  ○ monitor  [ERROR] │
├─────────────────────────────────────────────────────┤
│ Messages (5 new)                                     │
│ [12:34] builder -> tester: "Tests ready for review" │
│ [12:35] tester -> reviewer: "Running unit tests..." │
│                                                      │
└─────────────────────────────────────────────────────┘
```

## Key Bindings

### Global
- `q` - Quit
- `r` - Refresh all data
- `h` - Show help
- `Tab` - Next panel
- `Shift-Tab` - Previous panel

### Workflow Panel
- `p` - Pause selected workflow
- `s` - Resume selected workflow
- `Enter` - Show workflow details
- `↑/↓` or `k/j` - Navigate list

### CI Panel
- `Enter` - Show CI details
- `t` - Start/stop CI
- `↑/↓` or `k/j` - Navigate list

### Message Panel
- `c` - Clear messages
- `↑/↓` or `k/j` - Scroll history

## Status Indicators

### Workflow Status
- `●` (green) - Running
- `◐` (yellow) - Paused
- `○` (white) - Idle
- `✗` (red) - Failed
- `✓` (green) - Completed

### CI Status
- `●` (green) - Busy/Active
- `○` (cyan) - Ready
- `○` (white) - Starting
- `○` (gray) - Offline
- `✗` (red) - Error

## Configuration

Argo-Term reads settings from `~/.argorc`:

```bash
# UI Settings
UI_UPDATE_INTERVAL=1000   # Refresh interval (ms)
UI_COLOR_THEME=default    # Color scheme
UI_SHOW_TIMESTAMPS=1      # Show message timestamps
```

## Command Line Options

```bash
argo-term [OPTIONS]

Options:
  --registry <file>     Registry file to monitor
  --update <ms>         Update interval in milliseconds (default: 1000)
  --no-color            Disable color output
  --help                Show this help
```

## Terminal Requirements

### Minimum
- 80x24 character display
- ANSI terminal emulation
- Keyboard input support

### Recommended
- 120x40 or larger
- 256-color support
- UTF-8 encoding
- Modern terminal emulator

### Tested Terminals
- xterm, gnome-terminal (Linux)
- Terminal.app, iTerm2 (macOS)
- Windows Terminal (Windows)
- tmux, screen (multiplexers)

## Dependencies

- Argo core library (libargo_core.a)
- ncurses library (`-lncurses`)
- POSIX system
- C11 compiler

## Building

```bash
# From ui/argo-term directory
make              # Build argo-term
make test         # Run tests
make clean        # Clean build artifacts
make count        # Check code size (3000 line budget)
```

## Development

See [CLAUDE.md](CLAUDE.md) for UI-specific coding standards and ncurses patterns.

**Budget:** 3,000 lines (src/ only)

## Troubleshooting

### UI is garbled
```bash
# Reset terminal
reset
# Or
tput reset
```

### Colors don't work
```bash
# Check TERM variable
echo $TERM
# Should be xterm-256color or similar

# Set if needed
export TERM=xterm-256color
```

### Resize issues
```bash
# Argo-Term handles SIGWINCH
# If issues persist, restart the UI
```

---

*Argo-Term: See your AI workflows in action*
