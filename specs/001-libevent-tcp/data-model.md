# Data Model: libevent TCP Network Layer Integration

**Branch**: `001-libevent-tcp` | **Date**: 2026-03-01

This document describes the key entities in the network layer, their state, relationships, and state transitions after the libevent integration. It focuses on the logical model, not implementation syntax.

---

## Entities

### EventLoop

Represents a single event-dispatch context, owned by exactly one thread. Every I/O and timer event is dispatched through an `EventLoop`.

| Attribute | Description |
|-----------|-------------|
| `base` | Internal libevent event base (replaces `Epoll`) |
| `owner_thread_id` | OS thread ID of the thread that runs this loop |
| `running` | Whether the loop is currently dispatching events |

**Invariants**:
- Exactly one thread runs a given `EventLoop` at any point in time.
- Cross-thread tasks are always posted via the scheduling mechanism, never called directly.

**Operations**:
- `start()` — Begin dispatching events; blocks until stopped.
- `stop()` — Request the loop to exit after the current iteration.
- `post(task)` — Schedule a zero-argument callable to run in the owning thread on the next iteration (thread-safe).
- `register_io(fd, events, callback)` — Register read/write interest on a file descriptor.
- `deregister_io(fd)` — Remove all interest on a file descriptor.

---

### Channel

Represents monitoring interest (read, write, or both) on a single file descriptor within an `EventLoop`. A `Channel` has no I/O logic of its own — it only delivers events to registered callbacks.

| Attribute | Description |
|-----------|-------------|
| `fd` | The file descriptor being monitored |
| `loop` | The `EventLoop` this channel belongs to |
| `read_event` | Internal read-event handle (one per monitored fd) |
| `write_event` | Internal write-event handle (conditionally active) |
| `read_callback` | Invoked when `fd` becomes readable |
| `write_callback` | Invoked when `fd` becomes writable |
| `edge_triggered` | Whether the event fires once per state change (ET) or repeatedly while state persists (LT) |

**State transitions**:
```
Created
  │
  ├─ enableRead()  ──► ReadEnabled
  │                        │
  │                        ├─ enableWrite() ──► ReadWriteEnabled
  │                        │                        │
  │                        │                   disableWrite() ──► ReadEnabled
  │                        │
  │                   disableAll() ──► Idle
  │
  └─ (not added to loop) ──► Idle
```

**Relationships**:
- Belongs to exactly one `EventLoop`.
- Owned by a `Connection`, `Acceptor`, or `Connector`.

---

### Connection

Represents a live, bidirectional TCP session between two endpoints. Owns the socket file descriptor, I/O buffers, and lifecycle callbacks.

| Attribute | Description |
|-----------|-------------|
| `conn_id` | Unique integer identifier within the connection pool |
| `fd` | Socket file descriptor |
| `state` | Current lifecycle state (see below) |
| `local_addr` | Local IP:port |
| `peer_addr` | Remote IP:port |
| `loop` | The `EventLoop` managing this connection |
| `channel` | The `Channel` monitoring `fd` for read/write events |
| `input_buffer` | Bytes received from the remote peer, awaiting upper-layer consumption |
| `output_buffer` | Bytes queued for transmission to the remote peer |
| `on_conn` | Callback — invoked once when the connection is fully established |
| `on_message` | Callback — invoked when new data is available in `input_buffer` |
| `on_close` | Callback — invoked exactly once when the connection is closed |

**State machine**:
```
CONNECTING
    │
    └─ ConnectionEstablished() ──► CONNECTED
                                        │
                                        ├─ force_close() / peer closed ──► DISCONNECTING
                                        │                                        │
                                        └────────────────────────────────► DISCONNECTED
```

**Validation rules**:
- `Send()` must only be called in state `CONNECTED`.
- `on_close` is invoked exactly once regardless of how the close was triggered.
- All callbacks are invoked on the `EventLoop` thread that owns the connection.

**Relationships**:
- Belongs to exactly one `EventLoop`.
- Owned by a `Server` (connection pool) or a `Client` (single connection slot).
- Holds one `Channel` for I/O monitoring.

