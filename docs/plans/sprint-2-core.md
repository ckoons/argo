© 2025 Casey Koons All rights reserved

# Sprint 2: Core Features (Weeks 3-4)

## Objectives

Build the core Argo features for parallel AI development: session management, inter-AI communication, and the foundation of merge negotiation protocol.

## Deliverables

1. **Session Management System**
   - Multiple concurrent AI sessions
   - Session-to-branch mapping
   - Session lifecycle management

2. **Inter-AI Communication**
   - Full Terma-style messaging
   - Broadcast capabilities
   - Message queuing and delivery

3. **Merge Negotiation Foundation**
   - Conflict detection algorithm
   - Negotiation message protocol
   - Basic resolution strategies

4. **Integration Testing**
   - Multi-session test scenarios
   - Message flow validation
   - Merge conflict simulations

## Task Breakdown

### Week 3: Session Management and Communication

#### Day 11-12: Session Manager
- [ ] Implement argo_session.c
- [ ] Session creation and destruction
- [ ] Session-to-branch association
- [ ] Session state machine

#### Day 13-14: Message Router Enhancement
- [ ] Implement argo_router.c
- [ ] Message queue per session
- [ ] Broadcast message support
- [ ] Message delivery confirmation

#### Day 15: Inter-Session Communication
- [ ] Direct session-to-session messaging
- [ ] Purpose-based routing (@planner, @coder)
- [ ] Message history tracking
- [ ] Inbox functionality (new/keep queues)

### Week 4: Merge Negotiation

#### Day 16-17: Conflict Detection
- [ ] Implement argo_merge.c
- [ ] Git diff parsing
- [ ] Conflict identification algorithm
- [ ] Conflict categorization (syntax, semantic, logical)

#### Day 18-19: Negotiation Protocol
- [ ] Negotiation message types
- [ ] Proposal/counter-proposal system
- [ ] Voting mechanism for multiple AIs
- [ ] Consensus detection

#### Day 20: Integration and Testing
- [ ] Multi-session integration tests
- [ ] Merge simulation tests
- [ ] Performance benchmarks
- [ ] Documentation updates

## Code Samples

### Session Manager (argo_session.c)
```c
/* argo_session.c */
/* © 2025 Casey Koons All rights reserved */
#include "argo.h"
#include "argo_session.h"

typedef enum {
    SESSION_CREATED,
    SESSION_ACTIVE,
    SESSION_NEGOTIATING,
    SESSION_MERGING,
    SESSION_CLOSED
} argo_session_state_t;

typedef struct argo_session {
    char id[ARGO_SESSION_ID_LEN];
    char branch[ARGO_BRANCH_LEN];
    char purpose[ARGO_PURPOSE_LEN];
    argo_session_state_t state;
    int socket_fd;
    pid_t pid;
    time_t created;
    time_t last_activity;
    struct argo_session* next;
} argo_session_t;

argo_session_t* argo_session_create(argo_context_t* ctx,
                                    const char* branch,
                                    const char* purpose) {
    argo_session_t* session = calloc(1, sizeof(argo_session_t));
    if (!session) return NULL;

    argo_generate_id(session->id, ARGO_SESSION_ID_LEN);
    strncpy(session->branch, branch, ARGO_BRANCH_LEN - 1);
    strncpy(session->purpose, purpose, ARGO_PURPOSE_LEN - 1);
    session->state = SESSION_CREATED;
    session->created = time(NULL);
    session->last_activity = session->created;

    /* Add to context session list */
    session->next = ctx->sessions;
    ctx->sessions = session;

    return session;
}
```

