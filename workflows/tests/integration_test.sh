#!/bin/bash
# © 2025 Casey Koons All rights reserved
#
# integration_test.sh - Complete end-to-end integration test
#
# Tests the full workflow from project creation to completion
# Handles environment correctly for background processes

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Script directory
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly WORKFLOWS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Test configuration
readonly TEST_TIMEOUT=120  # 2 minutes max
readonly CHECK_INTERVAL=2  # Check every 2 seconds

#
# log_step - Print colored step message
#
log_step() {
    echo -e "${GREEN}✓${NC} $1"
}

#
# log_info - Print info message
#
log_info() {
    echo -e "  $1"
}

#
# log_error - Print error message
#
log_error() {
    echo -e "${RED}✗${NC} $1"
}

#
# log_warning - Print warning message
#
log_warning() {
    echo -e "${YELLOW}!${NC} $1"
}

#
# cleanup_test - Clean up test directory and mock CI tool
#
cleanup_test() {
    local test_dir="$1"
    local mock_ci="$2"

    if [[ -n "$test_dir" ]] && [[ -d "$test_dir" ]]; then
        # Kill orchestrator if still running
        if [[ -f "$test_dir/.argo-project/orchestrator.pid" ]]; then
            local pid=$(cat "$test_dir/.argo-project/orchestrator.pid" 2>/dev/null)
            if [[ -n "$pid" ]] && ps -p "$pid" >/dev/null 2>&1; then
                kill "$pid" 2>/dev/null || true
                sleep 1
            fi
        fi

        # Remove test directory
        rm -rf "$test_dir"
    fi

    # Remove mock CI tool
    if [[ -n "$mock_ci" ]] && [[ -f "$mock_ci" ]]; then
        rm -f "$mock_ci"
    fi
}

