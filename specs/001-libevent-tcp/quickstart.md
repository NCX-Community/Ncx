# Quickstart: libevent TCP Network Layer Integration

**Branch**: `001-libevent-tcp` | **Date**: 2026-03-01

This guide describes how to build, run, and verify the ENGINE after integrating libevent into the network layer.

---

## Prerequisites

### On the host (Ubuntu 22.04)

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake build-essential g++ \
    libprotobuf-dev protobuf-compiler \
    libboost-program-options-dev \
    libevent-dev
```

### Via Docker

The `Dockerfile` has been updated to include `libevent-dev`. Simply run:

```bash
docker build -t ncxs .
```

---

## Build

```bash
cd app
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

This produces two binaries: `build/ncxs` (server) and `build/ncxc` (client).

---

## Run

**Server:**
```bash
./build/ncxs ncxs.toml
```

**Client:**
```bash
./build/ncxc ncxc.toml
```

The server and client configurations (`ncxs.toml`, `ncxc.toml`) are unchanged from the pre-libevent version.

---

## Verify the Integration

### Smoke test: control-channel handshake

1. Start `ncxs` on a machine with a public IP (or locally with `127.0.0.1`).
2. Start `ncxc` pointing at the server address.
3. Confirm in the server log that a `CONTROL_CHANNEL_HELLO` was received and acknowledged.
4. The tunnel is now active.

### End-to-end test: visitor traffic

1. With the tunnel active, connect a visitor to the proxy port configured in `ncxs.toml`.
2. Data should flow bidirectionally through the tunnel to the local service.
3. Close the visitor connection and verify that the `BridgeCloser` callback fires and the data channel is torn down cleanly.

### Resource check (no memory/fd leak)

After closing all connections, verify:
- `cat /proc/<pid>/fd | wc -l` returns to the baseline fd count.
- `valgrind --leak-check=full ./build/ncxs ncxs.toml` reports no heap leaks after a clean shutdown.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `cmake` fails with "Libevent not found" | `libevent-dev` not installed | `apt-get install libevent-dev` |
| Linker error: `undefined reference to evthread_use_pthreads` | Missing `libevent_pthreads` | Ensure `libevent::pthreads` is linked in `CMakeLists.txt` |
| Server exits immediately with no connections | `EVLOOP_NO_EXIT_ON_EMPTY` flag missing | Verify `EventLoop::run()` passes this flag to `event_base_loop` |
| Cross-thread crash / assertion in libevent | `evthread_use_pthreads()` not called before `event_base_new()` | Call it first in `main()` of both `ncxs.cpp` and `ncxc.cpp` |