### Merge Negotiation (argo_merge.c)
```c
/* argo_merge.c */
/* © 2025 Casey Koons All rights reserved */
#include "argo.h"
#include "argo_merge.h"

typedef struct argo_conflict {
    char file[ARGO_PATH_MAX];
    int line_start;
    int line_end;
    char* content_a;
    char* content_b;
    argo_conflict_type_t type;
    struct argo_conflict* next;
} argo_conflict_t;

typedef struct argo_proposal {
    char session_id[ARGO_SESSION_ID_LEN];
    char* resolution;
    float confidence;
    char* rationale;
} argo_proposal_t;

typedef struct argo_negotiation {
    char id[ARGO_MSG_ID_LEN];
    argo_conflict_t* conflicts;
    argo_proposal_t* proposals;
    int proposal_count;
    argo_negotiation_state_t state;
} argo_negotiation_t;

argo_negotiation_t* argo_merge_start(const char* branch_a,
                                     const char* branch_b) {
    argo_negotiation_t* neg = calloc(1, sizeof(argo_negotiation_t));
    if (!neg) return NULL;

    argo_generate_id(neg->id, ARGO_MSG_ID_LEN);
    neg->state = NEGOTIATION_DETECTING;

    /* Detect conflicts between branches */
    neg->conflicts = detect_conflicts(branch_a, branch_b);

    if (neg->conflicts) {
        neg->state = NEGOTIATION_PROPOSING;
    } else {
        neg->state = NEGOTIATION_COMPLETE;
    }

    return neg;
}
```

### Message Router Enhancement (argo_router.c)
```c
/* argo_router.c */
/* © 2025 Casey Koons All rights reserved */
#include "argo.h"
#include "argo_router.h"

#define ARGO_MSG_QUEUE_SIZE 100

typedef struct argo_msg_queue {
    argo_message_t* messages[ARGO_MSG_QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;
} argo_msg_queue_t;

int argo_router_broadcast(argo_context_t* ctx,
                         const char* from,
                         const char* type,
                         const char* payload) {
    argo_session_t* session = ctx->sessions;
    int sent = 0;

    while (session) {
        if (strcmp(session->id, from) != 0) {  /* Don't send to self */
            argo_message_t* msg = argo_message_create(type, from,
                                                      session->id,
                                                      payload);
            if (argo_router_send(ctx, msg) == 0) {
                sent++;
            }
            argo_message_free(msg);
        }
        session = session->next;
    }

    return sent;
}
```

## Protocol Specifications

### Negotiation Message Format
```json
{
  "type": "negotiation_start",
  "negotiation_id": "neg-123",
  "conflicts": [
    {
      "file": "src/main.c",
      "type": "semantic",
      "line_start": 45,
      "line_end": 52,
      "branch_a_content": "...",
      "branch_b_content": "..."
    }
  ]
}
```

### Proposal Message Format
```json
{
  "type": "negotiation_proposal",
  "negotiation_id": "neg-123",
  "session_id": "session-456",
  "proposals": [
    {
      "conflict_index": 0,
      "resolution": "merged content here",
      "confidence": 0.85,
      "rationale": "Combines functionality from both branches"
    }
  ]
}
```

## Testing Strategy

### Unit Tests
- Session creation/destruction
- Message routing accuracy
- Conflict detection algorithm
- Queue overflow handling

### Integration Tests
- 10 concurrent sessions messaging
- Broadcast storm handling
- Merge negotiation with 3 AIs
- Network partition simulation

### Performance Tests
- Message throughput (target: 10,000 msg/sec)
- Session scaling (target: 100 sessions)
- Merge resolution time (target: < 5 seconds)

## Dependencies and Risks

### Dependencies
- Sprint 1 completion (foundation)
- Git installed for diff operations
- POSIX threads for concurrency

### Risks
1. **Concurrency Complexity** - Multiple sessions need careful locking
2. **Memory Growth** - Message queues could grow unbounded
3. **Merge Algorithm** - Conflict detection accuracy

### Mitigation
- Use simple mutex locking initially
- Implement queue size limits
- Start with line-based diff, enhance later

## Success Criteria

1. **Functional**
   - 10+ concurrent sessions working
   - Messages routed correctly
   - Basic conflicts detected
   - Proposals generated and shared

2. **Quality**
   - No race conditions
   - No memory leaks
   - All tests passing
   - < 4000 lines total code

3. **Performance**
   - < 10ms message routing
   - < 100MB memory for 100 sessions
   - < 5 second merge negotiation

## Notes

- Keep negotiation protocol simple initially
- Focus on correctness over sophistication
- Document protocol for AI IDE makers
- Prepare demos for Sprint 3

## Review Checklist

- [ ] Thread-safe operations verified
- [ ] Message delivery guaranteed
- [ ] Conflict detection accurate
- [ ] Protocol well documented
- [ ] Integration tests comprehensive
- [ ] Memory usage bounded
- [ ] Performance targets met
- [ ] Ready for AI provider integration