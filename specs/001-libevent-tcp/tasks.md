# Tasks: libevent TCP Network Layer Integration

**Input**: Design documents from `/specs/001-libevent-tcp/`
**Prerequisites**: plan.md ✓, spec.md ✓, research.md ✓, data-model.md ✓, quickstart.md ✓

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.
**Tests**: Not explicitly requested in the spec — no test tasks generated. Manual integration verification steps are included in the Polish phase per `quickstart.md`.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1–US4)
- Exact file paths included in every task description

---

## Phase 1: Setup (Build System & Dependency)

**Purpose**: Introduce libevent as a build dependency so the project compiles and links. No behaviour changes yet — this is a link-only checkpoint that unblocks all subsequent work.

- [X] T001 Add `libevent-dev` to the `apt-get install` line in `app/Dockerfile`
- [X] T002 Add `find_package(Libevent CONFIG REQUIRED COMPONENTS core extra pthreads)` with pkg-config fallback block to `app/CMakeLists.txt`
- [X] T003 Link `libevent::core libevent::extra libevent::pthreads` to both `ncxs` and `ncxc` targets in `app/CMakeLists.txt`
- [ ] T004 Verify `cmake .. && make` inside `app/build/` still produces working binaries (no libevent calls yet — compile-only checkpoint)

**Checkpoint**: Both binaries build and link successfully with libevent linked in.

---

## Phase 2: Foundational (EventLoop + Channel Rewrite)

**Purpose**: Replace the raw `epoll` backend (`Epoll` class + `Channel` epoll_ctl calls) with libevent's `event_base` and `struct event *`. This is the central infrastructure change that blocks all user-story implementation.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete. All TCP connection, I/O, and compatibility work depends on `EventLoop` and `Channel` being correct.

- [X] T005 Delete `app/network/epoll.h` and `app/network/epoll.cpp` from the repository
- [X] T006 Update `app/network/EventLoop.h`: remove `unique_ptr<Epoll> poller`, `int wakeup_fd_`, `unique_ptr<Channel> wakeup_channel_`, `vector<function<void()>> to_do_list`, `mutex mtx`; add `event_base* base_` member and `event_base* base() const` accessor; remove declarations of `wakeup_fd()`, `wakeup_loop()`, `wakeup_callback()`, `add_to_do()`, `do_after_handle_events()`
- [X] T007 Rewrite `app/network/EventLoop.cpp`: constructor calls `event_base_new()` and assigns to `base_`; destructor calls `event_base_free(base_)`; `run()` calls `event_base_loop(base_, EVLOOP_NO_EXIT_ON_EMPTY)`; `stop()` calls `event_base_loopbreak(base_)`; `run_on_owner_thread()` calls `cb()` directly if in owner thread, otherwise uses `event_base_once` shim that heap-allocates the `std::function` and frees it after invocation; remove all wakeup and to_do_list implementations; add `#include <event2/event.h>` and `#include <event2/thread.h>`
- [X] T008 Rewrite `app/network/channel.h`: remove `short listen_events_`, `short ready_events_`, `bool is_epolled_`; add `struct event* read_ev_` and `struct event* write_ev_` (both initially `nullptr`); remove `isEpolled()`, `setEpolled()`, `listen_events()`, `ready_events()`, `setRevs()`, `handle_event()`, `handle_event_guard()`; add `#include <event2/event.h>`
- [X] T009 Rewrite `app/network/channel.cpp`: `enableRead()` creates `read_ev_` with `event_new(loop_->base(), fd_, EV_READ | EV_PERSIST, read_shim, this)` and calls `event_add`; `enableWrite()` creates `write_ev_` similarly with `EV_WRITE | EV_PERSIST`; `disableWrite()` calls `event_del` + `event_free` on `write_ev_` and sets to `nullptr`; `disableAll()` del+frees both; `set_et()` stores an ET flag and includes `EV_ET` when creating events; `remove()` same as `disableAll()`; destructor calls `disableAll()`; static read/write shim functions extract `Channel*` from arg and invoke the corresponding stored callback
- [X] T010 [P] Update `app/util/util.h`: remove forward declaration of `class Epoll`; add `struct event_base;` forward declaration to avoid including the full libevent header in every translation unit
- [X] T011 [P] Update `app/network/common.h` (or equivalent shared header): remove any `#include "epoll.h"` references

**Checkpoint**: `EventLoop` and `Channel` compile cleanly. `EvPoolThread::StartLoop()` returns a valid loop whose `event_base` can accept event registrations.

---

## Phase 3: User Story 1 — Reliable TCP Connection Establishment (Priority: P1) 🎯 MVP

