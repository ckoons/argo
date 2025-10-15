# Workflow Template Directory Structure

© 2025 Casey Koons. All rights reserved.

## Concept: Directory-Based Templates

Instead of single `.sh` files, each workflow template is a **directory** containing:
- Main workflow script
- Separate test files
- Documentation files
- Configuration/metadata
- Supporting files (data, configs, etc.)

## Benefits Analysis

### Single File (Current Design)
```
.argo/workflows/templates/
  build_and_test.sh          # Everything in one file (500+ lines)
  deploy_app.sh              # Everything in one file (400+ lines)
  process_data.sh            # Everything in one file (600+ lines)
```

**Problems**:
- ❌ Large files hard to navigate (500+ lines)
- ❌ Can't separate concerns cleanly
- ❌ Difficult to version control specific parts
- ❌ No place for supporting files (test data, configs)
- ❌ Mixing metadata, docs, tests, and code
- ❌ Hard to edit just documentation or just tests

### Directory-Based (Proposed)
```
.argo/workflows/templates/
  build_and_test/
    workflow.sh              # Main workflow script (~100 lines)
    metadata.yaml            # Structured metadata
    README.md                # User-facing documentation
    tests/
      test_basic.sh          # Individual test case
      test_failures.sh       # Another test case
      test_edge_cases.sh     # Edge case tests
      test_data/             # Test fixtures
        sample_project/
        failing_project/
    config/
      defaults.env           # Default parameters
      production.env         # Production config
    lib/                     # Shared functions (optional)
      common.sh              # Helper functions
```

**Benefits**:
- ✅ Separation of concerns (workflow vs tests vs docs)
- ✅ Smaller, focused files (easier to edit)
- ✅ Natural place for supporting files
- ✅ Better version control (git blame, history)
- ✅ Can edit docs without touching code
- ✅ Tests are independent files (easier to add/modify)
- ✅ Configuration files separate from logic
- ✅ Room for growth (examples, assets, etc.)

## Detailed Directory Structure

### Mandatory Files

```
workflow_name/
  workflow.sh              # Main executable (required)
  metadata.yaml            # Template metadata (required)
  README.md                # User documentation (required)
```

### Optional Directories

```
workflow_name/
  tests/                   # Test cases (recommended)
    test_*.sh              # Individual test files
    test_data/             # Test fixtures
  config/                  # Configuration files (optional)
    defaults.env           # Default parameters
    *.env                  # Environment-specific configs
  lib/                     # Shared functions (optional)
    *.sh                   # Reusable bash functions
  docs/                    # Additional documentation (optional)
    examples.md            # Usage examples
    troubleshooting.md     # Common issues
  assets/                  # Supporting files (optional)
    templates/             # File templates
    data/                  # Reference data
```

## File Format Specifications

### `workflow.sh` - Main Workflow Script

**Purpose**: The executable workflow logic

**Format**: Clean bash script without embedded metadata/docs
```bash
#!/bin/bash
# Argo Workflow: Build and Test
# See README.md for documentation

set -e
set -u

# Load configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config/defaults.env"

# Load helper functions (if any)
if [ -f "${SCRIPT_DIR}/lib/common.sh" ]; then
    source "${SCRIPT_DIR}/lib/common.sh"
fi

# Parse parameters (override defaults from environment)
TEST_PATH=${TEST_PATH:-tests/}
PYTHON_VERSION=${PYTHON_VERSION:-3.11}

echo "=== Build and Test Workflow ==="
echo "Test Path: $TEST_PATH"
echo "Python Version: $PYTHON_VERSION"
echo ""

# Step 1: Environment Setup
echo "Step 1: Setting up environment..."
python${PYTHON_VERSION} -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
pip install pytest pytest-cov ruff black

# Step 2: Lint Check
echo ""
echo "Step 2: Running linters..."
ruff check .
black --check .

# Step 3: Run Tests
echo ""
echo "Step 3: Running tests..."
pytest "$TEST_PATH" --cov=. --cov-report=term-missing

# Step 4: Build
echo ""
echo "Step 4: Building distribution..."
python -m build

# Step 5: Validate
echo ""
echo "Step 5: Validating artifacts..."
if [ ! -d "dist" ] || [ -z "$(ls -A dist)" ]; then
    echo "ERROR: Build artifacts not found in dist/"
    exit 1
fi

echo ""
echo "=== Workflow Complete ==="
echo "Build artifacts: $(ls dist/)"
exit 0
```

