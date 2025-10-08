© 2025 Casey Koons All rights reserved

# Argo Development Workflow

This document defines the exact process for developing Argo. Follow it religiously.

## Sprint = Branch Philosophy

Every sprint is a Git branch. This provides:
- **Isolation**: Each sprint can't break others
- **Atomicity**: Entire sprint merges or doesn't
- **Traceability**: Git history shows sprint boundaries
- **User Control**: Only user decides merge/abandon

## The Development Workflow (Not State Machine)

### The Seven Phases

1. **Proposal** - Gather and understand requirements
2. **Define Sprint** - Scope the work, create branch
3. **Review Sprint** - What will we build?
4. **Agree on Approach** - Define acceptance criteria and tests
5. **Execute Sprint** - Build, test, iterate:
   - a) Write test specifications
   - b) Write code to pass tests
   - c) Run unit tests
   - d) Run functional tests
   - e) Run integration tests
   - f) Evaluate: Done or revise?
6. **Complete Phase** - Document progress, update onboarding
7. **Next Phase** - Return to step 3 or finish

### The Interleaved Pattern

Every CI interaction follows this pattern:
```
DETERMINISTIC → CI → DETERMINISTIC → CI → DETERMINISTIC
```

Example:
```
DETERMINISTIC: Create sprint branch, load context
    ↓
CI: Review requirements, propose approach
    ↓
DETERMINISTIC: Setup test framework
    ↓
CI: Write test cases
    ↓
DETERMINISTIC: Run tests, report results
```

CIs never control infrastructure - they provide creative input TO the deterministic system.

## The Four Phases (In Order, No Skipping)

### Phase 1: PLAN (Before Any Code)

#### What You Do:
1. Understand the task completely
2. Research existing code if modifying
3. Present 2-3 implementation approaches
4. Include trade-offs for each
5. Make a recommendation with reasoning

#### Format:
```markdown
## Task: Implement message router

### Approach 1: Array-based (Recommended)
- Simple fixed-size array of sessions
- Linear search for routing
- Pros: Dead simple, predictable memory, hard to break
- Cons: O(n) search time
- Code estimate: ~100 lines

### Approach 2: Hash table
- Hash session IDs for O(1) lookup
- Pros: Faster for many sessions
- Cons: More complex, collision handling, dynamic memory
- Code estimate: ~300 lines

### Approach 3: Linked list
- Dynamic list of sessions
- Pros: No size limits
- Cons: Still O(n), memory fragmentation
- Code estimate: ~150 lines

### Recommendation: Approach 1
At 100 sessions, linear search is <1ms. Simplicity wins.
```

#### Casey's Response:
- May pick your recommendation
- May suggest Approach 4 you didn't consider
- May combine approaches
- May simplify further

### Phase 2: DISCUSS (Refine Until Right)

#### The Discussion:
- Casey asks clarifying questions
- You provide additional detail
- Edge cases are explored
- Performance requirements confirmed
- Naming decisions made

#### Example Dialog:
```
Casey: "What happens if array is full?"
You: "Return ARGO_SESSION_FULL error. Alternative: dynamic realloc?"
Casey: "No, fixed size is fine. Make it 1000 sessions."
You: "Should we log when 80% full?"
Casey: "Good idea. Simple warning to stderr."
```

#### Output:
- Clear specification of chosen approach
- All design decisions documented
- Edge cases identified
- Names for components agreed

### Phase 3: AGREE (Define Success Before Starting)

#### Define All Tests FIRST:

```c
/* Test 1: Create and destroy session */
// Setup: Initialize argo context
// Action: Create session with branch name
// Verify: Session ID generated, state is CREATED
// Cleanup: Destroy session, verify removal

/* Test 2: Message routing between sessions */
// Setup: Create 3 sessions
// Action: Send message from session 1 to session 2
// Verify: Session 2 receives, session 3 does not
// Cleanup: Destroy all sessions

/* Test 3: Array full handling */
// Setup: Create MAX_SESSIONS sessions
// Action: Try to create one more
// Verify: Returns ARGO_SESSION_FULL
// Cleanup: Destroy all sessions

/* Test 4: Performance - 100 sessions routing */
// Setup: Create 100 sessions
// Action: Route 10000 messages randomly
// Verify: All delivered, average < 1ms
// Cleanup: Check no memory leaks
```

#### Casey Approves Tests:
- "Yes, build it" - Proceed to Phase 4
- "Add test for X" - Define additional test
- "Simplify test Y" - Reduce complexity
- "What about edge case Z?" - Add test

#### No Code Until:
- All tests defined
- Casey explicitly approves
- Success criteria clear

### Phase 4: BUILD (Write Code to Pass Tests)

#### Development Process:

1. **Write minimal code to pass Test 1**
   ```c
   /* Start with simplest possible implementation */
   argo_session_t* argo_create_session(argo_context_t* ctx,
                                       const char* branch) {
       /* Just enough to pass Test 1 */
   }
   ```

2. **Run Test 1**
   - Pass? Move to Test 2
   - Fail? Fix and retest

3. **Incrementally add code for each test**
   - No extra features
   - No "while I'm here" additions
   - No premature optimization

4. **Check with Casey at milestones**
   ```
   You: "Tests 1-3 passing. Starting performance test."
   Casey: "Good. Show me the code so far."
   ```

