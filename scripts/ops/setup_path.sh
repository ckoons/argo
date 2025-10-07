#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Setup PATH for Argo binaries in ~/.local/bin
# Usage: ./scripts/setup_path.sh [--auto]

TARGET_DIR="$HOME/.local/bin"

# Check if already in PATH
if echo "$PATH" | grep -q "$TARGET_DIR"; then
    echo "✓ $TARGET_DIR already in PATH"
    exit 0
fi

# Detect shell config file
if [ -n "$ZSH_VERSION" ]; then
    RC_FILE="$HOME/.zshrc"
elif [ -n "$BASH_VERSION" ]; then
    RC_FILE="$HOME/.bashrc"
elif [ -f "$HOME/.zshrc" ]; then
    RC_FILE="$HOME/.zshrc"
elif [ -f "$HOME/.bashrc" ]; then
    RC_FILE="$HOME/.bashrc"
else
    RC_FILE="$HOME/.profile"
fi

# Auto mode - add to RC file
if [ "$1" = "--auto" ]; then
    echo "" >> "$RC_FILE"
    echo '# Added by Argo install' >> "$RC_FILE"
    echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$RC_FILE"
    echo ""
    echo "✓ Added to $RC_FILE"
    echo ""
    echo "To activate in current shell, run:"
    echo "  source $RC_FILE"
    echo ""
    echo "Or restart your shell"
    exit 0
fi

# Manual mode - show instructions
echo ""
echo "⚠️  $TARGET_DIR is not in your PATH"
echo ""
echo "To use the installed Argo binaries, add this to your $RC_FILE:"
echo ""
echo '  export PATH="$HOME/.local/bin:$PATH"'
echo ""
echo "Then restart your shell or run:"
echo "  source $RC_FILE"
echo ""
echo "Or to add it automatically, run:"
echo "  ./scripts/setup_path.sh --auto"
echo ""
