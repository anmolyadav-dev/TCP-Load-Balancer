# TCP Layer 4 Load Balancer

An async, event-driven TCP load balancer in modern C++20. Uses standalone Asio (`epoll` on Linux) to handle thousands of concurrent connections on a single thread — zero thread-per-connection overhead.

```
Client ──► :8080 (Load Balancer) ──► :8081 (Backend 1)
                                 ──► :8082 (Backend 2)
                                 ──► :8083 (Backend 3)
```

## How It Works

1. Single-threaded event loop (`asio::io_context`) listens for connections
2. Each connection creates a `Session` (managed by `shared_ptr` — RAII)
3. `async_connect` to a round-robin-selected backend
4. Two async relay chains (`async_read_some` → `async_write` → repeat) bridge client ↔ backend
5. When either side disconnects, `shutdown(SHUT_WR)` propagates EOF; `shared_ptr` ref drops to 0 → Session destroyed → sockets closed automatically

**No threads spawned per connection. No manual `close()`. No resource leaks.**

## Project Structure

```
.
├── load_balancer.cpp   # The load balancer (~120 lines)
├── test_backend.py     # Echo server for testing
├── Makefile            # Build with `make`
└── README.md
```

## Prerequisites

- Linux (uses epoll via Asio)
- g++ with C++20 support
- Python 3 (for the test backend)
- Standalone Asio headers (included in repo, or `apt install libasio-dev`)

## Build

```bash
make
```

## Run

### 1. Start test backends

```bash
python3 test_backend.py 8081 &
python3 test_backend.py 8082 &
python3 test_backend.py 8083 &
```

### 2. Start the load balancer

```bash
./lb 8080 127.0.0.1:8081 127.0.0.1:8082 127.0.0.1:8083
```

**Usage:** `./lb <listen_port> <host:port> [host:port ...]`

### 3. Test it

```bash
echo "hello 1" | nc -q1 127.0.0.1 8080   # → [backend:8081] hello 1
echo "hello 2" | nc -q1 127.0.0.1 8080   # → [backend:8082] hello 2
echo "hello 3" | nc -q1 127.0.0.1 8080   # → [backend:8083] hello 3
echo "hello 4" | nc -q1 127.0.0.1 8080   # → [backend:8081] hello 4  (wraps)
```