**Size**: ~100 lines (clean, focused code)

### `metadata.yaml` - Structured Metadata

**Purpose**: Machine-readable workflow information

**Format**: YAML for easy parsing
```yaml
# Argo Workflow Template Metadata

workflow:
  name: build_and_test
  version: 1.0.0
  description: Build Python application and run test suite
  author: Casey Koons
  created: 2025-01-15
  updated: 2025-01-15
  tags:
    - python
    - testing
    - ci
    - build

parameters:
  TEST_PATH:
    description: Path to test directory
    type: string
    default: tests/
    required: false

  PYTHON_VERSION:
    description: Python version to use
    type: string
    default: "3.11"
    required: false
    allowed:
      - "3.9"
      - "3.10"
      - "3.11"
      - "3.12"

  COVERAGE_THRESHOLD:
    description: Minimum test coverage percentage
    type: integer
    default: 80
    required: false

success_criteria:
  - All linters pass (exit code 0)
  - All tests pass (exit code 0)
  - Test coverage >= {COVERAGE_THRESHOLD}%
  - Build artifacts exist in dist/

dependencies:
  system:
    - python3.11
  python:
    - pytest>=7.0
    - pytest-cov>=4.0
    - ruff>=0.1.0
    - black>=23.0
    - build>=1.0

timeout: 600  # seconds
retry_on_failure: false
```

**Size**: ~50 lines (structured, easy to parse)

### `README.md` - User Documentation

**Purpose**: Human-readable documentation

**Format**: Markdown
```markdown
# Build and Test Workflow

Build a Python application and run its test suite with coverage reporting.

## Purpose

This workflow automates the Python development cycle:
- Code quality checks (linting)
- Test execution with coverage
- Build artifact creation
- Validation of outputs

## Quick Start

```bash
# Run with defaults
arc workflow start build_and_test my_build

# Run with custom test path
arc workflow start build_and_test my_build TEST_PATH=tests/integration/

# Run with different Python version
arc workflow start build_and_test my_build PYTHON_VERSION=3.12
```

## Parameters

| Parameter | Description | Default | Required |
|-----------|-------------|---------|----------|
| TEST_PATH | Path to tests | `tests/` | No |
| PYTHON_VERSION | Python version | `3.11` | No |
| COVERAGE_THRESHOLD | Min coverage % | `80` | No |

## Workflow Steps

1. **Environment Setup** - Creates venv, installs dependencies
2. **Lint Check** - Runs ruff and black for code quality
3. **Run Tests** - Executes pytest with coverage reporting
4. **Build** - Creates distribution packages
5. **Validate** - Verifies artifacts exist

## Success Criteria

Workflow succeeds when:
- ✓ All linters pass (exit code 0)
- ✓ All tests pass (exit code 0)
- ✓ Coverage >= threshold
- ✓ Build artifacts exist in `dist/`

## Testing

Run workflow tests before production use:

```bash
# Run all tests
arc workflow test build_and_test --all

# Run specific test
arc workflow test build_and_test test_basic

