#pragma once
#include <string>
#include <cstdint>

/// Configuration for the GrpcEngineClient, parsed from the [engine_proxy] section
/// of ncxs.toml. Defined in its own header to avoid circular dependencies between
/// config.h (which includes nserver.h for ServerArgs) and nserver.h.
struct EngineProxyConfig {
    std::string proxy_endpoint;   // "host:port" of engine proxy gRPC server
    std::string engine_id;        // unique engine identifier
    std::string engine_address;   // "ip:port" reported in EngineRegistration
    int32_t     max_capacity;     // max concurrent proxy bridges
    int         heartbeat_interval = 10; // seconds between EngineHeartbeat messages
};