**Goal**: Inbound connections are accepted by the server and outbound connections are established by the client through the libevent-backed network layer. The control-channel handshake between `NServer` and `NClient` completes successfully.

**Independent Test**: Start `ncxs`, start `ncxc` pointing at the server. Confirm in logs that `CONTROL_CHANNEL_HELLO` is received and `Ack` is sent within 2 seconds. No data-channel or visitor traffic required.

### Implementation for User Story 1

- [X] T012 [US1] Rewrite `app/network/acceptor.h`: replace `unique_ptr<Channel>` and `TcpSocket` members with `struct evconnlistener* listener_` and `EventLoop* loop_`; preserve `set_new_connection_callback(function<void(int)>)` public interface; add `#include <event2/listener.h>`
- [X] T013 [US1] Rewrite `app/network/acceptor.cpp`: constructor calls `evconnlistener_new_bind` with `LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_CLOSE_ON_EXEC`, passing a static accept-callback shim that extracts the `Acceptor*` from `ptr` and invokes `new_connection_callback_(fd)`; destructor calls `evconnlistener_free(listener_)` if non-null
- [X] T014 [US1] Rewrite `app/network/connector.h`: replace `unique_ptr<Channel>` and direct-socket members with `struct bufferevent* bev_` for the pending connect attempt; preserve `setNewConnectionCb(function<void(int)>)` and `start()` / `stop()` public interface; add `#include <event2/bufferevent.h>`
- [X] T015 [US1] Rewrite `app/network/connector.cpp`: `connect()` creates a `bufferevent` via `bufferevent_socket_new(loop_->base(), -1, BEV_OPT_CLOSE_ON_FREE)`, calls `bufferevent_socket_connect` with the remote `sockaddr`; the event callback on `BEV_EVENT_CONNECTED` extracts the fd with `bufferevent_getfd(bev_)`, calls `setNewConnectionCb`'s stored callback with the fd, then calls `bufferevent_free(bev_)` and sets `bev_` to `nullptr` (fd ownership transfers to `Connection`); on `BEV_EVENT_ERROR` logs and resets state; `stop()` frees `bev_` if pending
- [X] T016 [US1] Update `app/network/connection.h`: replace `unique_ptr<Channel> channel` with `struct event* read_ev_` and `struct event* write_ev_` (both `nullptr` initially); add `#include <event2/event.h>`; update `get_epoll_run()` return type if needed
- [X] T017 [US1] Update `app/network/connection.cpp`: `ConnectionEstablished()` creates `read_ev_` via `event_new(loop_->base(), client_fd, EV_READ | EV_PERSIST, read_shim, this)` and calls `event_add`; `handle_data_out()` creates `write_ev_` on demand and calls `event_add` when output buffer has data, deletes it via `event_del` + `event_free` when output buffer drains; `ConnectionConstructor()` calls `event_del` + `event_free` on both events; destructor closes `client_fd`; static shims route to `handle_data_in()` and `handle_data_out()`; `ReadNonBlocking()` and `WriteNonBlocking()` bodies are unchanged
- [X] T018 [US1] Add `evthread_use_pthreads()` as the first statement in `main()` in `app/ncx_server/ncxs.cpp` (must precede any `EventLoop` construction); add `#include <event2/thread.h>`
- [X] T019 [US1] Add `evthread_use_pthreads()` as the first statement in `main()` in `app/ncx_client/ncxc.cpp` (must precede any `EventLoop` construction); add `#include <event2/thread.h>`
- [ ] T020 [US1] Build and smoke-test: run `ncxs` + `ncxc`, confirm control-channel handshake (`CONTROL_CHANNEL_HELLO` → `Ack`) completes without crash or hang

**Checkpoint**: `ncxs` and `ncxc` start, connect to each other, and complete the control-channel handshake. US1 is independently verified.

---

## Phase 4: User Story 2 — Non-Blocking Event-Driven I/O (Priority: P2)

**Goal**: The event loop dispatches all I/O callbacks without blocking. CPU usage is near 0 % at idle. Data sent over a connection arrives in the `on_message` callback correctly.

**Independent Test**: Send 1 000 messages over a single connection; verify all `on_message` callbacks fire and no data is lost. Verify CPU usage at idle (no connections) is near 0 % (no spin-loop).

### Implementation for User Story 2

