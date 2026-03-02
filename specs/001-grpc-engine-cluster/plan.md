# Implementation Plan: Engine Cluster gRPC Client

**Branch**: `001-grpc-engine-cluster` | **Date**: 2026-03-02 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-grpc-engine-cluster/spec.md`

---

## Summary

Add a gRPC client component to the NCX engine (`ncxs`) that connects to an external engine proxy service and participates in cluster management. The engine registers itself on startup, sends periodic heartbeats reporting its load (occupied proxy count), and responds to real-time port queries from the proxy. This runs on a dedicated background thread, completely separate from the libevent EventLoop reactor, preserving the performance-first architecture.

**Scope boundary**: Only the NCX engine side (`app/ncx_server/`, `app/protocol/`). The engine proxy (Go microservice) is out of scope.

**Proto contract**: `specs/001-grpc-engine-cluster/contracts/engine-cluster-stream.proto` (package `enginecluster.v1`, service `EngineClusterStream`)

---

## Technical Context

**Language/Version**: C++20 (GCC, `-std=c++20`)
**Primary Dependencies**: gRPC C++ (`grpc++`), libprotobuf, libevent 2.x (existing), `tomlplusplus` (existing)
**New Runtime Dependency**: `grpc++` — requires justification (see Complexity Tracking)
**Storage**: None (in-memory snapshot only; `std::mutex`-protected vector in NServer)
**Testing**: Manual smoke test via quickstart.md; gRPC stub server for integration test
**Target Platform**: Linux (Ubuntu 22.04 LTS)
**Project Type**: System service / binary (`ncxs`)
**Performance Goals**: gRPC client thread must not interfere with libevent reactor throughput; heartbeat latency < 1 s; port query round-trip < 500 ms under normal conditions
**Constraints**: gRPC client runs on a dedicated `std::thread`; no blocking calls in the EventLoop thread; cross-thread state shared only via mutex-protected snapshot
**Scale/Scope**: Single gRPC stream per engine instance; supports N SControlChannels reporting available ports

---

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| **I. Performance-First** | ✅ PASS | gRPC client runs on a dedicated `std::thread`, never touches the libevent `EventLoop` thread. No blocking calls added to the reactor path. |
| **II. Protocol Correctness** | ✅ PASS | Existing `PROTOMSGUTIL` framing is untouched. `engine-cluster-stream.proto` (package `enginecluster.v1`) defines a separate gRPC service. Proto-generated files committed with `.proto` diff. |
| **III. Connection Lifecycle** | ✅ PASS | `GrpcEngineClient` manages its own gRPC channel and stream lifecycle cleanly. NServer's `SControlChannel` ownership model is unchanged. `GrpcEngineClient` uses RAII for gRPC channel. |
| **IV. Security by Design** | ✅ PASS | gRPC channel uses insecure credentials initially (matches existing unencrypted TCP approach). TLS addable later without API breakage. No credentials logged. |
| **V. Minimal Footprint** | ⚠️ VIOLATION (justified) | Adding `grpc++` runtime dependency and potential binary size > 2 MB. See Complexity Tracking. |
| **VI. Linux-Native** | ✅ PASS | gRPC C++ targets Linux. No Windows/macOS shims introduced. Build validated on Ubuntu 22.04 in Dockerfile. |

---

## Project Structure

### Documentation (this feature)

```text
specs/001-grpc-engine-cluster/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/
│   └── engine-cluster-stream.proto    # gRPC service contract (canonical)
└── tasks.md             # Phase 2 output (/speckit.tasks)
```

### Source Code Changes

```text
app/
├── protocol/
│   ├── engine-cluster-stream.proto   # NEW — copied from contracts/ (canonical contract)
│   ├── engine_cluster_stream.pb.h           # NEW — generated (committed)
│   ├── engine_cluster_stream.pb.cc          # NEW — generated (committed)
│   ├── engine_cluster_stream.grpc.pb.h      # NEW — generated (committed)
│   └── engine_cluster_stream.grpc.pb.cc     # NEW — generated (committed)
│
├── ncx_server/
│   ├── grpc_engine_client.h          # NEW — GrpcEngineClient class declaration
│   ├── grpc_engine_client.cpp        # NEW — GrpcEngineClient implementation
│   ├── nserver.h                     # MODIFIED — add EngineProxySnapshot, GrpcEngineClient member
│   └── nserver.cpp                   # MODIFIED — start GrpcEngineClient, expose snapshot update API
│
├── util/
│   └── config.h                      # MODIFIED — add EngineProxyConfig parsing
│
└── CMakeLists.txt                    # MODIFIED — find gRPC, link grpc++ to ncxs
```

**Structure Decision**: Single project layout (existing). No new directories at the repo root. All new files in existing `ncx_server/` and `protocol/` directories.

---

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|--------------------------------------|
| New runtime dependency: `grpc++` | Cross-language bidirectional streaming to Go engine proxy; gRPC is the specified communication protocol and is the only production-grade solution with proto schema + cross-language support | Plain TCP + custom protocol: would require reimplementing bidirectional streaming, codec, and reconnection on both Go and C++ sides — significantly more code and less maintainable |
| Binary size may exceed 2 MB (stripped `ncxs`) | gRPC C++ shared library is large; dynamic linking (`grpc++` as `.so`) keeps binary small but requires gRPC installed on the target | Accept dynamic linking as the deployment model; update Dockerfile to `apt-get install libgrpc++-dev`; document in quickstart |

---

## Design Detail: GrpcEngineClient

### Proto Contract Summary (engine-cluster-stream.proto)

```
service EngineClusterStream {
  rpc Connect(stream EngineMessage) returns (stream ProxyMessage)
}