#### Code Review Points:
- After first test passes
- When all functional tests pass
- Before optimization
- When you think you're done

#### Casey Declares Completion:
- "Ship it" - Task complete
- "Clean up X" - Minor fixes
- "Refactor Y" - Structural changes
- "Start over with approach Z" - Rare but happens

## Standard Patterns

### Pattern 1: Error Handling
```c
int argo_operation(argo_context_t* ctx, /* params */) {
    int result = 0;
    resource_t* res = NULL;

    /* Validate inputs */
    if (!ctx) return ARGO_ERROR;

    /* Allocate resources */
    res = acquire_resource();
    if (!res) {
        result = ARGO_NO_MEMORY;
        goto cleanup;
    }

    /* Do work */
    if (do_something(res) < 0) {
        result = ARGO_ERROR;
        goto cleanup;
    }

cleanup:
    if (res) release_resource(res);
    return result;
}
```

### Pattern 2: Test Structure
```c
void test_feature_name(void) {
    /* Setup */
    argo_context_t* ctx = argo_init(NULL);
    assert(ctx != NULL);

    /* Action */
    int result = argo_operation(ctx, /* params */);

    /* Verify */
    assert(result == ARGO_SUCCESS);
    assert(ctx->state == EXPECTED_STATE);

    /* Cleanup */
    argo_cleanup(ctx);
}
```

### Pattern 3: Planning Communication
```
You: "For [TASK], I see [N] approaches..."
Casey: "Tell me more about approach [X]"
You: "Here are the trade-offs..."
Casey: "Let's go with [X] but [MODIFICATION]"
You: "Understood. Here are the tests..."
```

## Decision Points

### When to Ask Casey:

Always ask when:
- Naming something new
- Choosing between approaches
- Performance vs simplicity trade-off
- Adding a feature not in spec
- Test is failing mysteriously
- Code exceeds complexity budget

Format:
```
"Should we call this [boring_name] or [creative_name]?"
"Simple approach is X, sophisticated is Y. Which?"
"This edge case isn't in spec. Handle it?"
```

### When to Make Decisions Yourself:

Decide yourself when:
- Following established patterns
- Implementing agreed approach
- Fixing obvious bugs
- Adding agreed tests
- Formatting/style issues

But document what you did:
```
"Used standard error handling pattern"
"Named per convention: argo_[noun]_[verb]"
"Added bounds check per defensive programming"
```

## Sprint Management

### Branch Lifecycle
```bash
# Sprint starts
git checkout -b sprint-1-foundation

# Work happens on sprint branch
# Multiple CIs can work here
# All changes isolated from main

# Sprint complete (Casey decides)
git checkout main
git merge sprint-1-foundation  # Only if Casey approves

# Clean up
git branch -d sprint-1-foundation  # Or archive if needed
```

### No Intermediate Merges
- Sprint branch never merges to main until complete
- No "partial delivery" that might break main
- Sprint is atomic unit - all or nothing

## Daily Flow

### Start of Session:
```
CI: "Resuming work on [FEATURE]. Currently at [STATUS]."
Casey: "Good. Continue with [SPECIFIC TASK]."
```

### During Development:
```
CI: "Tests 1-3 passing. Issue with test 4: [PROBLEM]"
Casey: "Try [SOLUTION]" or "Show me the failing test"
```

### End of Session:
```
CI: "Completed: [WHAT]. Next: [WHAT]. Blockers: [ANY]"
Casey: "Good progress" or "Let's revisit [ISSUE] tomorrow"
```

### Sprint Completion:
```
CI: "All tests passing, documentation updated. Sprint ready for review."
Casey: [Reviews] "Merge it" / "Fix X first" / "Abandon this approach"
```

## Quality Gates

### Code is NOT ready if:
- Any test fails
- Memory leaks exist
- Warnings during compilation
- Casey hasn't reviewed
- Documentation missing
- Error handling incomplete

### Code IS ready when:
- All defined tests pass
- Valgrind reports clean
- No compiler warnings
- Casey says "ship it"
- Headers documented
- Follows all patterns

## Common Workflow Mistakes

### Mistake 1: Coding Before Planning
```
WRONG: "I implemented a thread pool for handling sessions"
RIGHT: "I see three ways to handle sessions concurrently..."
```

### Mistake 2: Adding Unrequested Features
```
WRONG: "While implementing X, I also added Y and Z"
RIGHT: "X is complete. Should I also handle Y?"
```

### Mistake 3: Hiding Problems
```
WRONG: "It mostly works except for [hidden edge case]"
RIGHT: "Test 3 fails intermittently. Here's what I found..."
```

### Mistake 4: Over-Engineering
```
WRONG: "I created an abstract factory for message types"
RIGHT: "Simple switch statement handles all 5 message types"
```

## The Golden Rules

1. **No code before plan approval**
2. **No features before test definition**
3. **No complexity without justification**
4. **No assumptions without confirmation**
5. **No completion without Casey's approval**

## Remember

We're building infrastructure that should be invisible. Every decision should make the code:
- Simpler to understand
- Harder to break
- Easier to integrate
- More predictable

When in doubt:
- Choose boring
- Choose simple
- Choose obvious
- Ask Casey

---

*"The schedule is the schedule, the tests are the tests, and Casey is the arbiter of done."*