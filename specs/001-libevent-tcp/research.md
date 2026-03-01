# Research: libevent TCP Network Layer Integration

**Branch**: `001-libevent-tcp` | **Date**: 2026-03-01
**Phase**: 0 — All NEEDS CLARIFICATION items resolved

---

## Decision 1: Event-Loop Backend — `event_base` replaces `Epoll`

**Decision**: Replace the `Epoll` class (which wraps `epoll_create` / `epoll_ctl` / `epoll_wait`) with libevent's `event_base*`. The `EventLoop::run()` body becomes a single call to `event_base_loop(base, EVLOOP_NO_EXIT_ON_EMPTY)`.

**Rationale**: `EVLOOP_NO_EXIT_ON_EMPTY` preserves the current behaviour of the `while(true)` loop — the loop stays alive even before any connections arrive. `event_base_loopexit(base, NULL)` or `event_base_loopbreak(base)` replaces the `stop_` boolean flag.

**Alternatives considered**:
- Keeping raw `epoll_wait` and layering libevent only for timers — rejected because this defeats the purpose of using libevent's unified dispatch model.
- Using `EVLOOP_ONCE` flag — rejected because it requires being called in a loop, which reintroduces the manual scheduling logic we want to eliminate.

---

## Decision 2: Channel Replacement — raw `struct event *`

**Decision**: Replace `Channel` (which calls `epoll_ctl` via `Epoll`) with raw libevent `struct event *`. Each `Connection`/`Acceptor`/`Connector` owns its own `struct event *` members for read and write monitoring.

**Rationale**: This is the minimal-change path. It preserves the existing `Buffer` class and `ReadNonBlocking` / `WriteNonBlocking` loops in `Connection`. The upper-layer callback signatures (`on_conn_`, `on_message_`, `on_close_`) remain identical. The `Epoll` class is deleted entirely — libevent's `event_base` encapsulates the same functionality transparently.

**Key API mapping**:

| Current (`Epoll` / `Channel`) | libevent equivalent |
|-------------------------------|---------------------|
| `EPOLL_CTL_ADD` + `EPOLL_CTL_MOD` | `event_add(ev, NULL)` (idempotent) |
| `EPOLL_CTL_DEL` | `event_del(ev)` |
| `EPOLLIN` | `EV_READ` |
| `EPOLLOUT` | `EV_WRITE` |
| `EPOLLET` | `EV_ET` |
| Persistent monitoring | `EV_PERSIST` |
| `is_epolled_` flag | Not needed (`event_add` is idempotent) |
| `channel->handle_event()` | libevent invokes callback directly |

**Alternatives considered**:
- Using `bufferevent` for Connection I/O — a viable future upgrade that provides zero-copy bridge forwarding (`bufferevent_write_buffer`) and built-in backpressure. Deferred to a follow-on feature to minimize scope of this integration. The raw-event path is the safer first step.

---

## Decision 3: Cross-Thread Wakeup — `evthread_use_pthreads` replaces eventfd

**Decision**: Call `evthread_use_pthreads()` once at process startup (before any `event_base_new()`). This causes every subsequent `event_base` to be internally notifiable. The manual `wakeup_fd_` (eventfd), `wakeup_channel_`, and `wakeup_loop()` method are all deleted from `EventLoop`.

**Rationale**: After `evthread_use_pthreads()`, libevent automatically sets up a self-pipe inside each `event_base` and exposes `evthread_notify_base()` as an internal mechanism. `event_base_loopbreak()` becomes safe to call from any thread. No user-level fd management is needed.

**Replacement for `run_on_owner_thread(cb)`**: Use `event_base_once(base, -1, EV_TIMEOUT, shim_fn, ctx, &zero_tv)` where `zero_tv = {0, 0}`. This queues `cb` to fire in the owning thread on the next loop iteration. It is thread-safe and internally calls the libevent notify mechanism.

**Alternatives considered**:
- Keeping the eventfd + `wakeup_channel_` alongside libevent — rejected because this duplicates the wakeup infrastructure libevent already provides.

---

## Decision 4: Task Queue Elimination

**Decision**: Delete `to_do_list`, the `mtx` mutex, `add_to_do()`, and `do_after_handle_events()` from `EventLoop`. Replace every call-site of `run_on_owner_thread(cb)` with `event_base_once(base, -1, EV_TIMEOUT, shim, ctx, &zero_tv)`.

**Rationale**: `event_base_once` is thread-safe and fully replaces the queue-then-wake pattern. libevent manages the internal event struct lifetime automatically (freed after the callback fires). No per-task heap allocation by user code is needed.