// Upstream (Engine → Proxy):
EngineMessage.registration  → EngineRegistration  { engine_id, address, max_capacity }
EngineMessage.heartbeat     → EngineHeartbeat     { engine_id, active_proxy_count }
EngineMessage.capacity_report → CapacityReport    { engine_id, max_capacity, active_proxy_count }

// Downstream (Proxy → Engine):
ProxyMessage.capacity_request  → CapacityRequest  { request_id }
ProxyMessage.capacity_response → CapacityResponse { request_id, available_capacity }
```

### Capacity Query Exchange

```
Engine                                Engine Proxy
──────                                ────────────
                          ←────────   CapacityRequest(request_id="r1")

[read snapshot]
CapacityReport(engine_id,
  max_capacity,
  active_proxy_count)     ────────→

                          ←────────   CapacityResponse(request_id="r1",
                                        available_capacity=N)
```

Note: `CapacityReport` has no `request_id`. The engine sends a fresh report in response to any pending `CapacityRequest`; the proxy correlates via `CapacityResponse`.

### Class Interface

```cpp
// app/ncx_server/grpc_engine_client.h

class GrpcEngineClient {
public:
    DISALLOW_COPY_AND_MOVE(GrpcEngineClient);

    struct Config {
        std::string proxy_endpoint;       // "host:port" of engine proxy gRPC server
        std::string engine_id;            // unique engine identifier
        std::string engine_address;       // "ip:port" reported in EngineRegistration
        int32_t max_capacity;             // declared max concurrent proxy bridges
        int heartbeat_interval_sec = 10;
    };

    GrpcEngineClient(Config cfg, NServer* nserver);
    ~GrpcEngineClient();

    void start();   // spawns read thread + write thread
    void stop();    // cancels gRPC context, joins threads

private:
    void run_read(stream);    // blocks on stream->Read(), handles CapacityRequest/CapacityResponse
    void run_write(stream);   // sends EngineRegistration once, then periodic EngineHeartbeat

    void handle_capacity_request(stream);  // read snapshot, send CapacityReport

    Config cfg_;
    NServer* nserver_;                      // non-owning, for snapshot access
    std::thread read_thread_;
    std::thread write_thread_;
    std::atomic<bool> running_{false};
    grpc::ClientContext ctx_;               // cancelled on stop()
};
```

### NServer Changes

```cpp
// Added to NServer:

struct EngineProxySnapshot {
    int active_proxy_count = 0;   // sum of bridge_closers_.size() across all SControlChannels
    std::mutex mu;
};

// Called from EventLoop thread only (no locking needed for the caller):
void on_bridge_opened();    // increments snapshot.active_proxy_count under mutex
void on_bridge_closed();    // decrements snapshot.active_proxy_count under mutex

// Called from gRPC thread (thread-safe via snapshot.mu):
int get_active_proxy_count();
```

### Thread Communication Pattern

```
EventLoop thread                      GrpcEngineClient threads
─────────────────────                 ───────────────────────────────────
BridgeCloser registered →
  lock(snapshot.mu)
  snapshot.active_proxy_count++
  unlock()