- [X] T021 [US2] Audit `app/network/EventLoop.cpp`: confirm `event_base_loop` is called with `EVLOOP_NO_EXIT_ON_EMPTY` (not `EVLOOP_ONCE` or `0`); confirm no `while(true)` busy-wait remains in the file
- [X] T022 [US2] Audit `app/network/connection.cpp` `ReadNonBlocking()`: confirm the read loop exits on `EAGAIN`/`EWOULDBLOCK` and does not spin; confirm `on_message_` callback is invoked after each successful read burst
- [X] T023 [US2] Audit `app/network/connection.cpp` `WriteNonBlocking()`: confirm the write event (`write_ev_`) is removed via `event_del` + `event_free` as soon as `output_buffer` is empty, so the loop does not repeatedly fire with nothing to write
- [ ] T024 [US2] Verify `app/example/echo/` builds and functions correctly with the new network layer: start the echo server, connect a client, send data, confirm it echoes back; this exercises `on_message` callback delivery under the libevent backend

**Checkpoint**: No spin-loop or busy-wait. `on_message` callbacks fire for all received data. Echo example works end-to-end.

---

## Phase 5: User Story 3 — Upper-Layer API Compatibility (Priority: P3)

**Goal**: All existing NCX server and client components (`NServer`, `SControlChannel`, `CControlChannel`, `DataChannel`, `BridgeCloser`) operate correctly without any changes to their source files.

**Independent Test**: Run the full tunnel scenario: visitor connects to proxy port → `SControlChannel` sends `CREATE_DATA_CHANNEL` command → `CControlChannel` spawns `DataChannel` → data flows bidirectionally → connection closes cleanly and `BridgeCloser` fires exactly once.

### Implementation for User Story 3

- [X] T025 [US3] Verify no source files under `app/ncx_server/` were modified (read each file to confirm); if any were modified as a side-effect of earlier phases, revert the changes and fix the network layer instead
- [X] T026 [US3] Verify no source files under `app/ncx_client/` were modified; apply the same correction policy
- [ ] T027 [US3] Run full end-to-end tunnel test: start `ncxs`, start `ncxc`, connect an external visitor to the configured proxy port, transfer data, close the connection; confirm `BridgeCloser` callback fires exactly once and the connection pool in `SControlChannel` is cleaned up
- [ ] T028 [US3] Verify `Connection::Send()` delivers data intact by piping known byte sequences through the tunnel and comparing sender and receiver buffers

**Checkpoint**: Full tunnel scenario runs without modification to `ncx_server/` or `ncx_client/`. All callbacks fire correctly. US3 is independently verified.

---

## Phase 6: User Story 4 — Concurrent Connection Scalability (Priority: P4)

**Goal**: The server sustains 500 simultaneous TCP connections. All connections are cleaned up after close with no fd or memory leaks.

**Independent Test**: Open 500 simultaneous tunnelled connections, transfer data on each, close them all; verify fd count returns to baseline and `valgrind` reports no leaks.

### Implementation for User Story 4

- [X] T029 [US4] Review `app/network/server.cpp` connection pool (`unordered_map<int, shared_ptr<Connection>>`): confirm `disconnectHandleInLoop` removes the entry and all `Connection` resources are released when the shared_ptr ref-count drops to zero
- [X] T030 [US4] Review `app/network/connection.cpp` destructor: confirm `event_del` and `event_free` are called on `read_ev_` and `write_ev_` before `close(client_fd)` to prevent libevent from firing callbacks on a closed fd
- [ ] T031 [US4] Run fd-leak check: start `ncxs`, open and close 100 connections in a loop, compare `/proc/<pid>/fd` count before and after; confirm count returns to baseline
- [ ] T032 [US4] Run `valgrind --leak-check=full ./build/ncxs ncxs.toml` with a full connect-transfer-close cycle; confirm no heap leaks are reported after clean shutdown

**Checkpoint**: 500 connections sustained without exhaustion. All resources released on close. US4 is independently verified.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final cleanup, documentation, and cross-cutting verification after all user stories pass.

- [X] T033 [P] Update `app/README.md`: add `libevent-dev` to the prerequisites section
- [X] T034 [P] Update `CLAUDE.md` (project-root): add libevent to the "Dependencies" list in the Build Commands section
- [X] T035 Remove any residual `#include "epoll.h"` or `Epoll`-related forward declarations from all files under `app/` that were not already cleaned up in Phase 2
- [ ] T036 [P] Run `cmake .. && make` inside a clean `app/build/` directory to confirm there are no leftover references to deleted files or undefined symbols
- [ ] T037 Run the complete quickstart verification sequence from `specs/001-libevent-tcp/quickstart.md` (build → start server → connect client → end-to-end tunnel → clean shutdown) and confirm all steps pass

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 completion — **BLOCKS all user stories**
- **US1 (Phase 3)**: Depends on Phase 2 completion
- **US2 (Phase 4)**: Depends on Phase 3 completion (I/O correctness builds on connection establishment)
- **US3 (Phase 5)**: Depends on Phase 3 + Phase 4 completion (full tunnel requires connections and I/O)
- **US4 (Phase 6)**: Depends on Phase 3 completion (scalability builds on basic connection handling)
- **Polish (Phase 7)**: Depends on all user story phases being complete

