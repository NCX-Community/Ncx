#ifndef GRPC_ENGINE_CLIENT_H
#define GRPC_ENGINE_CLIENT_H

#include "util.h"
#include <grpcpp/grpcpp.h>
#include "engine_cluster_stream.grpc.pb.h"

class NServer;

/// GrpcEngineClient — runs on two dedicated std::threads (read + write) completely
/// separate from the libevent EventLoop, satisfying Constitution Principle I.
///
/// Lifecycle:
///   1. start()  → spawns write_thread_ (owns reconnect loop + registration + heartbeat)
///                 and read_thread_   (blocks on stream->Read(), dispatches messages)
///   2. stop()   → sets running_=false, cancels gRPC context, joins both threads
class GrpcEngineClient {
public:
    DISALLOW_COPY_AND_MOVE(GrpcEngineClient);

    struct Config {
        std::string proxy_endpoint;       // "host:port" of engine proxy gRPC server
        std::string engine_id;            // unique engine identifier
        std::string engine_address;       // "ip:port" reported in EngineRegistration
        int32_t     max_capacity;         // declared max concurrent proxy bridges
        int         heartbeat_interval_sec = 10;
    };

    GrpcEngineClient(Config cfg, NServer* nserver);
    ~GrpcEngineClient();

    /// Spawn read_thread_ and write_thread_. Must be called exactly once.
    void start();

    /// Signal both threads to stop, cancel the active gRPC context, and join threads.
    void stop();

private:
    using Stream = grpc::ClientReaderWriter<
        enginecluster::v1::EngineMessage,
        enginecluster::v1::ProxyMessage>;

    /// Outer reconnect loop: create stream → send EngineRegistration → heartbeat loop.
    /// Runs on write_thread_.
    void run_write();

    /// Wait for a live stream, then block on stream->Read() dispatching messages.
    /// Runs on read_thread_.
    void run_read();

    /// Respond to a CapacityRequest: read snapshot, write CapacityReport to stream.
    void handle_capacity_request(Stream* stream, const std::string& request_id);

    Config cfg_;
    NServer* nserver_;  // non-owning; NServer outlives GrpcEngineClient

    std::shared_ptr<grpc::Channel> channel_;

    // Shared stream pointer: written by write_thread_, read by read_thread_ under stream_mu_.
    std::shared_ptr<Stream> stream_;
    std::mutex              stream_mu_;
    std::condition_variable stream_cv_;

    // Active gRPC context; recreated per connection attempt; cancelled in stop().
    std::unique_ptr<grpc::ClientContext> ctx_;
    std::mutex                           ctx_mu_;

    // Condition variable used to interrupt the heartbeat sleep on stop().
    std::mutex              hb_mu_;
    std::condition_variable hb_cv_;

    std::thread write_thread_;
    std::thread read_thread_;
    std::atomic<bool> running_{false};
};

#endif // GRPC_ENGINE_CLIENT_H