BridgeCloser removed →
  lock(snapshot.mu)
  snapshot.active_proxy_count--
  unlock()
                                      [write_thread] on stream open:
                                        send EngineRegistration(engine_id,
                                          address, max_capacity)

                                      [write_thread] every heartbeat_interval s:
                                        lock(snapshot.mu)
                                        count = snapshot.active_proxy_count
                                        unlock()
                                        send EngineHeartbeat(engine_id, count)

                                      [read_thread] on CapacityRequest:
                                        lock(snapshot.mu)
                                        count = snapshot.active_proxy_count
                                        unlock()
                                        send CapacityReport(engine_id,
                                          max_capacity, count)

                                      [read_thread] on CapacityResponse:
                                        log available_capacity (informational)
```

---

## Config Extension

New `[engine_proxy]` section in `ncxs.toml`:

```toml
[engine_proxy]
proxy_endpoint     = "host:port"     # gRPC address of engine proxy (required)
engine_id          = "engine-01"     # Unique engine identifier (required)
engine_address     = "ip:port"       # This engine's public address (required)
max_capacity       = 100             # Max concurrent proxy bridges (required)
heartbeat_interval = 10              # Heartbeat period in seconds (optional, default 10)
```

If `[engine_proxy]` is absent, `GrpcEngineClient` is not created — backward-compatible.

`config.h`: Add `EngineProxyConfig` struct and `ServerConfig::parseEngineProxyConfig()` returning `std::optional<EngineProxyConfig>`.

---

## CMake Changes

```cmake
# app/CMakeLists.txt additions:

# gRPC
find_package(gRPC CONFIG QUIET)
if(NOT gRPC_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GRPC REQUIRED IMPORTED_TARGET grpc++)
    set(GRPC_TARGETS PkgConfig::GRPC)
else()
    set(GRPC_TARGETS gRPC::grpc++)
endif()

# Link gRPC to ncxs only (not ncxc)
target_link_libraries(ncxs ${GRPC_TARGETS})
```

---

## Implementation Sequence

### Phase A — Proto & Build System
1. Copy `engine-cluster-stream.proto` from `specs/contracts/` to `app/protocol/`
2. Generate `engine_cluster_stream.pb.*` and `engine_cluster_stream.grpc.pb.*` via `protoc`
3. Update `CMakeLists.txt` to find and link gRPC to `ncxs` only
4. Verify build succeeds (proto + cmake only, no logic changes yet)

### Phase B — NServer Snapshot
5. Add `EngineProxySnapshot` struct to `nserver.h` (active_proxy_count + mutex)
6. Add `on_bridge_opened()` / `on_bridge_closed()` methods to `NServer`
7. Wire these into `BridgeCloser` construction and destruction in `nserver.cpp`
8. Add `get_active_proxy_count()` thread-safe public method

### Phase C — GrpcEngineClient
9. Implement `grpc_engine_client.h` with correct `Config` fields (engine_id, max_capacity)
10. Implement `grpc_engine_client.cpp`:
    - `run_write()`: send `EngineRegistration` on stream open, then `EngineHeartbeat` every interval
    - `run_write()` uses `std::chrono::sleep_for` between heartbeats; checks `running_` flag
    - `run_read()`: blocks on `stream->Read(&proxy_msg)`, dispatch on `payload` type
    - On `CapacityRequest`: read snapshot, send `CapacityReport`; log received `CapacityResponse`
    - Reconnect loop: catch stream error, wait 5 s, retry
    - `stop()`: set `running_ = false`, call `ctx_.TryCancel()`, join both threads

### Phase D — Config & Integration
11. Add `EngineProxyConfig` struct to `config.h` (engine_id, engine_address, max_capacity, heartbeat_interval)
12. Add `ServerConfig::parseEngineProxyConfig()` returning `std::optional<EngineProxyConfig>`
13. In `nserver.h`: add `std::unique_ptr<GrpcEngineClient> grpc_client_` member
14. In `nserver.cpp` (`run_server()`): if config present, construct and `start()` `GrpcEngineClient`
15. In destructor / shutdown path: call `grpc_client_->stop()`

### Phase E — Validation
16. Build full `ncxs` binary — no warnings
17. Run quickstart smoke test against a stub gRPC server
18. Verify existing NCX tunnel flow (control channel + data channel) unaffected
