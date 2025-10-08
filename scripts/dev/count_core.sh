#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Count meaningful lines in argo core
# Excludes: comments, blank lines, logs, prints, braces, provider duplicates

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
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

# Function to count meaningful lines excluding includes
count_meaningful_lines_no_includes() {
    local file=$1

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
        grep -v '^ *#include' | \
        wc -l | tr -d ' '
}

# Temporary files for provider tracking
provider_list=$(mktemp)
category_data=$(mktemp)
trap "rm -f $provider_list $category_data" EXIT

# Count all src/*.c files
total=0
total_no_includes=0
include_count=0
max_file_size=0
max_file_name=""
files_over_300=0

echo "Core Implementation Files:"
echo "--------------------------"

for file in "$SRC_DIR"/*.c; do
    [ -f "$file" ] || continue

    basename=$(basename "$file")

    # Skip third-party files and utility files
    if echo "$basename" | grep -qE "(jsmn|cJSON|_utils\.c$)"; then
        continue
    fi

    meaningful=$(count_meaningful_lines "$file")
    meaningful_no_inc=$(count_meaningful_lines_no_includes "$file")
    actual=$(wc -l < "$file" | tr -d ' ')
    savings=$((actual - meaningful))
    percent=$((savings * 100 / actual))
    file_includes=$((meaningful - meaningful_no_inc))

    # Track complexity metrics
    if [ "$meaningful" -gt "$max_file_size" ]; then
        max_file_size=$meaningful
        max_file_name=$basename
    fi
    if [ "$meaningful" -gt 300 ]; then
        files_over_300=$((files_over_300 + 1))
    fi

    # Categorize file
    category="other"
    if echo "$basename" | grep -qE "registry"; then
        category="registry"
    elif echo "$basename" | grep -qE "http|api"; then
        category="infrastructure"
    elif echo "$basename" | grep -qE "lifecycle|orchestrator"; then
        category="orchestration"
    elif echo "$basename" | grep -qE "memory|session"; then
        category="memory"
    elif echo "$basename" | grep -qE "workflow"; then
        category="workflow"
    elif echo "$basename" | grep -qE "error|string"; then
        category="error_utils"
    elif echo "$basename" | grep -qE "socket|json|merge"; then
        category="infrastructure"
    fi

    # Check if it's a provider implementation
    if echo "$basename" | grep -qE "(claude|openai|gemini|ollama|deepseek|grok)"; then
        provider=$(echo "$basename" | sed -E 's/.*(claude|openai|gemini|ollama|deepseek|grok).*/\1/')
        echo "$provider $meaningful $meaningful_no_inc" >> "$provider_list"
        printf "  %-30s %5d lines (%d actual, -%d%% diet) [PROVIDER: %s]\n" \
               "$basename" "$meaningful" "$actual" "$percent" "$provider"
    else
        printf "  %-30s %5d lines (%d actual, -%d%% diet)\n" \
               "$basename" "$meaningful" "$actual" "$percent"
        total=$((total + meaningful))
        total_no_includes=$((total_no_includes + meaningful_no_inc))
        include_count=$((include_count + file_includes))
        echo "$category $meaningful $meaningful_no_inc" >> "$category_data"
    fi
done

echo ""
echo "Provider Implementations:"
echo "-------------------------"

# Find the largest provider
largest_provider=""
largest_count=0
largest_count_no_inc=0

while read -r provider count count_no_inc; do
    printf "  %-20s %5d lines\n" "$provider" "$count"
    if [ "$count" -gt "$largest_count" ]; then
        largest_count=$count
        largest_count_no_inc=$count_no_inc
        largest_provider=$provider
    fi
done < "$provider_list"

echo ""
echo "Category Breakdown:"
echo "-------------------"

# Aggregate by category (compatible with older bash)
for cat_name in registry infrastructure orchestration memory workflow error_utils other; do
    cat_total=$(awk -v cat="$cat_name" '$1 == cat {sum += $2} END {print sum+0}' "$category_data")
    if [ "$cat_total" -gt 0 ]; then
        display_name=$(echo "$cat_name" | sed 's/_/ /g' | awk '{for(i=1;i<=NF;i++) $i=toupper(substr($i,1,1)) tolower(substr($i,2));}1')
        printf "  %-30s %5d lines\n" "$display_name:" "$cat_total"
    fi
done
printf "  %-30s %5d lines\n" "Providers (largest only):" "$largest_count"

