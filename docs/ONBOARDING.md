© 2025 Casey Koons All rights reserved

# Argo Development Onboarding

Welcome. You're about to work on code that should feel like Steely Dan's "Aja" - seemingly effortless, precisely crafted, where every note belongs and nothing could be added or removed.

## Your Mission

Build a C library that enables multiple AI agents to develop code in parallel without conflicts. Think of it as building the USB standard for AI collaboration - universal, simple, essential.

## The Philosophy: Lazy Elegance

**"Simple, works, hard to screw up"**

We don't write clever code. We write code so clear that it seems obvious in hindsight. Like Donald Fagen and Walter Becker spending months perfecting a three-minute song, we obsess over simplicity until it flows naturally.

Key principles:
- If it looks complicated, it's wrong
- If you have to explain it, it's wrong
- If it could break easily, it's wrong
- If Casey says it's not done, it's not done

## How We Work: The Four Phases

### Phase 1: PLAN
You present a detailed approach with 2-3 alternatives. Think like a session musician showing up prepared with different arrangements.

Example:
```
"For the message router, I see three approaches:
1. Simple array with linear search (boring but bulletproof)
2. Hash table (faster but more complex)
3. Linked list (middle ground)

I recommend #1 because at 100 sessions, linear search is still <1ms"
```

### Phase 2: DISCUSS
Casey and you refine the approach. This is where we debate trade-offs, question assumptions, and sometimes discover option #4 that's better than all three.

### Phase 3: AGREE
Casey approves the approach. CRITICAL: We define all tests BEFORE writing code.

Test definition example:
```c
// Test: Message routing with 10 concurrent sessions
// Setup: Create 10 sessions, each sending 100 messages
// Verify: All messages delivered, no drops, <10ms average latency
// Cleanup: Properly close all sessions, no memory leaks
```

### Phase 4: BUILD
Write code to pass the tests. Casey decides when it's complete. No "good enough" - either the tests pass and the code is clean, or we're not done.

## Architecture Overview

```
User's IDE
    |
    ├── arc (command line tool)
    |     |
    |     └── libargo.a (static library)
    |           |
    |           ├── Message Router (channels between agents)
    |           ├── Session Manager (tracks parallel development)
    |           └── Negotiator (resolves conflicts)
    |
    └── LSP Server (optional)
          |
          └── libargo.a
```

Simple. Each piece does one thing well.

## Naming Conventions

When creating something new, Casey will ask:
- "What should we call this?"
- "Boring (what it does) or creative (what it represents)?"

Default to boring unless the creative name is obviously better.

Examples:
- `arc` - Command line tool (creative, but short and memorable)
- `session` - Not "terma" (boring but clear)
- `channel` - Not "socket communication" (simpler word)

See TERMINOLOGY.md for full Tekton→Argo translations.

## Your First Questions

Before writing any code, ask:
1. "What sprint are we on?"
2. "What's the specific task?"
3. "What should we call [component/function/concept]?"
4. "Simple implementation or sophisticated?"

Casey will always prefer simple unless there's a compelling reason.

## Testing Philosophy

**Tests come BEFORE code**

1. Define what success looks like
2. Write the test specification
3. Casey approves the test
4. Only then write code
5. Code is done when Casey says it's done

No partial credit. No "mostly working." It either works or it doesn't.

## Code Standards Quick Reference

From CLAUDE.md:
- Copyright notice on EVERY file: `© 2025 Casey Koons All rights reserved`
- No hardcoded paths or strings (all in headers)
- No environment variables (zero exceptions currently)
- No file over 600 lines
- All paths in headers, not .c files

## Communication Protocol

### Good:
"I see three ways to handle concurrent sessions. Option 1 is simplest but might not scale past 1000 sessions. Option 2 scales better but adds complexity. What's our target?"

### Bad:
"I implemented a sophisticated thread pool manager with work-stealing queues."

Always discuss BEFORE implementing.

## Sprint Structure

We work in 2-week sprints:
- Sprint 1: Foundation (sockets, messages)
- Sprint 2: Core (sessions, negotiation)
- Sprint 3: Integration (AI providers, packaging)
- Sprint 4: Demo (extension, outreach)

Check `docs/plans/sprint-X-*.md` for current sprint details.

## Common Traps to Avoid

1. **Over-engineering** - We're not building a framework
2. **Premature optimization** - Correctness first, speed later
3. **Feature creep** - If it's not in the sprint, it doesn't exist
4. **Assumption making** - When in doubt, ask Casey

## The Argo Way

We're building infrastructure that will be invisible but essential. Like the bass line in "Peg" - you don't notice it explicitly, but the song wouldn't work without it.

Every line of code should justify its existence. Every function should do one thing perfectly. Every module should be understandable in isolation.

## Ready?

Start by reading:
1. This document (done!)
2. `CLAUDE.md` - Coding standards
3. `docs/plans/architecture.md` - Technical design
4. Current sprint document
5. `TERMINOLOGY.md` - Name mappings

Then ask Casey: "What should I work on first?"

Remember: We're not writing code to impress. We're writing code that works so well it seems inevitable.

---

*"In the corner of my eye, I saw you in Rudy's, you were very high" - but our code stays grounded, simple, and sober.*