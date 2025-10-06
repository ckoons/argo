#!/usr/bin/env bash
# © 2025 Casey Koons All rights reserved
#
# Daemon Helper for Arc Tests
# Provides functions to start/stop daemon for testing

# Daemon configuration
DAEMON_PORT=${ARGO_DAEMON_PORT:-9876}
# Daemon binary - use absolute path if PROJECT_ROOT is set
if [[ -n "$PROJECT_ROOT" ]]; then
    DAEMON_BIN="$PROJECT_ROOT/bin/argo-daemon"
else
    DAEMON_BIN="bin/argo-daemon"
fi
DAEMON_LOG="/tmp/argo_test_daemon.log"
DAEMON_PID_FILE="/tmp/argo_test_daemon.pid"

# Start test daemon
start_test_daemon() {
    # Check if daemon already running
    if [[ -f "$DAEMON_PID_FILE" ]]; then
        local old_pid=$(cat "$DAEMON_PID_FILE")
        if ps -p "$old_pid" > /dev/null 2>&1; then
            echo "Daemon already running (PID: $old_pid)"
            return 0
        fi
        rm -f "$DAEMON_PID_FILE"
    fi

    # Check if daemon binary exists
    if [[ ! -f "$DAEMON_BIN" ]]; then
        echo "ERROR: Daemon binary not found: $DAEMON_BIN"
        echo "Current directory: $(pwd)"
        echo "Run 'make' from project root first"
        return 1
    fi

    if [[ ! -x "$DAEMON_BIN" ]]; then
        echo "ERROR: Daemon binary not executable: $DAEMON_BIN"
        ls -la "$DAEMON_BIN"
        return 1
    fi

    # Start daemon (must run from project root to find workflows/templates)
    echo "Starting test daemon on port $DAEMON_PORT..."
    if [[ -n "$PROJECT_ROOT" ]]; then
        (cd "$PROJECT_ROOT" && exec "$DAEMON_BIN" --port $DAEMON_PORT) > "$DAEMON_LOG" 2>&1 &
    else
        $DAEMON_BIN --port $DAEMON_PORT > "$DAEMON_LOG" 2>&1 &
    fi
    local daemon_pid=$!

    # Save PID
    echo $daemon_pid > "$DAEMON_PID_FILE"

    # Wait for daemon to be ready
    local max_attempts=10
    local attempt=0
    while [ $attempt -lt $max_attempts ]; do
        if curl -s "http://localhost:${DAEMON_PORT}/api/health" > /dev/null 2>&1; then
            echo "Daemon started successfully (PID: $daemon_pid)"
            return 0
        fi
        sleep 0.5
        attempt=$((attempt + 1))
    done

    # Failed to start
    echo "ERROR: Daemon failed to start"
    echo "Check log: $DAEMON_LOG"
    cat "$DAEMON_LOG"
    kill $daemon_pid 2>/dev/null || true
    rm -f "$DAEMON_PID_FILE"
    return 1
}

# Stop test daemon
stop_test_daemon() {
    if [[ ! -f "$DAEMON_PID_FILE" ]]; then
        echo "No daemon PID file found"
        return 0
    fi

    local daemon_pid=$(cat "$DAEMON_PID_FILE")

    if ! ps -p "$daemon_pid" > /dev/null 2>&1; then
        echo "Daemon not running"
        rm -f "$DAEMON_PID_FILE"
        return 0
    fi

    echo "Stopping daemon (PID: $daemon_pid)..."

    # Try graceful shutdown first
    kill -TERM "$daemon_pid" 2>/dev/null || true

    # Wait for shutdown
    local max_wait=5
    local waited=0
    while ps -p "$daemon_pid" > /dev/null 2>&1 && [ $waited -lt $max_wait ]; do
        sleep 0.5
        waited=$((waited + 1))
    done

    # Force kill if still running
    if ps -p "$daemon_pid" > /dev/null 2>&1; then
        echo "Daemon didn't stop gracefully, forcing..."
        kill -9 "$daemon_pid" 2>/dev/null || true
    fi

    rm -f "$DAEMON_PID_FILE"
    echo "Daemon stopped"
}

# Check if daemon is running
is_daemon_running() {
    if [[ -f "$DAEMON_PID_FILE" ]]; then
        local daemon_pid=$(cat "$DAEMON_PID_FILE")
        if ps -p "$daemon_pid" > /dev/null 2>&1; then
            return 0
        fi
    fi
    return 1
}

# Get daemon PID
get_daemon_pid() {
    if [[ -f "$DAEMON_PID_FILE" ]]; then
        cat "$DAEMON_PID_FILE"
    else
        echo ""
    fi
}

# Wait for daemon to be ready
wait_for_daemon() {
    local max_attempts=${1:-20}
    local attempt=0

    while [ $attempt -lt $max_attempts ]; do
        if curl -s "http://localhost:${DAEMON_PORT}/api/health" > /dev/null 2>&1; then
            return 0
        fi
        sleep 0.5
        attempt=$((attempt + 1))
    done

    return 1
}

# Show daemon status
daemon_status() {
    if is_daemon_running; then
        local pid=$(get_daemon_pid)
        echo "Daemon running (PID: $pid, Port: $DAEMON_PORT)"
        return 0
    else
        echo "Daemon not running"
        return 1
    fi
}

# Export functions for use in test scripts
export -f start_test_daemon
export -f stop_test_daemon
export -f is_daemon_running
export -f get_daemon_pid
export -f wait_for_daemon
export -f daemon_status
