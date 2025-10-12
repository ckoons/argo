#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Interactive workflow example - prompts user for input

echo "=== Interactive Workflow Example ==="
echo "This workflow demonstrates user interaction"
echo ""

# Prompt for name
echo -n "What is your name? "
read name
echo "Hello, $name!"
echo ""

# Prompt for favorite color
echo -n "What is your favorite color? "
read color
echo "Nice choice! $color is a great color."
echo ""

# Prompt for age
echo -n "How old are you? "
read age
echo "You are $age years old."
echo ""

# Summary
echo "=== Summary ==="
echo "Name:  $name"
echo "Color: $color"
echo "Age:   $age"
echo ""

echo "=== Workflow Complete ==="
exit 0
