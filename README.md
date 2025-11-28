# TCP/UDP Server

A high-performance network server implementation using Linux system calls and epoll for handling both TCP and UDP connections concurrently.

# Dependencies

1. Boost 1.84 or higher - for logging functionality
2. C++17 compatible compiler

# Supported Commands

The server handles the following commands:

**_/stats_** - Returns connection statistics including total and active client counts.

Response format:
```
Total clients: 21. Active clients: 21
```

**_/time_** - Returns the current server local time.

Response format:
```
2025-11-28 15:04:05
```

**_/shutdown_** - Gracefully shuts down the server.

# Install

Build and install via makefile:

``` bash
make build && sudo make install
```

# Run

Run via makefile:

```bash
make run port
```