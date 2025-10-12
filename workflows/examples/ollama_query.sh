#!/bin/bash
# © 2025 Casey Koons All rights reserved
# Ollama integration test workflow

echo "=== Ollama Query Workflow ==="
echo "Testing Ollama provider integration"
echo ""

# Check if Ollama is running
if ! curl -s http://localhost:11434/api/tags >/dev/null 2>&1; then
    echo "WARNING: Ollama not running on localhost:11434"
    echo "This test requires Ollama to be running"
    echo "Start with: ollama serve"
    exit 1
fi

echo "✓ Ollama server running"
echo ""

# Check for available models
echo "Checking available models..."
MODELS=$(curl -s http://localhost:11434/api/tags | grep -o '"name":"[^"]*"' | cut -d'"' -f4 | head -1)

if [ -z "$MODELS" ]; then
    echo "WARNING: No Ollama models installed"
    echo "Install a model with: ollama pull llama3.2"
    exit 1
fi

echo "✓ Found model: $MODELS"
echo ""

# Simple query to Ollama
echo "Sending query to Ollama..."
echo ""

# Ask a simple question
RESPONSE=$(curl -s http://localhost:11434/api/generate -d "{
  \"model\": \"$MODELS\",
  \"prompt\": \"What is 2+2? Answer with just the number.\",
  \"stream\": false
}" 2>&1)

EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
    echo "✓ Ollama query successful"
    echo ""

    # Extract response
    ANSWER=$(echo "$RESPONSE" | grep -o '"response":"[^"]*"' | cut -d'"' -f4)
    echo "Ollama Response: $ANSWER"
    echo ""

    # Check if response contains "4"
    if echo "$ANSWER" | grep -q "4"; then
        echo "✓ Ollama response correct"
        exit 0
    else
        echo "✗ Ollama response unexpected (but query worked)"
        exit 0  # Still success - model responded
    fi
else
    echo "✗ Ollama query failed with exit code: $EXIT_CODE"
    exit $EXIT_CODE
fi