#
# Main test function
#
main() {
    local test_dir=""
    local test_failed=0

    echo "========================================"
    echo "Argo Workflows Integration Test"
    echo "========================================"
    echo ""

    # Step 1: Run automated unit tests
    log_step "Step 1: Running automated unit tests..."
    cd "$SCRIPT_DIR"
    if ./run_phase2_tests.sh > /tmp/argo_unit_tests.log 2>&1; then
        log_info "All 43 unit tests passed"
    else
        log_error "Unit tests failed"
        cat /tmp/argo_unit_tests.log
        return 1
    fi
    echo ""

    # Step 2: Create test project
    log_step "Step 2: Creating test project..."
    test_dir="/tmp/argo-integration-test-$$"
    mkdir -p "$test_dir"
    cd "$test_dir"

    # Initialize git
    git init >/dev/null 2>&1
    git config user.name "Integration Test"
    git config user.email "test@integration.test"
    echo "# Integration Test Project" > README.md
    git add README.md
    git commit -m "Initial commit" >/dev/null 2>&1

    log_info "Test directory: $test_dir"
    echo ""

    # Step 3: Initialize Argo project
    log_step "Step 3: Initializing Argo project..."
    "$WORKFLOWS_DIR/templates/project_setup.sh" "integration-test" . >/dev/null 2>&1
    log_info "Argo project initialized"
    echo ""

    # Step 4: Create mock CI tool and configure it
    log_step "Step 4: Creating mock CI tool..."
    local mock_ci="/tmp/mock_ci_integration_$$.sh"

    cat > "$mock_ci" << 'EOF'
#!/bin/bash
# Mock CI tool - returns instantly with simple design
cat << 'JSON'
{
  "summary": "Simple test file creator",
  "components": [
    {
      "id": "test-file",
      "description": "Creates a simple test file with sample content",
      "technologies": ["bash"]
    }
  ]
}
JSON
exit 0
EOF
    chmod +x "$mock_ci"

    # Configure CI tool in config.json (more reliable than environment variables)
    jq --arg ci_tool "$mock_ci" '.ci.tool = $ci_tool' \
       .argo-project/config.json > .argo-project/config.json.tmp
    mv .argo-project/config.json.tmp .argo-project/config.json

    log_info "Mock CI: $mock_ci"
    log_info "Configured in .argo-project/config.json"
    echo ""

    # Step 5: Start workflow
    log_step "Step 5: Starting workflow..."
    "$WORKFLOWS_DIR/start_workflow.sh" "Create a simple test.txt file with hello world content" >/dev/null 2>&1

    if [[ -f .argo-project/orchestrator.pid ]]; then
        local orch_pid=$(cat .argo-project/orchestrator.pid)
        log_info "Orchestrator started (PID: $orch_pid)"
    else
        log_error "Failed to start orchestrator"
        cleanup_test "$test_dir" "$mock_ci"
        return 1
    fi
    echo ""

    # Step 6: Monitor workflow progress
    log_step "Step 6: Monitoring workflow progress..."

    local elapsed=0
    local last_phase=""
    local design_approved=0

    while [[ $elapsed -lt $TEST_TIMEOUT ]]; do
        # Read current state
        local phase=$(jq -r '.phase' .argo-project/state.json 2>/dev/null)
        local action_owner=$(jq -r '.action_owner' .argo-project/state.json 2>/dev/null)
        local action_needed=$(jq -r '.action_needed' .argo-project/state.json 2>/dev/null)

        # Report phase changes
        if [[ "$phase" != "$last_phase" ]]; then
            log_info "Phase: $phase (owner: $action_owner, action: $action_needed)"
            last_phase="$phase"
        fi

        # Auto-approve design when needed
        if [[ "$phase" == "design" ]] && [[ "$action_owner" == "ci" ]] && [[ $design_approved -eq 0 ]]; then
            log_info "Auto-approving design..."
            jq '.action_owner = "code" | .action_needed = "design_build"' \
               .argo-project/state.json > .argo-project/state.json.tmp
            mv .argo-project/state.json.tmp .argo-project/state.json
            design_approved=1
        fi

        # Check for completion
        if [[ "$phase" == "storage" ]]; then
            log_info "Workflow completed!"
            break
        fi

        # Check if orchestrator crashed
        if [[ -f .argo-project/orchestrator.pid ]]; then
            local orch_pid=$(cat .argo-project/orchestrator.pid)
            if ! ps -p "$orch_pid" >/dev/null 2>&1; then
                # Orchestrator exited - check if it's because workflow completed
                if [[ "$phase" == "storage" ]]; then
                    log_info "Orchestrator exited (workflow complete)"
                    break
                else
                    log_error "Orchestrator crashed unexpectedly"
                    log_info "Last phase: $phase"
                    log_info "Checking logs..."
                    if [[ -f .argo-project/logs/workflow.log ]]; then
                        tail -10 .argo-project/logs/workflow.log
                    fi
                    test_failed=1
                    break
                fi
            fi
        fi

        sleep $CHECK_INTERVAL
        elapsed=$((elapsed + CHECK_INTERVAL))
    done

    if [[ $elapsed -ge $TEST_TIMEOUT ]]; then
        log_error "Workflow timeout after ${TEST_TIMEOUT}s"
        log_info "Final state: phase=$phase, owner=$action_owner"
        test_failed=1
    fi
    echo ""

    # Step 7: Verify results
    log_step "Step 7: Verifying results..."

    local verification_failed=0

    # Check final phase
    local final_phase=$(jq -r '.phase' .argo-project/state.json)
    if [[ "$final_phase" == "storage" ]]; then
        log_info "✓ Final phase is 'storage'"
    else
        log_error "✗ Final phase is '$final_phase' (expected 'storage')"
        verification_failed=1
    fi

    # Check state file is valid JSON
    if jq . .argo-project/state.json >/dev/null 2>&1; then
        log_info "✓ State file is valid JSON"
    else
        log_error "✗ State file is corrupted"
        verification_failed=1
    fi

    # Check orchestrator exited cleanly
    if [[ ! -f .argo-project/orchestrator.pid ]]; then
        log_info "✓ Orchestrator PID file cleaned up"
    else
        log_warning "⚠ Orchestrator PID file still exists"
    fi

    # Check git worktrees cleaned up
    local worktree_count=$(git worktree list | wc -l | tr -d ' ')
    if [[ $worktree_count -eq 1 ]]; then
        log_info "✓ Git worktrees cleaned up"
    else
        log_error "✗ Found $worktree_count worktrees (expected 1)"
        verification_failed=1
    fi

    # Check builder branches cleaned up
    local builder_branches=$(git branch -a | grep -c "argo-builder" || true)
    if [[ $builder_branches -eq 0 ]]; then
        log_info "✓ Builder branches cleaned up"
    else
        log_error "✗ Found $builder_branches builder branches (expected 0)"
        verification_failed=1
    fi

    # Check commits were made
    local commit_count=$(git log --oneline | wc -l | tr -d ' ')
    if [[ $commit_count -gt 1 ]]; then
        log_info "✓ Commits were made ($commit_count total)"
    else
        log_warning "⚠ Only $commit_count commit found"
    fi

    if [[ $verification_failed -eq 1 ]]; then
        test_failed=1
    fi
    echo ""

    # Step 8: Show workflow summary
    if [[ $test_failed -eq 0 ]]; then
        log_step "Step 8: Test Summary"
        log_info "Project: $test_dir"
        log_info "Workflow log: .argo-project/logs/workflow.log"
        log_info "Git commits:"
        git log --oneline -5 | sed 's/^/    /'
        echo ""
    fi

    # Step 9: Cleanup
    log_step "Step 9: Cleaning up..."

    if [[ -n "${KEEP_TEST_DIR:-}" ]]; then
        log_info "Test directory preserved: $test_dir"
        log_info "Mock CI preserved: $mock_ci"
        log_info "To inspect: cd $test_dir"
        log_info "To cleanup: rm -rf $test_dir $mock_ci"
    else
        cleanup_test "$test_dir" "$mock_ci"
        log_info "Test directory and mock CI removed"
    fi
    echo ""

    # Final result
    echo "========================================"
    if [[ $test_failed -eq 0 ]]; then
        echo -e "${GREEN}✓ INTEGRATION TEST PASSED${NC}"
        echo "========================================"
        return 0
    else
        echo -e "${RED}✗ INTEGRATION TEST FAILED${NC}"
        echo "========================================"
        echo ""
        echo "Troubleshooting:"
        echo "  1. Check logs: tail $test_dir/.argo-project/logs/workflow.log"
        echo "  2. Check state: jq . $test_dir/.argo-project/state.json"
        echo "  3. Check git: cd $test_dir && git log --oneline"
        echo ""
        return 1
    fi
}

# Run main function
main "$@"
exit $?
