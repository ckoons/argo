#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# test_framework.sh - Core testing framework for Argo workflows
#
# Provides:
# - Test execution and reporting
# - Assert functions
# - Test isolation (temp directories)
# - Setup/teardown hooks
# - Summary statistics

set -e

# Test state
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
CURRENT_TEST=""
FAILED_TESTS=()

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test isolation
TEST_TMP_DIR=""
TEST_SESSION_DIR=""

#
# Core Test Functions
#

# Start a test
test_start() {
    local test_name="$1"
    CURRENT_TEST="$test_name"
    TESTS_RUN=$((TESTS_RUN + 1))
    echo -n "  Testing: $test_name ... "
}

# Mark test as passed
test_pass() {
    TESTS_PASSED=$((TESTS_PASSED + 1))
    echo -e "${GREEN}PASS${NC}"
}

# Mark test as failed
test_fail() {
    local reason="$1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
    FAILED_TESTS+=("$CURRENT_TEST: $reason")
    echo -e "${RED}FAIL${NC}"
    echo -e "    ${RED}Reason: $reason${NC}"
}

#
# Assert Functions
#

# Assert two values are equal
assert_equals() {
    local expected="$1"
    local actual="$2"
    local message="${3:-Values not equal}"

    if [[ "$expected" != "$actual" ]]; then
        test_fail "$message (expected: '$expected', got: '$actual')"
        return 1
    fi
    return 0
}

# Assert value is non-empty
assert_not_empty() {
    local value="$1"
    local message="${2:-Value is empty}"

    if [[ -z "$value" ]]; then
        test_fail "$message"
        return 1
    fi
    return 0
}

# Assert value is empty
assert_empty() {
    local value="$1"
    local message="${2:-Value is not empty}"

    if [[ -n "$value" ]]; then
        test_fail "$message (got: '$value')"
        return 1
    fi
    return 0
}

# Assert file exists
assert_file_exists() {
    local file="$1"
    local message="${2:-File does not exist: $file}"

    if [[ ! -f "$file" ]]; then
        test_fail "$message"
        return 1
    fi
    return 0
}

# Assert directory exists
assert_dir_exists() {
    local dir="$1"
    local message="${2:-Directory does not exist: $dir}"

    if [[ ! -d "$dir" ]]; then
        test_fail "$message"
        return 1
    fi
    return 0
}

# Assert file contains string
assert_file_contains() {
    local file="$1"
    local string="$2"
    local message="${3:-File does not contain string}"

    if ! grep -q "$string" "$file" 2>/dev/null; then
        test_fail "$message (file: $file, string: '$string')"
        return 1
    fi
    return 0
}

# Assert command succeeds (exit code 0)
assert_success() {
    local exit_code="$1"
    local message="${2:-Command failed}"

    if [[ $exit_code -ne 0 ]]; then
        test_fail "$message (exit code: $exit_code)"
        return 1
    fi
    return 0
}

# Assert command fails (non-zero exit code)
assert_failure() {
    local exit_code="$1"
    local message="${2:-Command succeeded unexpectedly}"

    if [[ $exit_code -eq 0 ]]; then
        test_fail "$message"
        return 1
    fi
    return 0
}

# Assert JSON field equals value
assert_json_equals() {
    local json_file="$1"
    local field_path="$2"
    local expected="$3"
    local message="${4:-JSON field mismatch}"

    if [[ ! -f "$json_file" ]]; then
        test_fail "JSON file not found: $json_file"
        return 1
    fi

    local actual=$(jq -r "$field_path" "$json_file" 2>/dev/null)
    if [[ $? -ne 0 ]]; then
        test_fail "Failed to parse JSON or extract field: $field_path"
        return 1
    fi

    if [[ "$actual" != "$expected" ]]; then
        test_fail "$message (expected: '$expected', got: '$actual')"
        return 1
    fi
    return 0
}

# Assert value matches regex
assert_matches() {
    local value="$1"
    local pattern="$2"
    local message="${3:-Value does not match pattern}"

    if [[ ! "$value" =~ $pattern ]]; then
        test_fail "$message (value: '$value', pattern: '$pattern')"
        return 1
    fi
    return 0
}

#
# Test Isolation
#

# Setup test environment (called before each test)
test_setup() {
    # Create temporary directory for this test
    TEST_TMP_DIR=$(mktemp -d /tmp/argo_test.XXXXXX)

    # Create mock session directory
    TEST_SESSION_DIR="$TEST_TMP_DIR/sessions"
    mkdir -p "$TEST_SESSION_DIR"

    # Export for tests
    export TEST_TMP_DIR
    export TEST_SESSION_DIR
    export ARGO_SESSIONS_DIR="$TEST_SESSION_DIR"
}