# List available tests
arc workflow test build_and_test --list
```

## Troubleshooting

### Tests fail
- Check test logs in workflow output
- Verify dependencies in `requirements.txt`
- Run tests locally: `pytest tests/`

### Linting fails
- Run locally: `ruff check --fix .`
- Format code: `black .`

### Build fails
- Verify `pyproject.toml` or `setup.py` exists
- Check Python version compatibility

## Examples

### Standard CI Build
```bash
arc workflow start build_and_test ci_build
```

### Integration Tests Only
```bash
arc workflow start build_and_test integration TEST_PATH=tests/integration/
```

### Different Python Version
```bash
arc workflow start build_and_test py312 PYTHON_VERSION=3.12
```

## Files

- `workflow.sh` - Main workflow script
- `metadata.yaml` - Workflow metadata
- `README.md` - This documentation
- `tests/` - Test cases for this workflow
- `config/` - Configuration files

## See Also

- [Python Testing Guide](../docs/testing.md)
- [CI/CD Best Practices](../docs/cicd.md)
```

**Size**: ~100 lines (comprehensive, readable)

### `tests/test_basic.sh` - Individual Test Case

**Purpose**: Self-contained test for one scenario

**Format**: Bash script with structured sections
```bash
#!/bin/bash
# Test: Basic successful workflow execution

# Test metadata (parsed by test runner)
# @test-name: test_basic_success
# @test-description: Verify workflow succeeds with valid Python project
# @test-timeout: 120

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKFLOW_DIR="$(dirname "$SCRIPT_DIR")"
WORKFLOW_SCRIPT="${WORKFLOW_DIR}/workflow.sh"

# Test setup
setup() {
    echo "=== Test Setup ==="
    TEST_PROJECT="/tmp/argo_test_$$"
    mkdir -p "$TEST_PROJECT"
    cd "$TEST_PROJECT"

    # Create minimal Python project
    cat > pyproject.toml << 'EOF'
[build-system]
requires = ["setuptools", "wheel"]
build-backend = "setuptools.build_meta"

[project]
name = "test-app"
version = "0.1.0"
EOF

    cat > setup.py << 'EOF'
from setuptools import setup
setup()
EOF

    # Create test
    mkdir -p tests
    cat > tests/test_example.py << 'EOF'
def test_basic():
    assert 1 + 1 == 2
EOF

    # Create requirements
    cat > requirements.txt << 'EOF'
# No runtime dependencies
EOF

    echo "✓ Test project created: $TEST_PROJECT"
}

# Test execution
run_test() {
    echo ""
    echo "=== Running Workflow ==="

    # Run workflow in test project directory
    cd "$TEST_PROJECT"
    bash "$WORKFLOW_SCRIPT"

    local exit_code=$?
    echo "Workflow exit code: $exit_code"
    return $exit_code
}

# Test validation
validate() {
    echo ""
    echo "=== Validating Results ==="

    # Check exit code
    if [ $1 -ne 0 ]; then
        echo "✗ Workflow failed (expected success)"
        return 1
    fi

    # Check build artifacts exist
    if [ ! -d "$TEST_PROJECT/dist" ]; then
        echo "✗ Build artifacts not found"
        return 1
    fi

    if [ -z "$(ls -A "$TEST_PROJECT/dist")" ]; then
        echo "✗ Build artifacts directory is empty"
        return 1
    fi

    echo "✓ All validations passed"
    return 0
}

# Test cleanup
cleanup() {
    echo ""
    echo "=== Cleanup ==="
    if [ -d "$TEST_PROJECT" ]; then
        rm -rf "$TEST_PROJECT"
        echo "✓ Test project removed"
    fi
}

# Run test lifecycle
main() {
    local result=0

    setup || { echo "Setup failed"; return 1; }

    if run_test; then
        validate 0 || result=1
    else
        validate 1 || result=1
    fi

    cleanup

    if [ $result -eq 0 ]; then
        echo ""
        echo "✓ TEST PASSED: test_basic_success"
    else
        echo ""
        echo "✗ TEST FAILED: test_basic_success"
    fi

    return $result
}

main
```

**Size**: ~100 lines per test (comprehensive, isolated)

