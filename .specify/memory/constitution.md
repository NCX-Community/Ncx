<!--
  SYNC IMPACT REPORT
  ==================
  Version change: (none) → 1.0.0  (initial ratification)
  Modified principles: N/A — first version
  Added sections:
    - Core Principles (I–VI)
    - Platform & Stack Constraints
    - Development Workflow
    - Governance
  Removed sections: N/A
  Templates reviewed:
    - .specify/templates/plan-template.md    ✅ Constitution Check gate aligns
    - .specify/templates/spec-template.md   ✅ No mandatory section conflicts
    - .specify/templates/tasks-template.md  ✅ Task categories reflect principles
  Deferred items:
    - TODO(RATIFICATION_DATE): formal team sign-off date unknown; using project-start approximation 2026-03-01
-->

# ENGINE Constitution

## Core Principles

### I. Performance-First Design

Every subsystem MUST be designed for low latency and high throughput.
Specifically:

- The network engine MUST use a Reactor-pattern event loop (currently backed by libevent 2.x `event_base`).
- All I/O MUST be non-blocking; blocking calls inside an `EventLoop` thread are PROHIBITED.
- Cross-thread work MUST be dispatched via `run_on_owner_thread()` (or equivalent `event_base_once` delegation); direct mutex locking across threads on shared connection state is PROHIBITED.
- Buffer management MUST use a ring-buffer or equivalent zero-copy-friendly structure (`Buffer` class) to avoid unnecessary memory copies on the hot path.
- New features that introduce measurable latency regressions (>10% on baseline throughput benchmarks) MUST be justified in the plan's Complexity Tracking table before merging.

**Rationale**: Intranet penetration imposes an inherent RTT tax on every proxied byte.
Minimising framework overhead keeps Ncx competitive with frp and raw SSH tunnels.

### II. Protocol Correctness & Framing Integrity

All inter-process messages MUST use the canonical protobuf-based binary framing:

- Every message MUST be preceded by a serialised `MessageHeader` length prefix (5-byte framing as implemented in `PROTOMSGUTIL`).
- `CanReadMsg()` MUST be called before attempting to deserialise any message; partial reads MUST be buffered and retried — never partially parsed.
- Handshake ordering is MANDATORY and MUST NOT be skipped or reordered:
  1. Client → Server: `CONTROL_CHANNEL_HELLO` or `DATA_CHANNEL_HELLO` with nonce.
  2. Server → Client: `Ack`.
  3. Any command traffic only after successful ack.
- Protocol changes (new message types, field additions) MUST remain backward-compatible or increment `protocol.proto` with a new oneof/field number; existing field numbers MUST NOT be reused.
- Protobuf-generated files (`protocol.pb.cc`, `protocol.pb.h`) MUST be regenerated via `protoc` whenever `.proto` changes and committed alongside the `.proto` diff.

**Rationale**: A tunnelling tool's correctness guarantee is only as strong as its wire protocol.
Partial reads or out-of-order handshakes silently corrupt tunnelled streams.

### III. Connection Lifecycle Integrity

Every TCP connection MUST have a clearly owned lifecycle from accept/connect to close:

- `Connection` objects MUST be managed via `shared_ptr` and MUST inherit `enable_shared_from_this`.
  Raw owning pointers to `Connection` are PROHIBITED.
- Every bridged pair (visitor ↔ data-channel) MUST be managed by a `BridgeCloser` instance.
  When one side closes, the `BridgeCloser` MUST close the other side synchronously within the same event-loop iteration.
- Connection pools (`unordered_map<int, shared_ptr<Connection>>`) MUST remove entries on close; leaked entries constitute a resource-management bug.
- Callbacks (`on_conn_`, `on_message_`, `on_close_`) MUST be set before a connection becomes active; setting them after the first read event is a race condition.
- `DISALLOW_COPY_AND_MOVE` MUST be applied to all non-trivial network classes (`EventLoop`, `Channel`, `Connection`, `Server`, `Client`, `Connector`, `Acceptor`, `EvPoolThread`).

**Rationale**: Half-closed tunnels silently drop bytes. Ownership ambiguity causes use-after-free in async contexts.

### IV. Security by Design

Authentication and channel isolation MUST be enforced at the protocol layer:

- Every data channel connection MUST present the nonce received in `ControlChannelCmd` inside `DATA_CHANNEL_HELLO`.
  Server MUST reject data channels with missing or mismatched nonces.
- Control channels and data channels MUST be handled by separate code paths (`SControlChannel` vs data-channel bridging); mixing them is PROHIBITED.
- No plaintext credentials or service metadata MUST be logged at INFO level or below in production builds.
- Future authentication mechanisms (e.g., TLS, pre-shared keys) MUST be addable without breaking the existing `Hello`/`Ack` handshake structure.

**Rationale**: Without nonce verification, any actor who knows the server address can hijack a visitor slot.

### V. Minimal Footprint

Ncx MUST remain lightweight and dependency-lean:

- The compiled binary size for `ncxs` and `ncxc` MUST stay below 2 MB (stripped release build).
- Runtime dependencies are limited to: `libevent` (core, extra, pthreads), `libprotobuf`, `libboost_program_options`.
  Adding a new runtime dependency requires explicit justification in the feature spec's Requirements section.
- `tomlplusplus` is header-only and fetched at configure time via `FetchContent` — this pattern MUST be used for any future header-only library additions.
- Features MUST NOT add unnecessary abstraction layers. YAGNI applies: implement what is needed now, not what might be needed later.
- Avoid C++ templates beyond CRTP patterns already in use (e.g., `Singleton<T>`); complex template metaprogramming increases compile times and binary size.

**Rationale**: Ncx targets embedded devices (routers, low-spec cloud VMs). Bloat defeats the purpose.

### VI. Linux-Native Targeting

Ncx targets Linux exclusively:

- All network code MUST use Linux-native APIs: `epoll` (via libevent), `eventfd`, POSIX `sys/socket.h`, and `pthreads`.
- Code MUST NOT introduce `#ifdef _WIN32` or macOS compatibility shims — cross-platform portability is explicitly out of scope.
- The canonical build environment is Ubuntu 22.04 LTS (as reflected in the Dockerfile).
  CI MUST validate on this environment.
- New kernel features (e.g., `io_uring`) MAY be explored but MUST be feature-gated and MUST NOT break the libevent baseline.
- `util.h` remains the single common include for STL headers and forward declarations; all new shared forward declarations MUST be added there.

**Rationale**: Linux-native APIs deliver optimal performance without portability tax. Scope control keeps the codebase maintainable.

## Platform & Stack Constraints

| Concern            | Decision                                        |
|--------------------|-------------------------------------------------|
| Language           | C++20 (GCC with `-std=c++20`)                   |
| Network engine     | libevent 2.x (event_base, evconnlistener, bufferevent) |
| Serialisation      | Protocol Buffers 3 (proto3 syntax)              |
| Config format      | TOML via `tomlplusplus` (header-only, FetchContent) |
| CLI parsing        | `boost::program_options`                        |
| Build system       | CMake 3.10+, single `CMakeLists.txt` at `app/` |
| Target OS          | Linux (Ubuntu 22.04 LTS reference)              |
| Container          | Docker (Dockerfile at `app/`)                   |
| Binary targets     | `ncxs` (server), `ncxc` (client)               |
| Testing            | Manual smoke tests + quickstart validation (automated tests: future work) |
| Protocol transport | TCP only (UDP, HTTP: planned future work)       |

Any deviation from the decisions above MUST be documented as a Complexity Tracking entry in the relevant feature plan.

## Development Workflow

### Feature Lifecycle

1. **Specify** — Create `specs/<###-feature-name>/spec.md` via `/speckit.specify`.
2. **Plan** — Generate `specs/<###-feature-name>/plan.md` via `/speckit.plan`.
   - The "Constitution Check" gate in `plan.md` MUST explicitly verify compliance with all six principles.
3. **Task** — Generate `specs/<###-feature-name>/tasks.md` via `/speckit.tasks`.
4. **Implement** — Execute tasks via `/speckit.implement`.
5. **Validate** — Run `quickstart.md` steps manually; confirm no regressions.
6. **Merge** — PR merged only after validation passes.

### Coding Standards

- Every non-trivial class that manages resources MUST use RAII; raw `new`/`delete` outside of low-level wrappers is PROHIBITED.
- All new files MUST include the standard project headers via `util.h`; add new forward declarations there rather than in individual headers.
- Commit messages MUST follow Conventional Commits format: `type(scope): description`.
- Regenerated protobuf files MUST be committed in the same commit as the `.proto` change.

### Quality Gates

- Build MUST succeed with `cmake .. && make` in the Dockerfile environment before any PR is merged.
- The `quickstart.md` smoke test (connect client to server, verify traffic flows) MUST pass.
- No new compiler warnings (treat warnings as errors policy: `-Wall -Wextra` recommended).

## Governance

This Constitution supersedes all informal design decisions and undocumented conventions.

- **Amendment procedure**: Propose changes as a PR that modifies this file. The PR description MUST include:
  - Which principle(s) are affected and why.
  - A migration plan if existing code violates the new rule.
  - Version bump rationale (MAJOR / MINOR / PATCH).
- **Versioning policy**:
  - MAJOR bump: removal or redefinition of a Core Principle.
  - MINOR bump: new principle, new section, or materially expanded guidance.
  - PATCH bump: clarifications, wording, typo fixes.
- **Compliance review**: Every feature plan's "Constitution Check" section serves as the per-feature compliance record.
  Violations not recorded there are unreviewed violations.
- **Runtime guidance**: See `app/README.md` for operational quickstart; `CLAUDE.md` for AI-assisted development context.

**Version**: 1.0.0 | **Ratified**: 2026-03-01 | **Last Amended**: 2026-03-01
