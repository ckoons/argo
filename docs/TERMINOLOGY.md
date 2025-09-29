© 2025 Casey Koons All rights reserved

# Argo Terminology Guide

This document maps concepts from Tekton to their Argo equivalents and defines new terms for the project.

## Command Line Tools

| Tekton | Argo | Why |
|--------|------|-----|
| `aish` | `arc` | **A**rgo **R**esolution **C**oordinator - short, memorable, no conflicts |
| `till` | N/A | Not needed - Argo is a library, not a platform |
| `tekton` | N/A | Not applicable |

## Core Concepts

| Tekton Term | Argo Term | Definition |
|-------------|-----------|------------|
| CI (Companion Intelligence) | **Agent** | An AI instance doing development work |
| Terma (terminal) | **Session** | A development context for an agent |
| Socket communication | **Channel** | Communication pathway between agents |
| Message | **Message** | Unchanged - clear enough |
| Inbox (new/keep) | **Queue** | Message queue for each session |
| Specialist | **Agent** or **Role** | Depends on context |

## Component Names

| Tekton Component | Argo Equivalent | Purpose |
|-----------------|-----------------|---------|
| Hermes | **Router** | Message routing between sessions |
| Engram | **Context** | Session memory and state |
| Apollo | **Analyzer** | Code analysis functions |
| Rhetor | Not needed | Prompt management not required |
| Athena | Not needed | Knowledge management handled by agents |
| Numa | Not needed | No central coordinator needed |

## Protocol Names

| Concept | Argo Term | Abbreviation |
|---------|-----------|--------------|
| Message Protocol | **Argo Message Protocol** | AMP |
| Negotiation Protocol | **Argo Negotiation Protocol** | ANP |
| Conflict Resolution | **Argo Conflict Resolution** | ACR |
| Session Protocol | **Argo Session Protocol** | ASP |

## Message Types

| Purpose | Argo Message Type | Description |
|---------|-------------------|-------------|
| Code change notification | `code_change` | Agent modified code |
| Conflict detected | `conflict_detected` | System found merge conflict |
| Negotiation request | `negotiate_request` | Start conflict resolution |
| Proposal | `proposal` | Suggested resolution |
| Vote | `vote` | Agreement with proposal |
| Resolution | `resolution` | Final agreed solution |
| Broadcast | `broadcast` | Message to all sessions |

## File and Function Naming

### File Names (boring is better)
- `argo_router.c` - Message routing
- `argo_session.c` - Session management
- `argo_channel.c` - Communication channels
- `argo_queue.c` - Message queues
- `argo_negotiate.c` - Conflict negotiation
- `argo_analyze.c` - Code analysis

### Function Prefixes
- `argo_` - Public API functions
- `arc_` - CLI tool functions
- `session_` - Session internal functions
- `channel_` - Channel internal functions
- `msg_` - Message handling internals

### Example Function Names
```c
// Public API (what it does)
argo_init()
argo_create_session()
argo_send_message()
argo_start_negotiation()

// Internal (still boring)
session_cleanup()
channel_connect()
msg_parse_json()
queue_push()
```

## AI Provider Terms

| Concept | Argo Term | Example |
|---------|-----------|---------|
| Provider | **Provider** | Claude, OpenAI, Ollama |
| Model | **Model** | gpt-4, claude-3, llama-3 |
| Completion | **Response** | What the AI returns |
| Prompt | **Query** | What we send to the AI |

## State Names

### Session States
- `SESSION_CREATED` - Just initialized
- `SESSION_ACTIVE` - Normal development
- `SESSION_NEGOTIATING` - In conflict resolution
- `SESSION_MERGING` - Applying resolution
- `SESSION_CLOSED` - Terminated

### Negotiation States
- `NEGOTIATE_PENDING` - Waiting to start
- `NEGOTIATE_ACTIVE` - Proposals being made
- `NEGOTIATE_VOTING` - Agents voting
- `NEGOTIATE_RESOLVED` - Consensus reached
- `NEGOTIATE_FAILED` - No consensus

## Configuration Keys

All configuration keys are lowercase with underscores:

```json
{
  "max_sessions": 100,
  "socket_path": "/tmp/argo.sock",
  "ai_provider": "claude",
  "api_key": "...",
  "message_timeout": 5000,
  "negotiation_timeout": 30000
}
```

## Error Codes

Using standard errno values plus Argo-specific:

```c
#define ARGO_SUCCESS         0
#define ARGO_ERROR          -1
#define ARGO_TIMEOUT        -2
#define ARGO_NO_CONSENSUS   -3
#define ARGO_SESSION_FULL   -4
#define ARGO_INVALID_MSG    -5
```

## Naming Decision Process

When naming something new:

1. **Default to boring** - What it does, not what it dreams of being
2. **Casey decides** - Present options: "Should we call this X (boring) or Y (creative)?"
3. **Consistency matters** - Follow existing patterns
4. **No clever abbreviations** - Full words when possible
5. **Ask when uncertain** - "What should we call the thing that..."

## Examples of Good vs Bad Names

### Good (Clear and Boring)
- `handle_message()` - Obviously handles a message
- `session_list` - A list of sessions
- `merge_conflict` - A merge conflict

### Bad (Too Clever or Vague)
- `hermes_whisper()` - What does this do?
- `sl` - Unclear abbreviation
- `situation` - Too vague

## Philosophy

We choose names that:
- A junior developer would understand immediately
- Don't require explanation
- Won't be confused with something else
- Are boring enough to be forgotten (infrastructure should be invisible)

When in doubt, choose the name that would make sense at 2 AM when debugging.

## Quick Reference Card

```
Command:      arc
Library:      libargo
Header:       argo.h
Config:       argo.conf
Socket:       /tmp/argo.sock
Sessions:     Not terminals
Agents:       Not CIs
Channels:     Not sockets
Queue:        Not inbox
```

---

*"The naming of cats is a difficult matter" - but the naming of functions should be dead simple.*