echo ""
echo "Diet Calculation:"
echo "-----------------"
printf "  Core files (non-provider):     %5d lines\n" "$total"
printf "  Largest provider (%s):    %5d lines\n" "$largest_provider" "$largest_count"

diet_total=$((total + largest_count))
diet_total_no_inc=$((total_no_includes + largest_count_no_inc))
provider_includes=$((largest_count - largest_count_no_inc))

printf "  PRIMARY DIET COUNT:            %5d lines\n" "$diet_total"
echo ""
printf "  Alternate (no #includes):      %5d lines\n" "$diet_total_no_inc"
printf "  Include directives:              %3d lines\n" "$((include_count + provider_includes))"

budget=10000
remaining=$((budget - diet_total))
percent_used=$((diet_total * 100 / budget))
percent_used_no_inc=$((diet_total_no_inc * 100 / budget))

echo ""
echo "Budget Status:"
echo "--------------"
printf "  Budget:                        10,000 lines\n"
printf "  Used (primary):                 %5d lines (%d%%)\n" "$diet_total" "$percent_used"
printf "  Used (no includes):             %5d lines (%d%%)\n" "$diet_total_no_inc" "$percent_used_no_inc"

if [ $remaining -gt 0 ]; then
    printf "  Remaining:                      %5d lines\n" "$remaining"
else
    printf "  OVER BUDGET:                    %5d lines\n" "$((remaining * -1))"
fi

echo ""

# Show breakdown by type
echo "Excluded from Diet:"
echo "-------------------"
all_actual=$(find "$SRC_DIR" -name "*.c" ! -name "*jsmn*" ! -name "*cJSON*" -exec wc -l {} + 2>/dev/null | tail -1 | awk '{print $1}')
excluded=$((all_actual - diet_total))
provider_count=$(wc -l < "$provider_list" | tr -d ' ')
duplicate_lines=$(( (provider_count - 1) * largest_count ))
printf "  Comments, blanks, logs, prints: ~%d lines\n" "$excluded"
printf "  Duplicate provider impls:       ~%d lines\n" "$duplicate_lines"

# Utility files
utils_total=0
for util_file in "$SRC_DIR"/*_utils.c; do
    if [ -f "$util_file" ]; then
        lines=$(wc -l < "$util_file" | tr -d ' ')
        utils_total=$((utils_total + lines))
    fi
done
if [ "$utils_total" -gt 0 ]; then
    utils_count=$(find "$SRC_DIR" -name "*_utils.c" | wc -l | tr -d ' ')
    printf "  Utility files (*_utils.c):       %3d lines (%d files)\n" "$utils_total" "$utils_count"
fi

echo ""
echo "Complexity Indicators:"
echo "----------------------"
# Count total files
file_count=$(find "$SRC_DIR" -name "*.c" ! -name "*jsmn*" ! -name "*cJSON*" | wc -l | tr -d ' ')
avg_size=$((diet_total / (file_count - provider_count + 1)))
printf "  Total files (incl providers):    %3d files\n" "$file_count"
printf "  Average file size (diet):        %3d lines\n" "$avg_size"
printf "  Largest file:                    %3d lines (%s)\n" "$max_file_size" "$max_file_name"
printf "  Files over 300 lines:            %3d files\n" "$files_over_300"

echo ""
echo "Quality Metrics:"
echo "----------------"
# Test files
if [ -d "$PROJECT_ROOT/tests" ]; then
    test_lines=$(find "$PROJECT_ROOT/tests" -name "*.c" -exec wc -l {} + 2>/dev/null | tail -1 | awk '{print $1}')
    if [ "$test_lines" -gt 0 ]; then
        test_ratio=$(awk "BEGIN {printf \"%.2f\", $test_lines / $diet_total}")
        printf "  Test code:                      %5d lines\n" "$test_lines"
        printf "  Test:Core ratio:                 %s:1\n" "$test_ratio"
    fi
fi

# Header files
if [ -d "$PROJECT_ROOT/include" ]; then
    header_lines=$(find "$PROJECT_ROOT/include" -name "*.h" -exec wc -l {} + 2>/dev/null | tail -1 | awk '{print $1}')
    header_count=$(find "$PROJECT_ROOT/include" -name "*.h" | wc -l | tr -d ' ')
    if [ "$header_lines" -gt 0 ]; then
        printf "  Header API surface:             %5d lines (%d files)\n" "$header_lines" "$header_count"
    fi
fi

echo ""