### `config/defaults.env` - Default Parameters

**Purpose**: Default values for workflow parameters

**Format**: Bash-compatible environment file
```bash
# Default configuration for build_and_test workflow

# Test configuration
TEST_PATH=tests/
COVERAGE_THRESHOLD=80

# Python configuration
PYTHON_VERSION=3.11

# Build configuration
BUILD_OUTPUT=dist/
BUILD_CLEAN=true

# Verbosity
VERBOSE=false
```

**Size**: ~20 lines (focused configuration)

## Command Changes

### `arc workflow start <template> <instance>`

**Before** (single file):
```bash
arc workflow start build_and_test my_build
  → Looks for: .argo/workflows/templates/build_and_test.sh
  → Executes: bash .argo/workflows/templates/build_and_test.sh
```

**After** (directory):
```bash
arc workflow start build_and_test my_build
  → Looks for: .argo/workflows/templates/build_and_test/
  → Executes: bash .argo/workflows/templates/build_and_test/workflow.sh
```

### `arc workflow test <template> [test-name]`

**Before** (single file):
```bash
arc workflow test build_and_test test_basic
  → Parses embedded @test-name from .sh file
  → Extracts setup/run/cleanup commands
  → Executes inline
```

**After** (directory):
```bash
arc workflow test build_and_test test_basic
  → Looks in: .argo/workflows/templates/build_and_test/tests/
  → Finds: test_basic.sh
  → Executes: bash test_basic.sh
```

### `arc workflow docs <template>`

**Before** (single file):
```bash
arc workflow docs build_and_test
  → Extracts DOCUMENTATION section from .sh file
  → Renders markdown
```

**After** (directory):
```bash
arc workflow docs build_and_test
  → Reads: .argo/workflows/templates/build_and_test/README.md
  → Renders markdown (or shows in pager/browser)
```

### `arc workflow list`

**Before** (single file):
```bash
arc workflow list
  → Finds all *.sh files
  → Parses @workflow-name from each
```

**After** (directory):
```bash
arc workflow list
  → Finds all directories
  → Reads metadata.yaml from each
  → Shows name, version, description
```

## Migration Path

### Phase 1: Support Both Formats

Allow both:
- `.argo/workflows/templates/workflow_name.sh` (legacy)
- `.argo/workflows/templates/workflow_name/` (new)

Detection logic:
```bash
if [ -d ".argo/workflows/templates/$WORKFLOW_NAME" ]; then
    # New directory format
    WORKFLOW_SCRIPT=".argo/workflows/templates/$WORKFLOW_NAME/workflow.sh"
elif [ -f ".argo/workflows/templates/$WORKFLOW_NAME.sh" ]; then
    # Legacy single file
    WORKFLOW_SCRIPT=".argo/workflows/templates/$WORKFLOW_NAME.sh"
else
    echo "Error: Workflow '$WORKFLOW_NAME' not found"
    exit 1
fi
```

### Phase 2: Migration Tool

```bash
arc workflow migrate <workflow-name>
  → Converts single .sh file to directory structure
  → Extracts metadata to metadata.yaml
  → Extracts documentation to README.md
  → Extracts tests to tests/*.sh
  → Preserves original as *.sh.backup
```

### Phase 3: Deprecate Single Files

After transition period:
- Warn when using single-file templates
- Eventually require directory format

## Directory Template Examples

### Simple Workflow (Minimal)
```
hello_world/
  workflow.sh              # 10 lines
  metadata.yaml            # 15 lines
  README.md                # 30 lines
```

### Medium Workflow (Typical)
```
build_and_test/
  workflow.sh              # 100 lines
  metadata.yaml            # 50 lines
  README.md                # 100 lines
  tests/
    test_basic.sh          # 100 lines
    test_failures.sh       # 100 lines
  config/
    defaults.env           # 20 lines
```

