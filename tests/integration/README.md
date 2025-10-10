# Arc CLI Integration Tests

## Test Files

### test_arc_complete.sh
Comprehensive integration test covering all arc workflow commands:

**Tests Included:**
1. ✅ Daemon health check
2. ✅ Arc binary verification
3. ✅ `arc workflow list` (empty state)
4. ✅ `arc workflow start <template> <instance>`
5. ✅ `arc workflow list` (with active workflows)
6. ✅ `arc workflow status <workflow_name>`
7. ✅ `arc workflow abandon <workflow_name>` (with confirmation)
8. ✅ Verify workflow removal after abandon
9. ⏱️  Workflow completion and auto-removal (timing dependent)
10. ✅ Error handling for non-existent template
11. ✅ Error handling for non-existent workflow (status)
12. ✅ Error handling for non-existent workflow (abandon)
13. ✅ Abandon cancellation (answering 'n')
14. ✅ Workflow persistence after cancel
15. ✅ Multiple concurrent workflows

**Test Results:** 15/16 passing (93.75%)

**Usage:**
```bash
# Start daemon first
bin/argo-daemon --port 9876 &

# Run tests
./tests/integration/test_arc_complete.sh
```

## Query Parameter Implementation

All workflow endpoints now use explicit query parameters instead of path extraction:

- `/api/workflow/status?workflow_name=<name>`
- `/api/workflow/abandon?workflow_name=<name>`
- `/api/workflow/pause?workflow_name=<name>`
- `/api/workflow/resume?workflow_name=<name>`
- `/api/workflow/progress?workflow_name=<name>`

This ensures clear parameter passing without ambiguous path parsing.
