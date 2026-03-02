# Tasks: Engine Cluster gRPC Client

**Input**: Design documents from `/specs/001-grpc-engine-cluster/`
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/ ✅

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.
**Tests**: Not included (not requested in spec).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story this task belongs to (US1–US4)

---

## Phase 1: Setup (Proto & Build System)

**Purpose**: Copy the canonical proto contract, generate gRPC stubs, and wire gRPC into the CMake build. This phase must complete before any source logic is written.

- [x] T001 Copy `specs/001-grpc-engine-cluster/contracts/engine-cluster-stream.proto` to `app/protocol/engine-cluster-stream.proto`
- [x] T002 Run `protoc` to generate `engine_cluster_stream.pb.h`, `engine_cluster_stream.pb.cc`, `engine_cluster_stream.grpc.pb.h`, `engine_cluster_stream.grpc.pb.cc` in `app/protocol/` (command in quickstart.md Step 1)
- [x] T003 Update `app/CMakeLists.txt`: add `find_package(gRPC CONFIG QUIET)` / PkgConfig fallback for `grpc++`; add `grpc_cpp_plugin` invocation for proto; link `gRPC::grpc++` (or `PkgConfig::GRPC`) to `ncxs` target only
- [ ] T004 Verify CMake configure and `make ncxs` succeed with only proto+CMake changes (no logic yet); confirm `ncxc` is unaffected

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared infrastructure that every user story depends on — `EngineProxySnapshot` in NServer (thread-safe proxy count) and `EngineProxyConfig` parsing in config.h. Must complete before Phase 3+.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [x] T005 Add `EngineProxySnapshot` struct (fields: `int active_proxy_count`, `std::mutex mu`) to `app/ncx_server/nserver.h` as a member of `NServer`
- [x] T006 Declare `void on_bridge_opened()` and `void on_bridge_closed()` on `NServer` in `app/ncx_server/nserver.h`; implement them in `app/ncx_server/nserver.cpp` to increment/decrement `snapshot_.active_proxy_count` under `snapshot_.mu`
- [x] T007 Wire `on_bridge_opened()` into `BridgeCloser` construction and `on_bridge_closed()` into `BridgeCloser` destruction in `app/ncx_server/nserver.cpp`
- [x] T008 Declare and implement `int get_active_proxy_count()` on `NServer` in `app/ncx_server/nserver.h` and `nserver.cpp` — reads `snapshot_.active_proxy_count` under `snapshot_.mu`
- [x] T009 [P] Add `EngineProxyConfig` struct (fields: `proxy_endpoint`, `engine_id`, `engine_address`, `max_capacity`, `heartbeat_interval`) to `app/util/config.h`
- [x] T010 Implement `ServerConfig::parseEngineProxyConfig()` returning `std::optional<EngineProxyConfig>` in `app/util/config.h`; returns `std::nullopt` when `[engine_proxy]` section is absent (backward-compatible)

**Checkpoint**: NServer snapshot wired; config parsing done; build must still succeed.

---

## Phase 3: User Story 1 — Engine Registers with the Engine Proxy (Priority: P1) 🎯 MVP

**Goal**: The NCX engine establishes a persistent gRPC bidirectional stream on startup, sends `EngineRegistration` exactly once, and the engine proxy records the engine as active.

**Independent Test**: Start `ncxs` with a valid `[engine_proxy]` config pointing at a stub gRPC server. Verify the stub logs one `EngineMessage` with `registration` payload containing the configured `engine_id`, `address`, and `max_capacity`.

- [x] T011 Declare `GrpcEngineClient` class with nested `Config` struct (fields: `proxy_endpoint`, `engine_id`, `engine_address`, `max_capacity`, `heartbeat_interval_sec`) and method signatures `start()`, `stop()`, `run_read()`, `run_write()`, `handle_capacity_request()` in `app/ncx_server/grpc_engine_client.h`; include `DISALLOW_COPY_AND_MOVE`
- [x] T012 Implement `GrpcEngineClient` constructor (store config + NServer pointer) and `start()` (spawn `read_thread_` and `write_thread_`, set `running_ = true`) in `app/ncx_server/grpc_engine_client.cpp`
- [x] T013 [US1] Implement `run_write()` in `app/ncx_server/grpc_engine_client.cpp`: create gRPC channel → stub, call `stub->Connect(&ctx_, &stream)`, send one `EngineMessage` with `EngineRegistration{engine_id, address, max_capacity}` immediately after stream opens; log success/failure
- [x] T014 [US1] Add `std::unique_ptr<GrpcEngineClient> grpc_client_` member to `NServer` in `app/ncx_server/nserver.h`
- [x] T015 [US1] In `NServer::run_server()` (`app/ncx_server/nserver.cpp`): if `parseEngineProxyConfig()` returns a value, construct `GrpcEngineClient` from config and call `grpc_client_->start()`; if absent, skip (standalone mode)

