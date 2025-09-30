#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Count meaningful lines in argo core
# Excludes: comments, blank lines, logs, prints, braces, provider duplicates

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC_DIR="$PROJECT_ROOT/src"

echo "Argo Core Line Count (Diet-Aware)"
echo "=================================="
echo ""

# Function to count meaningful lines in a file
count_meaningful_lines() {
    local file=$1

    # Remove:
    # - C comments (/* */ and //)
    # - Blank lines
    # - Lines with only braces
    # - LOG_* lines
    # - printf/fprintf lines

    cat "$file" | \
        sed 's|/\*.*\*/||g' | \
        sed 's|//.*$||' | \
        grep -v '^ *$' | \
        grep -v '^ *[{}] *$' | \
        grep -v '^ *} *$' | \
        grep -v '^ *{ *$' | \
        grep -v 'LOG_[A-Z]*(' | \
        grep -v 'printf(' | \
        grep -v 'fprintf(' | \
        grep -v 'snprintf.*\.\.\.' | \
        grep -v '^ *; *$' | \
        wc -l | tr -d ' '
}

# Temporary files for provider tracking
provider_list=$(mktemp)
trap "rm -f $provider_list" EXIT

# Count all src/*.c files
total=0

echo "Core Implementation Files:"
echo "--------------------------"

for file in "$SRC_DIR"/*.c; do
    [ -f "$file" ] || continue

    basename=$(basename "$file")
    meaningful=$(count_meaningful_lines "$file")
    actual=$(wc -l < "$file" | tr -d ' ')
    savings=$((actual - meaningful))
    percent=$((savings * 100 / actual))

    # Check if it's a provider implementation
    if echo "$basename" | grep -qE "(claude|openai|gemini|ollama|deepseek|grok)"; then
        provider=$(echo "$basename" | sed -E 's/.*(claude|openai|gemini|ollama|deepseek|grok).*/\1/')
        echo "$provider $meaningful" >> "$provider_list"
        printf "  %-30s %5d lines (%d actual, -%d%% diet) [PROVIDER: %s]\n" \
               "$basename" "$meaningful" "$actual" "$percent" "$provider"
    else
        printf "  %-30s %5d lines (%d actual, -%d%% diet)\n" \
               "$basename" "$meaningful" "$actual" "$percent"
        total=$((total + meaningful))
    fi
done

echo ""
echo "Provider Implementations:"
echo "-------------------------"

# Find the largest provider
largest_provider=""
largest_count=0

while read -r provider count; do
    printf "  %-20s %5d lines\n" "$provider" "$count"
    if [ "$count" -gt "$largest_count" ]; then
        largest_count=$count
        largest_provider=$provider
    fi
done < "$provider_list"

echo ""
echo "Diet Calculation:"
echo "-----------------"
printf "  Core files (non-provider):     %5d lines\n" "$total"
printf "  Largest provider (%s):    %5d lines\n" "$largest_provider" "$largest_count"

diet_total=$((total + largest_count))
printf "  TOTAL DIET COUNT:              %5d lines\n" "$diet_total"

budget=10000
remaining=$((budget - diet_total))
percent_used=$((diet_total * 100 / budget))

echo ""
echo "Budget Status:"
echo "--------------"
printf "  Budget:                        10,000 lines\n"
printf "  Used:                           %5d lines (%d%%)\n" "$diet_total" "$percent_used"

if [ $remaining -gt 0 ]; then
    printf "  Remaining:                      %5d lines\n" "$remaining"
else
    printf "  OVER BUDGET:                    %5d lines\n" "$((remaining * -1))"
fi

echo ""

# Show breakdown by type
echo "Excluded from Count:"
echo "--------------------"
all_actual=$(find "$SRC_DIR" -name "*.c" -exec wc -l {} + | tail -1 | awk '{print $1}')
excluded=$((all_actual - diet_total))
provider_count=$(wc -l < "$provider_list" | tr -d ' ')
duplicate_lines=$(( (provider_count - 1) * largest_count ))
printf "  Comments, blanks, logs, prints: ~%d lines\n" "$excluded"
printf "  Duplicate provider impls:       ~%d lines\n" "$duplicate_lines"

echo ""