### User Story Dependencies

- **US1 (P1)**: Can start after Phase 2 — no dependency on other user stories
- **US2 (P2)**: Builds on US1 (I/O callbacks depend on connections working)
- **US3 (P3)**: Builds on US1 + US2 (full tunnel needs both)
- **US4 (P4)**: Can run in parallel with US2 and US3 after US1 completes

### Within Each Phase

- `EventLoop` (T006–T007) must complete before `Channel` (T008–T009) since `Channel` calls `loop_->base()`
- `Acceptor` (T012–T013) and `Connector` (T014–T015) can be implemented in parallel (different files, both depend on `Channel`)
- `Connection` (T016–T017) can be implemented in parallel with `Acceptor` and `Connector`
- `evthread_use_pthreads()` additions (T018–T019) must precede any runtime test (T020+)

### Parallel Opportunities

- T001 (Dockerfile) and T002–T003 (CMakeLists) can run in parallel
- T005 (delete Epoll files) can run in parallel with T010–T011 (header cleanup)
- T008–T009 (Channel) can begin as soon as T006–T007 (EventLoop) are complete
- T012–T013 (Acceptor), T014–T015 (Connector), T016–T017 (Connection) can all be worked in parallel after Phase 2
- T018 and T019 (thread init in both entry points) can run in parallel
- T025–T028 (US3 verification) tasks can run in parallel once the build is clean
- T029–T032 (US4 scalability) can run in parallel with US2 and US3 after US1 completes
- T033–T036 (polish) can run in parallel with each other

---

## Parallel Example: Phase 2 (Foundational)

```text
# These can start in parallel immediately after T004 passes:
T005: Delete epoll.h / epoll.cpp
T010: Update util.h (remove Epoll forward decl)
T011: Update common.h (remove epoll.h include)

# After T005 completes:
T006: Rewrite EventLoop.h

# After T006 completes:
T007: Rewrite EventLoop.cpp
T008: Rewrite channel.h (uses loop_->base())

# After T008 completes:
T009: Rewrite channel.cpp
```

## Parallel Example: Phase 3 (US1)

```text
# After Phase 2 checkpoint passes, these three can start in parallel:
T012–T013: Rewrite Acceptor (acceptor.h + acceptor.cpp)
T014–T015: Rewrite Connector (connector.h + connector.cpp)
T016–T017: Update Connection (connection.h + connection.cpp)

# After all three complete:
T018: Add evthread_use_pthreads() to ncxs.cpp
T019: Add evthread_use_pthreads() to ncxc.cpp (parallel with T018)

# After T018 + T019:
T020: Build and smoke-test
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001–T004)
2. Complete Phase 2: Foundational (T005–T011) — **critical, blocks everything**
3. Complete Phase 3: US1 (T012–T020)
4. **STOP and VALIDATE**: control-channel handshake works end-to-end
5. Optionally demo / checkpoint the branch before continuing

### Incremental Delivery

1. Phase 1 + Phase 2 → libevent-backed EventLoop and Channel are live
2. Phase 3 (US1) → TCP connections establish; handshake works (MVP)
3. Phase 4 (US2) → I/O callbacks verified correct; no spin-loop
4. Phase 5 (US3) → Full tunnel scenario works; upper layers unmodified
5. Phase 6 (US4) → Scalability and leak-free cleanup confirmed
6. Phase 7 → Docs and final polish

### Solo Developer Strategy

Work phases sequentially in priority order:

1. Setup → Foundational → US1 → US2 → US3 → US4 → Polish
2. Commit after each phase checkpoint passes
3. Each checkpoint is a valid, demonstrable state of the feature

---

## Notes

- `[P]` tasks = different files, no blocking dependencies; can be dispatched in parallel
- `[USn]` label maps every implementation task to its user story for traceability
- The `Buffer` class (`app/network/buffer.h/cpp`) requires **zero changes** — it is a pure in-memory data structure with no epoll/libevent coupling
- The `server.h/cpp`, `client.h/cpp`, `evPoolThread.h/cpp`, and all files under `ncx_client/` and `ncx_server/` require **zero changes**
- The only files outside `app/network/` that change are `app/CMakeLists.txt`, `app/Dockerfile`, `app/ncx_server/ncxs.cpp` (one line), and `app/ncx_client/ncxc.cpp` (one line)
- Commit after each phase checkpoint to make the migration bisectable