**Checkpoint**: `ncxs` connects to stub proxy and stub logs the registration message. US1 is fully testable.

---

## Phase 4: User Story 2 — Engine Sends Periodic Heartbeats and Status Updates (Priority: P2)

**Goal**: After registration, the engine continuously sends `EngineHeartbeat` messages every `heartbeat_interval` seconds reflecting the current `active_proxy_count`, allowing the proxy to detect liveness and track load.

**Independent Test**: Register an engine (US1 complete), then open several proxy tunnels via `SControlChannel`. Verify the stub proxy receives subsequent `EngineHeartbeat` messages with `active_proxy_count` matching the number of open bridges.

- [x] T016 [US2] Extend `run_write()` in `app/ncx_server/grpc_engine_client.cpp`: after sending `EngineRegistration`, enter a loop — sleep `heartbeat_interval_sec` seconds, check `running_` flag, read `nserver_->get_active_proxy_count()`, send `EngineMessage` with `EngineHeartbeat{engine_id, active_proxy_count}`; exit loop when `running_` is false or write fails
- [x] T017 [US2] Implement `run_read()` skeleton in `app/ncx_server/grpc_engine_client.cpp`: block on `stream->Read(&proxy_msg)` in a loop; on read failure log stream error and set `running_ = false`; dispatch on `proxy_msg.payload()` type (cases: `capacity_request`, `capacity_response`)

**Checkpoint**: Heartbeat messages visible in stub proxy at the configured interval; count updates when bridges open/close.

---

## Phase 5: User Story 3 — Engine Proxy Queries Available Proxy Ports from an Engine (Priority: P3)

**Goal**: When the proxy sends a `CapacityRequest` downstream, the engine reads the current snapshot and responds with a `CapacityReport` containing `max_capacity` and `active_proxy_count`. The proxy uses this to compute remaining capacity.

**Independent Test**: With a registered, heartbeating engine, trigger a `CapacityRequest` from the stub proxy. Verify the engine sends back a `CapacityReport` with correct `max_capacity` and `active_proxy_count` reflecting current bridge state.

- [x] T018 [US3] Implement `handle_capacity_request()` in `app/ncx_server/grpc_engine_client.cpp`: call `nserver_->get_active_proxy_count()`, write `EngineMessage` with `CapacityReport{engine_id, max_capacity, active_proxy_count}` to stream; log request_id and report values
- [x] T019 [US3] In `run_read()` (`app/ncx_server/grpc_engine_client.cpp`): dispatch `capacity_request` → call `handle_capacity_request(stream)` passing the `request_id`; dispatch `capacity_response` → log `request_id` and `available_capacity` (informational only, no action required)

**Checkpoint**: Stub proxy receives `CapacityReport` in response to every `CapacityRequest`; reported values match live bridge state.

---

## Phase 6: User Story 4 — Engine Deregisters Gracefully or Is Detected as Lost (Priority: P4)

**Goal**: When `ncxs` shuts down cleanly, `GrpcEngineClient::stop()` cancels the gRPC context and joins both threads cleanly. When the stream is interrupted unexpectedly, `run_write()` / `run_read()` detect the error and reconnect after 5 seconds.

**Independent Test**: (a) Start engine + stub, then send SIGTERM to `ncxs` — stub should detect connection close cleanly. (b) Kill the stub while engine is running — engine log should show "Stream error … Reconnecting in 5s" and reconnect when stub restarts.

- [x] T020 [US4] Implement reconnect loop in `run_write()` in `app/ncx_server/grpc_engine_client.cpp`: wrap the channel-create + stream-open + registration + heartbeat-loop in a `while (running_)` outer loop; on stream write failure or end, log error, wait 5 seconds (`std::this_thread::sleep_for`), then retry
- [x] T021 [US4] Implement `stop()` in `app/ncx_server/grpc_engine_client.cpp`: set `running_ = false`, call `ctx_.TryCancel()`, join `read_thread_` and `write_thread_`
- [x] T022 [US4] Wire `GrpcEngineClient::stop()` into `NServer` destructor (or shutdown method) in `app/ncx_server/nserver.cpp`; guard with null check for standalone mode