# Teardown test environment (called after each test)
test_teardown() {
    # Clean up temporary directory
    if [[ -n "$TEST_TMP_DIR" ]] && [[ -d "$TEST_TMP_DIR" ]]; then
        rm -rf "$TEST_TMP_DIR"
    fi

    # Clear exports
    unset TEST_TMP_DIR
    unset TEST_SESSION_DIR
    unset ARGO_SESSIONS_DIR
}

#
# Test Suite Management
#

# Run a test function with setup/teardown
run_test() {
    local test_function="$1"
    local test_name="${2:-$test_function}"

    test_start "$test_name"

    # Setup
    test_setup

    # Run test (capture result but don't exit on failure)
    set +e
    $test_function
    local result=$?
    set -e

    # Teardown
    test_teardown

    # Record result
    if [[ $result -eq 0 ]]; then
        test_pass
    fi
}

# Print test summary
test_summary() {
    echo ""
    echo "========================================"
    echo "Test Summary"
    echo "========================================"
    echo "Tests run:    $TESTS_RUN"
    echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
    echo ""

    if [[ $TESTS_FAILED -gt 0 ]]; then
        echo -e "${RED}Failed Tests:${NC}"
        for failed in "${FAILED_TESTS[@]}"; do
            echo -e "  ${RED}✗${NC} $failed"
        done
        echo ""
        return 1
    else
        echo -e "${GREEN}All tests passed!${NC}"
        return 0
    fi
}

# Run test suite from file
run_test_suite() {
    local suite_name="$1"
    local suite_file="$2"

    echo ""
    echo "========================================"
    echo "Running Test Suite: $suite_name"
    echo "========================================"

    # Source the test suite file
    if [[ ! -f "$suite_file" ]]; then
        echo -e "${RED}ERROR: Test suite file not found: $suite_file${NC}"
        return 1
    fi

    source "$suite_file"

    # Print summary
    test_summary
}

#
# Mock Helpers
#

# Create mock function
mock_function() {
    local function_name="$1"
    local return_value="${2:-0}"
    local output="${3:-}"

    eval "$function_name() {
        if [[ -n \"$output\" ]]; then
            echo \"$output\"
        fi
        return $return_value
    }"
}

# Create mock command
mock_command() {
    local command_name="$1"
    local script_content="$2"

    local mock_script="$TEST_TMP_DIR/mock_$command_name"
    cat > "$mock_script" <<EOF
#!/bin/bash
$script_content
EOF
    chmod +x "$mock_script"

    # Add to PATH
    export PATH="$TEST_TMP_DIR:$PATH"
}

# Create mock git repository
mock_git_repo() {
    local repo_dir="${1:-$TEST_TMP_DIR/repo}"

    mkdir -p "$repo_dir"
    cd "$repo_dir"

    git init -q
    git config user.email "test@example.com"
    git config user.name "Test User"

    # Create initial commit
    echo "# Test Repository" > README.md
    git add README.md
    git commit -q -m "Initial commit"

    echo "$repo_dir"
}

#
# Fixture Helpers
#

# Load test fixture
load_fixture() {
    local fixture_name="$1"
    local fixtures_dir="${2:-$(dirname ${BASH_SOURCE[0]})/fixtures}"

    local fixture_file="$fixtures_dir/$fixture_name"
    if [[ ! -f "$fixture_file" ]]; then
        echo "ERROR: Fixture not found: $fixture_file" >&2
        return 1
    fi

    cat "$fixture_file"
}

# Copy fixture to location
copy_fixture() {
    local fixture_name="$1"
    local destination="$2"
    local fixtures_dir="${3:-$(dirname ${BASH_SOURCE[0]})/fixtures}"

    local fixture_file="$fixtures_dir/$fixture_name"
    if [[ ! -f "$fixture_file" ]]; then
        echo "ERROR: Fixture not found: $fixture_file" >&2
        return 1
    fi

    cp "$fixture_file" "$destination"
}

# Export functions for use in test scripts
export -f test_start
export -f test_pass
export -f test_fail
export -f assert_equals
export -f assert_not_empty
export -f assert_empty
export -f assert_file_exists
export -f assert_dir_exists
export -f assert_file_contains
export -f assert_success
export -f assert_failure
export -f assert_json_equals
export -f assert_matches
export -f test_setup
export -f test_teardown
export -f run_test
export -f test_summary
export -f mock_function
export -f mock_command
export -f mock_git_repo
export -f load_fixture
export -f copy_fixture
