# Quickstart: Engine Cluster gRPC Client

**Feature**: 001-grpc-engine-cluster
**Branch**: `001-grpc-engine-cluster`

---

## Prerequisites

### Install gRPC C++ (Ubuntu 22.04)

```bash
# Install gRPC and protobuf development packages
sudo apt-get update
sudo apt-get install -y libgrpc++-dev libprotobuf-dev protobuf-compiler \
  protobuf-compiler-grpc grpc-proto
```

Alternatively, build gRPC from source (if package version is too old):

```bash
# Follow official gRPC C++ installation guide for Linux
# https://grpc.io/docs/languages/cpp/quickstart/
```

### Verify tools

```bash
protoc --version          # must be 3.x+
grpc_cpp_plugin --version # or: which grpc_cpp_plugin
```

---

## Step 1: Copy Proto and Regenerate

Copy the canonical contract into the protocol directory, then generate:

```bash
cd /path/to/ENGINE

cp specs/001-grpc-engine-cluster/contracts/engine-cluster-stream.proto app/protocol/

protoc \
  --cpp_out=app/protocol \
  --grpc_out=app/protocol \
  --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) \
  -I=app/protocol \
  app/protocol/engine-cluster-stream.proto
```

Commit the four generated files alongside the `.proto` copy:
- `app/protocol/engine_cluster_stream.pb.h`
- `app/protocol/engine_cluster_stream.pb.cc`
- `app/protocol/engine_cluster_stream.grpc.pb.h`
- `app/protocol/engine_cluster_stream.grpc.pb.cc`

---

## Step 2: Build

```bash
mkdir -p build && cd build
cmake ..
make ncxs
```

Expected: `build/ncxs` compiles without errors or warnings.

---

## Step 3: Configure `ncxs.toml`

Add the `[engine_proxy]` section to your server config:

```toml
[server]
server_ip   = "0.0.0.0"
server_port = 9000
backlog     = 256

[engine_proxy]
proxy_endpoint     = "127.0.0.1:50051"   # gRPC address of the engine proxy
engine_id          = "engine-01"          # Unique engine identifier
engine_address     = "203.0.113.5:9001"  # This engine's public address
max_capacity       = 100                  # Max concurrent proxy bridges
heartbeat_interval = 10                  # Heartbeat frequency in seconds
```

To run without cluster management (standalone mode), omit the `[engine_proxy]` section entirely.

---

## Step 4: Run the Engine Proxy Stub (for testing)

You need a gRPC server listening on the configured `proxy_endpoint`. For smoke testing, a minimal Go stub can be used:

```bash
# Run a stub gRPC server (e.g., grpcurl in server mode, or a minimal Go test server)
# The stub should implement EngineClusterService.Connect and:
#   1. Log received EngineMessage (status / heartbeats)
#   2. Optionally send a PortQueryRequest after a few seconds
```

---

## Step 5: Run the NCX Engine

```bash
./build/ncxs ncxs.toml
```

**Expected log output** (on successful connection):
```
[GrpcEngineClient] Connecting to engine proxy at 127.0.0.1:50051
[GrpcEngineClient] Stream open. Sending registration: address=203.0.113.5:9001 occupied=0
[GrpcEngineClient] Heartbeat sent. occupied=0
[GrpcEngineClient] Heartbeat sent. occupied=0
...
```

**If engine proxy is unavailable** (expected behavior):
```
[GrpcEngineClient] Stream error: connection refused. Reconnecting in 5s...
```

---

## Smoke Test Checklist

- [ ] `ncxs` builds without errors after adding gRPC to CMakeLists.txt
- [ ] Engine sends initial `EngineStatus` on stream open (visible in proxy logs)
- [ ] Engine sends periodic heartbeats every `heartbeat_interval` seconds
- [ ] Engine responds to `PortQueryRequest` with the list of active proxy port addresses
- [ ] Engine reconnects automatically when the proxy goes down and comes back up
- [ ] Omitting `[engine_proxy]` from config starts `ncxs` in standalone mode without errors
- [ ] Existing NCX tunnel traffic (control channel + data channel) is unaffected by the gRPC client thread