**Checkpoint**: Clean shutdown completes without hanging; reconnect loop retries on disconnect without crashing.

---

## Phase 7: Polish & Validation

**Purpose**: End-to-end build verification and smoke test confirming all user stories work together and existing NCX tunnel traffic is unaffected.

- [ ] T023 Build full `ncxs` binary (`make ncxs`) with all changes; confirm zero compiler warnings and no regressions in `ncxc` build
- [ ] T024 [P] Run quickstart.md smoke test (Steps 1–5): start stub gRPC server, configure `ncxs.toml` with `[engine_proxy]`, run `ncxs`, verify all checklist items in quickstart.md pass
- [ ] T025 [P] Verify existing NCX tunnel flow is unaffected: start `ncxs` + `ncxc`, establish a tunnel, confirm control channel and data channel work correctly with the gRPC client thread running concurrently

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 (proto generated, cmake compiles) — **blocks all user stories**
- **US1 (Phase 3)**: Depends on Phase 2 completion
- **US2 (Phase 4)**: Depends on US1 (needs live gRPC stream open, run_read skeleton)
- **US3 (Phase 5)**: Depends on US2 (run_read dispatch loop must exist)
- **US4 (Phase 6)**: Depends on US1 (needs start/stop structure); can be worked in parallel with US2/US3
- **Polish (Phase 7)**: Depends on all user stories complete

### User Story Dependencies

- **US1 (P1)**: Requires Foundational (Phase 2) — no other story dependencies
- **US2 (P2)**: Requires US1 complete (extends `run_write`, adds `run_read` skeleton)
- **US3 (P3)**: Requires US2 complete (needs `run_read` dispatch loop)
- **US4 (P4)**: Requires US1 complete (needs `start()`/`stop()` structure); T020 extends `run_write` already modified in US2

### Within Each Phase

- T001 → T002 → T003 → T004 (sequential — each depends on previous)
- T005 → T006 → T007 → T008 (sequential — each depends on previous)
- T009 and T010 can run in parallel with T005–T008 (different file: `config.h`)
- T011 → T012 → T013 → T014 → T015 (US1 sequential — each builds on previous)
- T016 → T017 (US2 sequential — T017 read loop pairs with T016 write loop)
- T018 → T019 (US3 sequential — T019 wires T018)
- T020 → T021 → T022 (US4 sequential)
- T023 → T024, T025 (T024 and T025 parallel after T023)

### Parallel Opportunities

```bash
# Phase 2: Config can run in parallel with snapshot wiring
[P] T009, T010  (config.h changes)  ← run while T005–T008 run on nserver.h/cpp

# Phase 7: Smoke tests independent of each other
[P] T024  (quickstart smoke test)
[P] T025  (NCX tunnel regression test)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001–T004)
2. Complete Phase 2: Foundational (T005–T010)
3. Complete Phase 3: US1 Registration (T011–T015)
4. **STOP and VALIDATE**: Check stub proxy logs one `EngineRegistration` per engine start
5. Deploy/demo if ready

### Incremental Delivery

1. Setup + Foundational → Build compiles with gRPC, NServer snapshot wired
2. US1 → Registration works → validate independently
3. US2 → Heartbeat works → validate load updates
4. US3 → Capacity query works → validate round-trip
5. US4 → Reconnect + graceful stop works → validate resilience
6. Polish → Full smoke test + regression check

---

## Notes

- All new source files go in `app/ncx_server/` or `app/protocol/` — no new directories at repo root
- Generated proto files (`*.pb.h`, `*.pb.cc`, `*.grpc.pb.h`, `*.grpc.pb.cc`) must be committed alongside the `.proto` copy (per plan.md)
- gRPC is linked to `ncxs` **only** — `ncxc` must remain unaffected
- If `[engine_proxy]` is absent from `ncxs.toml`, `GrpcEngineClient` is never constructed — backward-compatible
- The libevent `EventLoop` thread must never be blocked by gRPC calls; all gRPC I/O happens on `read_thread_` and `write_thread_`
- `grpc::ClientContext ctx_` is cancelled in `stop()` via `ctx_.TryCancel()` to unblock the blocking `stream->Read()`
- Reconnect creates a **new** `grpc::ClientContext` each time (old context is not reusable after cancellation)
