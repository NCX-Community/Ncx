# Research: gRPC Engine Cluster Client (NCX Engine Side)

**Feature**: 001-grpc-engine-cluster
**Date**: 2026-03-02
**Scope**: NCX engine (`ncxs`) gRPC client for engine proxy cluster management

---

## Decision 1: gRPC Thread Model for C++ Client

**Question**: How should the gRPC bidirectional stream be integrated with the existing libevent EventLoop?

**Decision**: Run the gRPC client on a **dedicated std::thread**, completely separate from the libevent `EventLoop`. The gRPC stream's read/write operations are synchronous blocking calls on this thread. The libevent reactor is never touched from the gRPC thread.

**Rationale**:
- Constitution Principle I forbids blocking calls inside the EventLoop thread. gRPC's synchronous read (`stream->Read()`) blocks until a message arrives — this is only safe off the reactor thread.
- gRPC's async `CompletionQueue` model (alternative) is more complex and not warranted for a single persistent stream.
- A dedicated thread cleanly separates concerns: libevent handles NCX tunnel traffic; the gRPC thread handles cluster management communication.

**Alternatives Considered**:
- **gRPC Async API + CompletionQueue**: More complex setup, not justified for a single stream with low message frequency (heartbeats every ~10 s). Rejected.
- **Running gRPC inside EventLoop via `event_base_once`**: Would require making gRPC reads non-blocking, which forces async gRPC API. Rejected for same reason above.

---

## Decision 2: gRPC C++ API Style — Synchronous Streaming

**Decision**: Use **synchronous blocking gRPC client streaming API** (`ClientReaderWriter<W,R>`), running on a dedicated thread.

**Pattern**:
```
GrpcEngineClient thread:
  1. Create channel → stub
  2. Open bidirectional stream: stub->Connect(&ctx) → stream
  3. Spawn internal write-loop: send EngineStatus on start, then every heartbeat_interval
  4. Read-loop: stream->Read(&msg) blocks until message arrives
     → on PortQueryRequest: collect available ports, write PortQueryResponse
  5. On stream error: reconnect after backoff delay
```

**Rationale**: Simple, correct, low code volume. The NCX engine sends infrequent messages (heartbeat every N seconds) and receives infrequent queries (only when a new connection is routed). Blocking I/O on a dedicated thread is perfectly appropriate.

**Alternatives Considered**:
- **gRPC callback API (experimental in some versions)**: Would avoid extra thread but is less stable. Rejected.

---

## Decision 3: Cross-Thread Port Query (gRPC Thread → NServer EventLoop Thread)

**Question**: How should the gRPC thread safely read the available proxy port list from NServer, which is owned by the EventLoop thread?

**Decision**: NServer maintains a **thread-safe snapshot** of available proxy port addresses, stored in a `std::vector<std::string>` protected by a `std::mutex`. The EventLoop thread updates this snapshot whenever a SControlChannel is added, removed, or its bridge count changes. The gRPC thread reads the snapshot under the mutex without posting back to the EventLoop.

**Rationale**:
- Avoids blocking the EventLoop: no need to post a task and wait (which would require a condition variable and serialise the event loop).
- The snapshot update events are low-frequency (service registration/deregistration, bridge open/close), so mutex contention is negligible.
- Snapshot reads by the gRPC thread are also low-frequency (only on PortQueryRequest).

**Alternative Considered**:
- **Post task to EventLoop + condition variable wait**: Correct but unnecessarily complex. Blocks the gRPC thread briefly and requires EventLoop cooperation. Rejected.

---

## Decision 4: Proto Service Shape

**Decision**: Single bidirectional streaming RPC `Connect` with `EngineMessage` (upstream) and `ProxyMessage` (downstream). `EngineMessage` uses `oneof` to carry either `EngineStatus` (registration+heartbeat combined) or `PortQueryResponse`. `ProxyMessage` uses `oneof` for `PortQueryRequest`.

**Rationale**:
- Registration and heartbeat carry the same fields (`address`, `occupied_proxy_count`); the first message acts as registration, subsequent ones are heartbeats.
- `oneof` keeps the proto future-extensible without breaking the stream API.
- Single bidirectional RPC is the simplest model; no need for separate RPCs for different message types.

**Alternatives Considered**:
- **Separate RPCs for registration and heartbeat**: Would require multiple streams, complicating lifecycle management. Rejected.
- **Server-side streaming only (proxy→engine) + unary for engine→proxy**: Breaks the full-duplex model needed. Rejected.

---

## Decision 5: gRPC Dependency Integration in CMake

**Decision**: Use `find_package(gRPC CONFIG REQUIRED)` for the `ncxs` target only. gRPC is **not** linked to `ncxc` (the client binary). CMakeLists.txt adds gRPC find logic and links `gRPC::grpc++` to `ncxs`.

**Rationale**: The gRPC cluster management feature is server-side only. Keeping `ncxc` free of gRPC avoids binary bloat for the client.

**Proto codegen**: `engine_cluster.pb.{h,cc}` and `engine_cluster.grpc.pb.{h,cc}` are pre-generated via:
```bash
protoc --cpp_out=protocol --grpc_out=protocol \
  --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) \
  -I=protocol protocol/engine_cluster.proto
```
Generated files are committed alongside the `.proto` diff (per Constitution Principle II).

---

## Decision 6: Config Extension

**Decision**: Extend `ncxs.toml` with an `[engine_proxy]` section:

```toml
[engine_proxy]
proxy_endpoint    = "192.168.1.100:50051"  # gRPC server address of engine proxy
engine_address    = "203.0.113.5:9001"     # This engine's public address reported to proxy
heartbeat_interval = 10                    # seconds between heartbeats
```

If the `[engine_proxy]` section is absent, `GrpcEngineClient` is not started (backward-compatible with deployments that don't use a cluster proxy).

---

## Complexity Tracking (Constitution Violations)

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|--------------------------------------|
| New runtime dependency: `grpc++` | Cross-language persistent stream to Go engine proxy requires gRPC; no other protocol provides bidirectional streaming with proto schema and cross-language support out of the box | Plain TCP with custom framing: would require reimplementing connection management, codec, and service discovery on both Go and C++ sides — far more code and less maintainable |
| Binary size may exceed 2 MB (stripped `ncxs`) | gRPC C++ static libs are large (~several MB); dynamic linking reduces this but requires gRPC installed on target | Static link is the current pattern; dynamic link acceptable if documented in Dockerfile and quickstart |