**Shim pattern**: Since libevent callbacks have signature `void cb(evutil_socket_t, short, void*)` but `run_on_owner_thread` accepts `std::function<void()>`, a small heap-allocated wrapper is used:
```cpp
// Heap-allocate the lambda, free it inside the shim:
auto* fn = new std::function<void()>(std::move(cb));
event_base_once(base_, -1, EV_TIMEOUT,
    [](evutil_socket_t, short, void* arg) {
        auto* f = static_cast<std::function<void()>*>(arg);
        (*f)();
        delete f;
    }, fn, &zero_tv);
```

---

## Decision 5: Acceptor — `evconnlistener` replaces manual `accept()` + `Channel`

**Decision**: Replace `Acceptor`'s listening `Channel` and manual `TcpSocket::accept()` with `evconnlistener_new_bind`. The `set_new_connection_callback` hook is preserved at the `Server` level.

**Rationale**: `evconnlistener` handles `EPOLLIN` on the listening socket, calls `accept()` internally, and delivers the accepted fd to a callback. It also manages EAGAIN and large-backlog bursts correctly. The `Acceptor` class can be replaced with a thin wrapper around `evconnlistener`.

**Key API**:
```c
struct evconnlistener *evconnlistener_new_bind(
    struct event_base *base,
    evconnlistener_cb cb,        // void cb(evconnlistener*, evutil_socket_t, sockaddr*, int, void*)
    void *ptr,
    unsigned flags,              // LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE
    int backlog,
    const struct sockaddr *sa,
    int socklen
);
```

**Alternatives considered**:
- Keeping `Acceptor` and adapting its `Channel` to libevent events — possible but `evconnlistener` is a cleaner, purpose-built API for listening sockets.

---

## Decision 6: Connector — `bufferevent_socket_connect` replaces manual `connect()` + `Channel`

**Decision**: Replace `Connector`'s non-blocking `connect()` + `Channel` write-event monitoring with `bufferevent_socket_connect`. The success/failure callback is delivered via the `bufferevent` event callback (`BEV_EVENT_CONNECTED` / `BEV_EVENT_ERROR`).

**Rationale**: The current `Connector` manually calls `connect()`, registers a write-event `Channel` to detect completion, and reads `SO_ERROR` via `getsockopt`. `bufferevent_socket_connect` handles all of this internally, including the non-blocking connect handshake.

---

## Decision 7: Buffer Class — Keep Unchanged

**Decision**: The `Buffer` class (backed by `std::vector<char>`) is kept as-is. `Connection::ReadNonBlocking()` reads from the socket into `input_buffer` using raw `::read()`, and `Connection::WriteNonBlocking()` drains `output_buffer` using raw `::write()`. These syscalls are unchanged.

**Rationale**: Migrating `Buffer` to `evbuffer` is a deeper change that would affect the `on_message_` callback signature (`Buffer*` parameter used throughout upper layers). Deferring this keeps the upper-layer API stable and reduces the scope of this feature.

---

## Decision 8: CMake Integration

**Decision**: Add libevent using `find_package(Libevent CONFIG REQUIRED COMPONENTS core extra pthreads)` in `app/CMakeLists.txt`, with a fallback to `pkg_check_modules`. Link targets: `libevent::core`, `libevent::extra`, `libevent::pthreads`.

**Dockerfile change**: Add `RUN apt-get install -y libevent-dev` to the Ubuntu 22.04 base.

**Alternatives considered**:
- Direct linking (`target_link_libraries(... event event_pthreads)`) — works but is least portable; rejected as primary approach.
- FetchContent build from source — rejected; libevent is a mature system library well-packaged by `apt`.

---

## Decision 9: Thread Initialisation Order

**Decision**: Call `evthread_use_pthreads()` in `main()` of both `ncxs.cpp` and `ncxc.cpp`, before any `EventLoop` is constructed. This is a one-time global initialisation.

**Rationale**: libevent requires this to be called before the first `event_base_new()`. The existing `EvPoolThread` class creates `EventLoop` (and therefore `event_base`) in a background thread — so the call must happen in `main()` before `EvPoolThread::StartLoop()` is invoked.

---

## Resolved Unknowns

| Unknown | Resolution |
|---------|-----------|
| Which libevent abstraction level to target? | Raw `struct event *` (not `bufferevent`) for minimal upper-layer disruption |
| How to handle cross-thread task dispatch? | `event_base_once` + `evthread_use_pthreads` |
| How to handle the listening socket? | `evconnlistener_new_bind` |
| How to handle outbound connection establishment? | `bufferevent_socket_connect` inside `Connector` |
| CMake integration strategy? | `find_package(Libevent CONFIG)` with pkg-config fallback |
| What happens to `Epoll` class? | Deleted entirely; functionality absorbed by `event_base` |
| What happens to `Channel` class? | Rewritten to wrap `struct event *` instead of calling `epoll_ctl` |
| What happens to `wakeup_fd_` / `wakeup_channel_`? | Deleted; replaced by libevent's internal notification |