---

### Buffer

An ordered, byte-oriented queue. Supports efficient append at the write end and consume at the read end. Used as the `input_buffer` and `output_buffer` of a `Connection`.

| Attribute | Description |
|-----------|-------------|
| `data` | Internal contiguous byte storage |
| `read_index` | Position of the first unconsumed byte |
| `write_index` | Position of the next byte to be written |
| `prepend_space` | Fixed-size prefix area (8 bytes) reserved for length-prefixed framing |

**Invariants**:
- `read_index <= write_index <= data.size()`
- Readable bytes = `write_index - read_index`
- Writable bytes = `data.size() - write_index`

**Operations**:
- `Append(bytes)` — Add bytes to the write end; auto-expands if needed.
- `Peek()` — View readable bytes without consuming.
- `Retrieve(n)` — Consume `n` bytes from the read end.
- `RetrieveAll()` — Consume all bytes, reset to initial state.

---

### Server (Listener)

Binds to a local address and accepts incoming `Connection` objects on behalf of the application. Maintains a pool of active connections.

| Attribute | Description |
|-----------|-------------|
| `loop` | The `EventLoop` on which the listener runs |
| `listen_addr` | IP:port to bind to |
| `backlog` | Maximum pending connection queue depth |
| `connections` | Map of `conn_id → Connection` for all currently active connections |
| `on_connect` | Callback invoked for each newly accepted `Connection` |
| `on_message` | Default message callback applied to accepted connections |
| `on_disconnect` | Callback invoked when any connection closes |
| `listener` | Internal accept-listener handle (wraps the listening socket) |

**Operations**:
- `start()` — Begin accepting connections.
- `getConnection(conn_id)` — Retrieve an active connection by ID.
- `disconnectHandle(conn)` — Remove a closed connection from the pool.

**Relationships**:
- Owns zero or more `Connection` objects.
- Belongs to one `EventLoop`.

---

### Connector

Manages the lifecycle of a single outbound connection attempt from the client side.

| Attribute | Description |
|-----------|-------------|
| `loop` | The `EventLoop` on which the connection attempt runs |
| `server_addr` | The remote address to connect to |
| `state` | Current connection attempt state: `Disconnected → Connecting → Connected` |
| `on_new_connection` | Callback delivering the connected socket fd to the caller |

**State transitions**:
```
Disconnected
    │
    └─ start() ──► Connecting
                       │
                       ├─ success ──► Connected ──► on_new_connection(fd) ──► Disconnected*
                       │
                       └─ failure / stop() ──► Disconnected
```
\* After delivering the fd, `Connector` resets to `Disconnected` (the `Connection` object takes over the fd).

**Relationships**:
- Belongs to one `EventLoop`.
- Used by `Client`.

---

### Client

Manages a single outbound TCP session from the application's perspective. Uses a `Connector` to establish the connection, then holds the resulting `Connection`.

| Attribute | Description |
|-----------|-------------|
| `loop` | The `EventLoop` this client belongs to |
| `server_addr` | Remote address |
| `connector` | The `Connector` instance that performs the connect |
| `conn` | The active `Connection` (null until connected) |
| `on_connect` | Callback delivered when connection is established |
| `on_message` | Callback for incoming data |
| `on_disconnect` | Callback for connection close |

---

## Entity Relationships Summary

```
EvPoolThread
  └─ owns ──► EventLoop (1:1 per thread)
                   └─ manages ──► Channel (0..*)
                   └─ dispatches events to ──► Connection (0..*)

Server
  └─ belongs to ──► EventLoop
  └─ owns ──► Connection (0..*)
  └─ uses ──► Acceptor (internal listener)

Client
  └─ belongs to ──► EventLoop
  └─ owns ──► Connection (0 or 1)
  └─ uses ──► Connector

Connection
  └─ belongs to ──► EventLoop
  └─ owns ──► Channel (1)
  └─ owns ──► Buffer × 2 (input + output)
```
