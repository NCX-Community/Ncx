# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Ncx** is a lightweight, high-performance intranet penetration tool (similar to frp) written in C++20. It exposes local services to a public network via a server-client architecture. Only TCP is currently supported.

## Build Commands

**Dependencies (must be installed first):**
- `protobuf` + `libprotobuf-dev`
- `boost` (specifically `boost::program_options`)
- `libevent` + `libevent-dev` (core, extra, pthreads components)
- CMake 3.10+, g++ with C++20 support

**Build:**
```bash
mkdir build && cd build
cmake ..
make
```
This produces two binaries: `build/ncxs` (server) and `build/ncxc` (client).

**Run server:**
```bash
./build/ncxs ncxs.toml
```

**Run client:**
```bash
./build/ncxc ncxc.toml
```

**Regenerate protobuf files** (when `protocol/protocol.proto` changes):
```bash
protoc -I=protocol --cpp_out=protocol --experimental_allow_proto3_optional protocol/protocol.proto
```

**Docker build:**
```bash
docker build -t ncxs .
```
The Dockerfile handles all dependencies and compiles everything in an Ubuntu 22.04 environment.

## Architecture

### Core Network Engine (`network/`)

A custom Reactor-pattern async network framework backed by **libevent 2.x**:

- **`EventLoop`** — The core reactor. Wraps a libevent `event_base`, runs the event loop, and dispatches I/O events. Supports cross-thread task scheduling via `run_on_onwer_thread()` (uses `event_base_once` internally).
- **`Channel`** — Wraps a file descriptor and its I/O event callbacks. Owns `struct event *` members for read and write monitoring registered with the `event_base`.
- **`Connection`** — Represents a live TCP connection. Owns input/output `Buffer`s, raw `struct event *` members for I/O monitoring, and lifecycle callbacks (`on_conn_`, `on_message_`, `on_close_`). Inherits `enable_shared_from_this`.
- **`Buffer`** — Ring-buffer style byte buffer for async I/O.
- **`Server`** — Accepts incoming TCP connections via `Acceptor` (backed by `evconnlistener`), creates `Connection` objects. Maintains a connection pool (`unordered_map<int, shared_ptr<Connection>>`).
- **`Client`** / **`Connector`** — Initiates outgoing TCP connections via `bufferevent_socket_connect`.
- **`EvPoolThread`** — Spawns a background thread owning its own `EventLoop`. Used to run event loops on dedicated threads.

### Protocol Layer (`protocol/`)

Protobuf-based binary protocol with a 5-byte length-prefixed framing:

- **`protocol.proto`** defines: `MessageHeader`, `Hello` (with `CONTROL_CHANNEL_HELLO` / `DATA_CHANNEL_HELLO` subtypes), `Ack`, `ControlChannelCmd`, `DataChannelCmd`.
- **`PROTOMSGUTIL`** (singleton) — Handles framing: `HeaderInstaller()` prepends a serialized `MessageHeader`, `CanReadMsg()` checks if a complete message is buffered.

### NCX Server (`ncx_server/`)

- **`NServer`** — Top-level server. Listens for connections from NCX clients. On new connection, reads a `Hello` message to determine if it's a control channel or data channel connection.
- **`SControlChannel`** — Created per NCX client service. Runs a `proxy_server_` (`Server`) that listens on the configured `proxy_port` for external visitors. When an external visitor connects, it notifies the NCX client via control command to open a data channel. Maintains a `VisitorsQueue` and a `BridgeCloserMap`.
- **`BridgeCloser`** — Manages lifetime of a bridged connection pair (inside ↔ outside). When one side disconnects, closes the other.

### NCX Client (`ncx_client/`)

- **`NClient`** — Top-level client. Creates and starts `CControlChannel` instances for each configured service.
- **`CControlChannel`** — Connects to `NServer` and performs the control channel handshake (Hello → Ack). After handshake, waits for `CREATE_DATA_CHANNEL` commands from the server, then spawns a `DataChannel`.
- **`DataChannel`** — When triggered, connects to both `NServer` and the local service simultaneously. Performs a data channel handshake (sends `DATA_CHANNEL_HELLO` with nonce), then transparently forwards bytes bidirectionally between the server and the local service.

### Utilities (`util/`, `cli/`)

- **`util.h`** — Central include: all STL headers + forward declarations for all major classes. Also defines `DISALLOW_COPY`, `DISALLOW_MOVE`, `DISALLOW_COPY_AND_MOVE` macros.
- **`config.h`** — TOML config parsing (via `tomlplusplus`, fetched by CMake).
- **`singleton.h`** — CRTP singleton base class.
- **`uuid.h`** — UUID generation for bridge/channel IDs.
- **`cli/cli.h`** — CLI argument parsing (uses `boost::program_options`).

## Connection Lifecycle & Tunnel Establishment

1. NCX client connects to `NServer` → sends `CONTROL_CHANNEL_HELLO` with `service_name` and `proxy_port`.
2. `NServer` creates an `SControlChannel`, which starts a `proxy_server_` on `proxy_port`.
3. An external visitor connects to `proxy_port` → `SControlChannel` sends `CREATE_DATA_CHANNEL` command to the NCX client.
4. NCX client's `CControlChannel` receives the command → creates a `DataChannel` that connects to both `NServer` and the local service.
5. `DataChannel` sends `DATA_CHANNEL_HELLO` with a nonce to prove authenticity.
6. Server bridges the data channel connection with the waiting visitor connection. `BridgeCloser` manages the joint lifetime.

## Key Conventions

- All network code targets **Linux** (`epoll`, `eventfd`, `sys/socket.h`). The code does not build on Windows or macOS.
- `util.h` is the single include that most files use — add new common forward declarations there.
- Protobuf-generated files (`protocol.pb.cc`, `protocol.pb.h`) are committed to the repo. Re-generate only when `.proto` changes.
- `tomlplusplus` is fetched via `FetchContent` at configure time — no manual install needed.
- The `DISALLOW_COPY_AND_MOVE` macro is used on most non-trivial classes that shouldn't be copied.