### Complex Workflow (Advanced)
```
deploy_to_production/
  workflow.sh              # 150 lines
  metadata.yaml            # 80 lines
  README.md                # 200 lines
  tests/
    test_staging.sh        # 150 lines
    test_production.sh     # 150 lines
    test_rollback.sh       # 150 lines
    test_data/
      staging_config.yaml
      prod_config.yaml
  config/
    defaults.env           # 30 lines
    staging.env            # 20 lines
    production.env         # 20 lines
  lib/
    deploy_common.sh       # 100 lines
    validation.sh          # 50 lines
  docs/
    deployment_guide.md    # 300 lines
    troubleshooting.md     # 200 lines
  assets/
    templates/
      nginx.conf.template
      systemd.service.template
```

## Size Comparison

### Single File Approach
```
build_and_test.sh: 600 lines
  - Metadata:       50 lines
  - Documentation: 150 lines
  - Tests:         200 lines
  - Implementation: 200 lines
```

**Problems**:
- One giant file
- Hard to navigate
- Mixing concerns

### Directory Approach
```
build_and_test/
  workflow.sh:      100 lines  (focused code)
  metadata.yaml:     50 lines  (structured data)
  README.md:        100 lines  (user docs)
  tests/
    test_basic.sh:  100 lines  (one test)
    test_fail.sh:   100 lines  (one test)
  config/
    defaults.env:    20 lines  (configuration)

Total: 470 lines (130 lines saved through better organization)
```

**Benefits**:
- Smaller, focused files
- Clear separation
- Easy to modify individual parts

## Recommended Structure

For new workflows created by `create_workflow`:

```
workflow_name/
  workflow.sh              # Required: Main script
  metadata.yaml            # Required: Structured metadata
  README.md                # Required: User documentation
  tests/                   # Required: At least 3 tests
    test_basic.sh          # Basic success case
    test_failure.sh        # Expected failure handling
    test_edge_case.sh      # Edge case or unusual input
  config/                  # Optional: If parameters needed
    defaults.env           # Default parameter values
```

## Implementation Notes

### Template Detection
```bash
# Check if template exists and get workflow script path
find_workflow_script() {
    local template_name=$1
    local template_dir="${HOME}/.argo/workflows/templates"

    if [ -d "${template_dir}/${template_name}" ]; then
        # Directory format (preferred)
        echo "${template_dir}/${template_name}/workflow.sh"
    elif [ -f "${template_dir}/${template_name}.sh" ]; then
        # Legacy single file
        echo "${template_dir}/${template_name}.sh"
    else
        return 1
    fi
}
```

### Test Discovery
```bash
# Find all test files for a template
find_tests() {
    local template_name=$1
    local template_dir="${HOME}/.argo/workflows/templates/${template_name}"

    if [ -d "${template_dir}/tests" ]; then
        find "${template_dir}/tests" -name 'test_*.sh' -type f
    fi
}
```

### Metadata Reading
```bash
# Read metadata (YAML parsing needed)
read_metadata() {
    local template_name=$1
    local metadata_file="${HOME}/.argo/workflows/templates/${template_name}/metadata.yaml"

    if [ -f "$metadata_file" ]; then
        # Parse YAML (using yq, jq, or python)
        cat "$metadata_file"
    fi
}
```

## Conclusion

**Recommendation**: **Adopt directory-based templates**

This provides:
- ✅ Better organization and separation of concerns
- ✅ Easier to maintain and modify
- ✅ Natural place for supporting files
- ✅ Better version control
- ✅ Room for growth
- ✅ Cleaner, more focused files

**Migration Strategy**:
1. Support both formats initially
2. Create new workflows as directories
3. Provide migration tool for existing workflows
4. Eventually deprecate single files

**Next Steps**:
1. Update `create_workflow` to generate directory structure
2. Implement directory detection in `arc workflow` commands
3. Create migration tool
4. Update documentation and examples
