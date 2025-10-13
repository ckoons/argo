#!/usr/bin/env python3
# © 2025 Casey Koons All rights reserved
# Find longest functions in C source files

import re
import sys
from pathlib import Path

def find_functions(file_path):
    """Parse C file and find function definitions with their line counts."""
    try:
        with open(file_path, 'r') as f:
            lines = f.readlines()
    except:
        return []

    functions = []
    in_function = False
    func_start = 0
    func_name = ""
    brace_count = 0

    # Pattern to match function definitions (simplified)
    func_pattern = re.compile(r'^(static\s+)?(\w+\s+\*?)(\w+)\s*\([^)]*\)\s*\{?$')

    for i, line in enumerate(lines, 1):
        stripped = line.strip()

        # Skip empty lines, comments, preprocessor directives
        if not stripped or stripped.startswith('//') or stripped.startswith('#'):
            continue

        # Look for function start
        if not in_function:
            match = func_pattern.match(stripped)
            if match and not stripped.startswith('typedef'):
                func_name = match.group(3)
                func_start = i
                in_function = True
                brace_count = stripped.count('{') - stripped.count('}')
                continue

        # Track braces inside function
        if in_function:
            brace_count += stripped.count('{') - stripped.count('}')

            # Function ends when braces balance
            if brace_count == 0:
                func_length = i - func_start + 1
                functions.append((func_name, func_start, func_length, file_path))
                in_function = False
                func_name = ""

    return functions

def main():
    if len(sys.argv) < 2:
        print("Usage: find_longest_functions.py <src_directory>")
        sys.exit(1)

    src_dir = Path(sys.argv[1])
    all_functions = []

    # Find all .c files
    for c_file in src_dir.rglob('*.c'):
        functions = find_functions(c_file)
        all_functions.extend(functions)

    # Sort by length descending
    all_functions.sort(key=lambda x: x[2], reverse=True)

    # Print top 5
    print("Top 5 longest functions:")
    for i, (name, start, length, path) in enumerate(all_functions[:5], 1):
        print(f"{i}. {name} ({path}:{start}) - {length} lines")

    # Return warning if any function > 150 lines
    long_funcs = [f for f in all_functions if f[2] > 150]
    if long_funcs:
        print(f"\nFound {len(long_funcs)} functions exceeding 150 lines")
        sys.exit(1)
    else:
        sys.exit(0)

if __name__ == '__main__':
    main()
