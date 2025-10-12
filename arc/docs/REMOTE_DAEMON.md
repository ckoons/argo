# Connecting Arc to Remote Daemon

© 2025 Casey Koons. All rights reserved.

## Quick Start

Arc can connect to an argo-daemon running on any host (local or remote).

### Configuration

**Option 1: Config file** (recommended)
```bash
mkdir -p ~/.argo/config
cat > ~/.argo/config/daemon.conf <<EOF
daemon_host=daemon.example.com
daemon_port=9876
EOF
```

**Option 2: Environment variables**
```bash
export ARGO_DAEMON_HOST=daemon.example.com
export ARGO_DAEMON_PORT=9876
arc workflow list
```

**Option 3: Default** (if nothing set)
```
Host: localhost
Port: 9876
```

## Testing Connection

```bash
# Check daemon is accessible
curl http://daemon.example.com:9876/api/health

# Use arc
arc workflow list
```

## Secure Remote Access

For daemons on untrusted networks, use SSH tunneling:

```bash
# Start tunnel
ssh -L 9876:localhost:9876 daemon.example.com

# In another terminal (connects through tunnel)
arc workflow list
```

## Troubleshooting

### Connection Failed

```bash
# 1. Check configuration
echo "Host: $ARGO_DAEMON_HOST"
echo "Port: $ARGO_DAEMON_PORT"

# 2. Test connectivity
telnet daemon.example.com 9876

# 3. Check daemon is running
ssh daemon.example.com pgrep argo-daemon
```

### Multiple Daemons

Arc connects to **one daemon at a time**. To switch:

```bash
# Project A daemon
cd ~/projects/projectA
export ARGO_DAEMON_HOST=daemon-a.local
arc workflow list  # Shows projectA workflows

# Project B daemon
cd ~/projects/projectB
export ARGO_DAEMON_HOST=daemon-b.local
arc workflow list  # Shows projectB workflows
```

## Implementation

Arc uses dynamic URL construction - see `arc/src/arc_http_client.c`:

```c
const char* arc_get_daemon_url(void) {
    // Checks ARGO_DAEMON_HOST, ARGO_DAEMON_PORT environment
    // Returns: "http://host:port"
}

// All arc commands use this helper
arc_http_get("/api/workflow/list", &response);
```

## See Also

- [Full Remote Daemon Guide](/docs/guide/REMOTE_DAEMON.md) - Complete reference
- [Configuration System](/docs/guide/CONFIGURATION.md) - Config hierarchy details
- [Security Considerations](/docs/guide/REMOTE_DAEMON.md#security-considerations)

---

**Note**: Arc automatically starts a local daemon if none is running. To disable this behavior (force remote-only), set `ARGO_NO_AUTO_DAEMON=1`.
