© 2025 Casey Koons All rights reserved

# Sprint 1: Foundation (Weeks 1-2)

## Objectives

Establish the core foundation of Argo by porting the essential aish communication protocol from Tekton to C, creating the base for all inter-AI communication.

## Deliverables

1. **Core Message System**
   - Basic socket server/client in C
   - JSON message parsing without external libraries
   - Message routing between connections

2. **Protocol Implementation**
   - Port aish message format from Python to C
   - Implement basic message types (query, response, broadcast)
   - Session identification system

3. **Build System**
   - Makefile with proper targets
   - Static library compilation (libargo.a)
   - Test framework setup

4. **Unit Tests**
   - Socket connection tests
   - Message parsing tests
   - Protocol validation tests

## Task Breakdown

### Week 1: Core Infrastructure

#### Day 1-2: Project Setup
- [ ] Create directory structure per CLAUDE.md
- [ ] Write initial Makefile
- [ ] Create header files with type definitions
- [ ] Implement argo_local.h.example

#### Day 3-4: Socket Implementation
- [ ] Implement argo_socket.c with Unix socket support
- [ ] Create server socket binding and listening
- [ ] Implement client connection handling
- [ ] Add non-blocking I/O support

#### Day 5: JSON Parser
- [ ] Implement minimal JSON parser (argo_json.c)
- [ ] Support for objects, arrays, strings, numbers
- [ ] Serialization functions for C structures
- [ ] Error handling for malformed JSON

### Week 2: Protocol and Testing

#### Day 6-7: Message Protocol
- [ ] Port aish message format to C structures
- [ ] Implement message serialization/deserialization
- [ ] Create message routing logic
- [ ] Add message type handlers

#### Day 8-9: Session Management
- [ ] Implement basic session tracking
- [ ] Session ID generation
- [ ] Session lookup and management
- [ ] Memory management for session list

#### Day 10: Testing and Documentation
- [ ] Write unit tests for all components
- [ ] Create integration test for message flow
- [ ] Document API in headers
- [ ] Update README with build instructions

## Code Samples

### Message Structure (argo_types.h)
```c
/* argo_types.h */
/* © 2025 Casey Koons All rights reserved */
#ifndef ARGO_TYPES_H
#define ARGO_TYPES_H

#define ARGO_SESSION_ID_LEN 32
#define ARGO_MSG_ID_LEN 16
#define ARGO_MSG_TYPE_LEN 32

typedef struct argo_message {
    char id[ARGO_MSG_ID_LEN];
    char type[ARGO_MSG_TYPE_LEN];
    char from[ARGO_SESSION_ID_LEN];
    char to[ARGO_SESSION_ID_LEN];
    char* payload;
    size_t payload_len;
    time_t timestamp;
} argo_message_t;

#endif /* ARGO_TYPES_H */
```

### Socket Server Skeleton (argo_socket.c)
```c
/* argo_socket.c */
/* © 2025 Casey Koons All rights reserved */
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "argo.h"

#define ARGO_SOCKET_PATH "/tmp/argo.sock"
#define ARGO_BACKLOG 10

int argo_socket_create(void) {
    int fd;
    struct sockaddr_un addr;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ARGO_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(ARGO_SOCKET_PATH);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, ARGO_BACKLOG) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}
```

## Dependencies and Risks

### Dependencies
- C compiler (gcc/clang)
- POSIX system (Linux/macOS)
- Git for version control

### Risks
1. **JSON Parsing Complexity** - Keep minimal, only what's needed
2. **Memory Management** - Careful with allocations, use static buffers where possible
3. **Platform Compatibility** - Focus on POSIX first, Windows later

### Mitigation
- Use fixed-size buffers where possible
- Implement careful bounds checking
- Add memory leak detection in tests
- Document platform-specific code

## Success Criteria

1. **Functional**
   - Can create socket server
   - Can accept multiple client connections
   - Can route messages between clients
   - Can parse and generate JSON messages

2. **Quality**
   - All tests passing
   - No memory leaks (valgrind clean)
   - Code follows CLAUDE.md standards
   - < 2000 lines of code total

3. **Performance**
   - < 1ms local message routing
   - < 10MB memory usage
   - Handles 100 concurrent connections

## Notes

- Keep it simple - this is just the foundation
- No external dependencies means writing our own JSON parser
- Focus on correctness over optimization
- Document everything for future sprints

## Review Checklist

- [ ] All code has copyright notice
- [ ] No hardcoded paths (all in headers)
- [ ] All strings defined in headers
- [ ] No environment variables used
- [ ] All .c files < 600 lines
- [ ] Makefile builds without warnings
- [ ] Tests cover core functionality
- [ ] No memory leaks in valgrind