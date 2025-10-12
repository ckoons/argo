# Remote Daemon Support

© 2025 Casey Koons. All rights reserved.

## Overview

Argo supports running the daemon on a remote host, enabling distributed deployment and centralized workflow coordination. All components (arc CLI, workflow executors, utilities) can connect to a daemon running anywhere on the network.

## Configuration

### Daemon Location

Configure daemon location via three methods (in precedence order):

1. **Config files** (recommended for persistent settings):
```bash
# ~/.argo/config/daemon.conf
daemon_host=daemon.example.com
daemon_port=9876
```

2. **Environment variables** (good for temporary overrides):
```bash
export ARGO_DAEMON_HOST=daemon.example.com
export ARGO_DAEMON_PORT=9876
```

3. **Defaults** (if nothing configured):
```
Host: localhost
Port: 9876
```

### Starting Remote Daemon

On the daemon host:
```bash
# Bind to all interfaces (default is localhost only)
argo-daemon --host 0.0.0.0 --port 9876
```

### Connecting from Arc CLI

On client machines:
```bash
# Option 1: Config file
mkdir -p ~/.argo/config
echo "daemon_host=daemon.example.com" > ~/.argo/config/daemon.conf
echo "daemon_port=9876" >> ~/.argo/config/daemon.conf

# Option 2: Environment
export ARGO_DAEMON_HOST=daemon.example.com
arc workflow list  # Connects to remote daemon
```

## Network Requirements

### Firewall Rules

Allow incoming TCP connections on daemon port (default 9876):
```bash
# Example: ufw (Ubuntu)
sudo ufw allow 9876/tcp

# Example: firewalld (RHEL/CentOS)
sudo firewall-cmd --add-port=9876/tcp --permanent
sudo firewall-cmd --reload
```

### Security Considerations

**Current Status**: No authentication (suitable for trusted networks only)

**Recommended Setup**:
- Run daemon on **private network** (not public internet)
- Use **SSH tunneling** for remote access:
  ```bash
  # On client machine
  ssh -L 9876:localhost:9876 daemon-host

  # In another terminal
  arc workflow list  # Connects through tunnel
  ```
- Use **firewall rules** to restrict access by IP
- Consider **VPN** for multi-user teams

**Future**: Authentication and TLS support planned

## Programming Pattern

### For Developers

**Never hard-code daemon URLs** - always use helpers:

```c
/* BAD - Hard-coded (fails with remote daemon) */
curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:9876/api/workflow/start");

/* GOOD - Dynamic construction */
#include "argo_daemon_client.h"

char url[512];
snprintf(url, sizeof(url), "%s/api/workflow/start", argo_get_daemon_url());
curl_easy_setopt(curl, CURLOPT_URL, url);
```

### Helper Functions

```c
/* Get daemon host (config → env → default) */
const char* argo_get_daemon_host(void);

/* Get daemon port (config → env → default) */
int argo_get_daemon_port(void);

/* Get complete base URL: "http://host:port" */
const char* argo_get_daemon_url(void);
```

### Arc CLI Example

Arc CLI already uses this pattern:
```c
/* arc/src/arc_http_client.c */
const char* arc_get_daemon_url(void) {
    // Checks config, environment, defaults
    // Returns: "http://host:port"
}

/* Usage in arc commands */
arc_http_response_t* resp;
arc_http_get("/api/workflow/list", &resp);  // Auto uses configured daemon
```

## Use Cases

### Development Team

**Scenario**: Team shares a central daemon for workflow coordination

```bash
# Daemon host (build server)
argo-daemon --host 0.0.0.0 --port 9876

# Developer machines
echo "daemon_host=buildserver.local" >> ~/.argo/config/daemon.conf
arc workflow run deploy.json  # Uses central daemon
```

### Multi-Host Workflows

**Scenario**: Workflows spawn executors on multiple hosts, all reporting to central daemon

```bash
# Central coordinator
argo-daemon --host 0.0.0.0 --port 9876

# Worker nodes
export ARGO_DAEMON_HOST=coordinator.local
argo_workflow_executor workflow_123  # Reports progress to central daemon
```

### Testing

**Scenario**: Run daemon on custom port for testing

```bash
# Test daemon
argo-daemon --port 9999

# Test client
export ARGO_DAEMON_PORT=9999
arc workflow list  # Uses test daemon
```

## Troubleshooting

### Connection Refused

```
Error: Failed to connect to daemon: http://daemon.example.com:9876
```

**Check**:
1. Is daemon running? `ssh daemon.example.com pgrep argo-daemon`
2. Is daemon listening on correct interface? `ss -tlnp | grep 9876`
3. Is firewall blocking? `telnet daemon.example.com 9876`
4. Is client configured correctly? `echo $ARGO_DAEMON_HOST`

### Wrong Daemon

```
Error: Workflow not found
```

**Check**: May be connecting to wrong daemon
```bash
# Show current daemon URL
arc workflow list --debug  # Shows URL being used

# Verify daemon identity
curl http://daemon.example.com:9876/api/health
```

### Network Latency

For high-latency networks (>100ms), consider:
- Increase timeouts in curl settings
- Batch operations where possible
- Use local daemon with remote workflow execution

## Examples

### SSH Tunnel Setup

```bash
#!/bin/bash
# tunnel-to-daemon.sh

REMOTE_HOST="daemon.example.com"
LOCAL_PORT=9876
REMOTE_PORT=9876

# Start tunnel in background
ssh -fNT -L ${LOCAL_PORT}:localhost:${REMOTE_PORT} ${REMOTE_HOST}

# Verify tunnel
if curl -s http://localhost:${LOCAL_PORT}/api/health > /dev/null; then
    echo "Tunnel established"
else
    echo "Tunnel failed"
    exit 1
fi
```

### Config Management

```bash
# Project-specific daemon
# <project>/.argo/config/daemon.conf
daemon_host=project-daemon.local
daemon_port=9876

# User's home daemon (lower precedence)
# ~/.argo/config/daemon.conf
daemon_host=personal-daemon.local
daemon_port=9876
```

## Future Enhancements

- **HTTPS support**: TLS encryption for daemon connections
- **Authentication**: API keys or token-based auth
- **Service discovery**: Auto-detect daemons on network
- **Load balancing**: Multiple daemons for high availability
- **Proxy support**: HTTP proxy configuration

---

**Status**: Remote daemon support is **functional** but assumes trusted network. Production deployments should use SSH tunneling or VPN until authentication is added.
